// nxrs - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "buildmanager.h"
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTimer>
#ifdef Q_OS_LINUX
#include <signal.h>
#endif

BuildManager::BuildManager(QObject* parent) : QObject(parent) {
    process.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_LINUX
    process.setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession);
#endif
    connect(&process, &QProcess::readyReadStandardOutput, this, &BuildManager::readOutput);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) complete(false, process.errorString());
    });
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        readOutput();
        if (!pending.isEmpty()) { emit output(QString::fromUtf8(pending)); pending.clear(); }
        if (cancelled) { complete(false, "Build cancelled. Saves were not deleted. Rebuild before playing again."); return; }
        if (status != QProcess::NormalExit || code != 0) {
            complete(false, QString("Build failed (exit %1). See the build log for details.").arg(code)); return;
        }
        game.builtSource = game.source;
        game.builtUpdate = game.updateToken();
        game.buildIncomplete = false;
        if (!game.hasBuild()) { complete(false, "The recompiler exited successfully but the expected build files are missing."); return; }
        QString error;
        // Window::startBuild resolved the inherited controller layout into this copy
        if (!writeGameSettings(game, game.buttonLayout, &error)) { complete(false, "Build completed, but settings could not be saved: " + error); return; }
        complete(true, "Build complete. Your game is ready to play.");
    });
}
QStringList BuildManager::arguments(const Game& game, const Toolchain& tools, int jobs) {
    QStringList arguments{"-u", recompCli(tools.root), game.source,
        "--outdir", game.buildDir, "-o", game.outputName, "--target", nativeTarget, "--backend", nativeBackend,
        "--cc", tools.compiler, "--jobs", QString::number(qBound(1, game.workers ? game.workers : jobs, 64)),
        "-O", QString::number(qBound(0, game.optimization, 3)), "--runtime", tools.runtime};
    // the update to recompile and to read in place over the game's data, or the game without any
    const QString update = game.updateToken();
    if (update == "none") arguments << "--no-update";
    else if (!update.isEmpty()) arguments << "--update" << QDir::toNativeSeparators(update);
    return arguments;
}
bool BuildManager::start(const Game& next, const Toolchain& tools, int jobs, QString* error) {
    if (active) { *error = "Another build is already running."; return false; }
    if (!tools.canBuild()) { *error = tools.problem(); return false; }
    if (next.source.endsWith(".nsp", Qt::CaseInsensitive) && (tools.keys.isEmpty() || !QFileInfo(tools.keys).isFile())) {
        *error = "NSP builds need prod.keys. Use Keys > Install keys first."; return false;
    }
    if (!validSource(next.source, error)) return false;
    if (const auto* update = next.activeUpdate()) {
        if (!QFileInfo(update->path).isFile()) { *error = "The update " + QFileInfo(update->path).fileName() + " is missing. Put the file back, or remove it in Game > Install Updates."; return false; }
        if (tools.keys.isEmpty() || !QFileInfo(tools.keys).isFile()) { *error = "Applying an update needs prod.keys. Use Keys > Install keys first."; return false; }
    }
    if (next.buildDir.isEmpty() || !QDir().mkpath(next.buildDir)) { *error = "Cannot create the build output folder."; return false; }
    game = next; cancelled = false; holdsDlc = false; pending.clear();
    log.setFileName(QDir(game.buildDir).filePath(game.outputName + ".build.log"));
    if (!log.open(QIODevice::WriteOnly | QIODevice::Truncate)) { *error = log.errorString(); return false; }
    log.write(("SwitchRecomp build · " + QDateTime::currentDateTime().toString(Qt::ISODate) + "\n").toUtf8());
    auto env = tools.environment();
    env.insert("PYTHONUTF8", "1"); env.insert("PYTHONUNBUFFERED", "1");
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(tools.root);
    active = true;
    emit progress("Starting recompiler", 0, 0);
    process.start(tools.python, arguments(game, tools, jobs));
    return true;
}
void BuildManager::readOutput() {
    const QByteArray data = process.readAllStandardOutput();
    if (log.isOpen()) { log.write(data); log.flush(); }
    pending += data;
    int pos;
    while ((pos = pending.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(pending.left(pos)).trimmed();
        pending.remove(0, pos + 1);
        emit output(line); parseLine(line);
    }
}
void BuildManager::parseLine(const QString& line) {
    static const QRegularExpression counts("(?:C generated:|compiled)\\s+(\\d+)/(\\d+)");
    const auto match = counts.match(line);
    if (match.hasMatch()) {
        emit progress(line.contains("C generated") ? "Generating native C" : "Compiling native code", match.captured(1).toInt(), match.captured(2).toInt());
    } else if (line.startsWith("Analyzing")) emit progress("Analyzing ARM64 code", 0, 0);
    else if (line.startsWith("Linking")) emit progress("Linking native module", 0, 0);
    else if (line.startsWith("RomFS:")) emit progress("Preparing game data", 0, 0);
    else if (reportsPackageDlc(line)) holdsDlc = true;
}
void BuildManager::complete(bool success, const QString& detail) {
    if (!active) return;
    active = false;
    if (log.isOpen()) { log.write((detail + '\n').toUtf8()); log.close(); }
    emit finished(game.id, success, detail);
}
void BuildManager::cancel() {
    if (!active || cancelled) return;
    cancelled = true;
    emit progress("Stopping build processes", 0, 0);
#ifdef Q_OS_WIN
    const qint64 pid = process.processId();
    if (pid > 0) {
        auto* kill = new QProcess(this);
        connect(kill, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), kill, &QObject::deleteLater);
        kill->start("taskkill.exe", {"/PID", QString::number(pid), "/T", "/F"});
    }
#elif defined(Q_OS_LINUX)
    const auto pid = process.processId();
    if (pid > 0) ::kill(-pid, SIGTERM);
    QTimer::singleShot(4000, this, [this, pid] { if (cancelled && active && process.processId() == pid) ::kill(-pid, SIGKILL); });
#else
    process.terminate();
#endif
    QTimer::singleShot(5000, this, [this] { if (cancelled && active) process.kill(); });
}

UpdateProbe::UpdateProbe(QObject* parent) : QObject(parent) {
    process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&process, &QProcess::readyReadStandardOutput, this, &UpdateProbe::readOutput);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { errors << process.errorString(); complete(); }
    });
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        readOutput();
        if (!pending.isEmpty()) { pending += '\n'; readOutput(); }
        if ((status != QProcess::NormalExit || code != 0) && errors.isEmpty())
            errors << QString("The engine stopped (exit %1). See the console for details.").arg(code);
        complete();
    });
}
bool UpdateProbe::start(const Game& next, const Toolchain& tools, const QStringList& packages, QString* error) {
    if (active) { *error = "Updates are being checked already."; return false; }
    if (tools.root.isEmpty() || tools.python.isEmpty()) { *error = tools.problem().isEmpty() ? "Python is missing." : tools.problem(); return false; }
    if (tools.keys.isEmpty() || !QFileInfo(tools.keys).isFile()) { *error = "Installing an update needs prod.keys. Use Keys > Install keys first."; return false; }
    if (!next.sourceAvailable()) { *error = "The game's own package is missing; it is needed to check that the update belongs to it."; return false; }
    game = next; pending.clear(); errors.clear(); found.clear();
    auto env = tools.environment();
    env.insert("PYTHONUTF8", "1"); env.insert("PYTHONUNBUFFERED", "1");
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(tools.root);
    active = true;
    process.start(tools.python, updateInfoArguments(game, tools.root, packages));
    return true;
}
void UpdateProbe::readOutput() {
    pending += process.readAllStandardOutput();
    int pos;
    while ((pos = pending.indexOf('\n')) >= 0) {
        QString line = QString::fromUtf8(pending.left(pos));
        pending.remove(0, pos + 1);
        while (line.endsWith('\r') || line.endsWith('\n')) line.chop(1);
        if (line.trimmed().isEmpty()) continue;
        GameUpdate update; QString error;
        if (parseUpdateInfo(line, &update, &error)) found.append(update);
        else if (!error.isEmpty()) errors << error;
        emit output(line.trimmed());
    }
}
void UpdateProbe::complete() {
    if (!active) return;
    active = false;
    emit finished(game.id, found, errors);
}

DlcInstaller::DlcInstaller(QObject* parent) : QObject(parent) {
    process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&process, &QProcess::readyReadStandardOutput, this, &DlcInstaller::readOutput);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { errors << process.errorString(); complete(false); }
    });
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        readOutput();
        if (!pending.isEmpty()) { pending += '\n'; readOutput(); }
        const bool okay = status == QProcess::NormalExit && code == 0 && errors.isEmpty();
        if (!okay && errors.isEmpty()) errors << QString("The engine stopped (exit %1). See the console for details.").arg(code);
        complete(okay);
    });
}
bool DlcInstaller::start(const Game& next, const Toolchain& tools, QString* error) {
    if (active) { *error = "Another DLC installation is already running."; return false; }
    if (tools.root.isEmpty() || tools.python.isEmpty()) { *error = tools.problem().isEmpty() ? "Python is missing." : tools.problem(); return false; }
    if (!QDir(next.dataDir()).exists()) { *error = "Build the game first: DLC is installed into its data folder."; return false; }
    if (!next.dlc.isEmpty() && (tools.keys.isEmpty() || !QFileInfo(tools.keys).isFile())) {
        *error = "Installing DLC needs prod.keys. Use Keys > Install keys first."; return false;
    }
    game = next; pending.clear(); errors.clear();
    auto env = tools.environment();
    env.insert("PYTHONUTF8", "1"); env.insert("PYTHONUNBUFFERED", "1");
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(tools.root);
    active = true;
    process.start(tools.python, dlcSyncArguments(game, tools.root));
    return true;
}
void DlcInstaller::readOutput() {
    pending += process.readAllStandardOutput();
    int pos;
    while ((pos = pending.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(pending.left(pos)).trimmed();
        pending.remove(0, pos + 1);
        if (line.isEmpty()) continue;
        if (line.startsWith("DLC error: ")) errors << line.mid(11);
        emit output(line);
    }
}
void DlcInstaller::complete(bool success) {
    if (!active) return;
    active = false;
    emit finished(game.id, success, errors);
}
