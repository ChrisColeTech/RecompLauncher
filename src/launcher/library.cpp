// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "library.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace {
QString absolute(const QString& path) { return path.isEmpty() ? QString{} : QFileInfo(path).absoluteFilePath(); }
QString tool(const QString& root, const QStringList& choices, const QString& fallback = {}) {
    for (const auto& candidate : choices) {
        const QString path = QDir(root).filePath(candidate);
        if (!root.isEmpty() && QFileInfo(path).isFile()) return absolute(path);
    }
    return fallback.isEmpty() ? QString{} : QStandardPaths::findExecutable(fallback);
}
bool fail(QString* error, const QString& message) { if (error) *error = message; return false; }
QString pythonTool(const QStringList& roots, const QStringList& choices, const QString& fallback) {
    const auto runnable=[](const QString& path) {
        if(path.isEmpty())return false;
        QProcess process;process.start(path,{"-c","import sys; sys.exit(sys.version_info < (3, 8))"});
        if(!process.waitForFinished(5000)){process.kill();process.waitForFinished();return false;}
        return process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0;
    };
    const QString override=qEnvironmentVariable("SWR_PYTHON");
    if(!override.isEmpty()) {
        const QString path=QStandardPaths::findExecutable(override);
        return runnable(path)?path:QString{};
    }
    for(const auto& choice:choices)for(const auto& root:roots){const auto path=tool(root,{choice});if(runnable(path))return path;}
    const auto path=QStandardPaths::findExecutable(fallback);return runnable(path)?path:QString{};
}
}

// Two layouts, checked in this order:
//  release:  <release>/RecompLauncher(.exe), <release>/engine (switchrecomp.py, recomp_cli.py) and the shared tools in
//            <release>/toolchain (python, gcc, dotnet, shader-tools); Linux keeps the bundled shared libraries in <release>/lib
//  checkout: <project>/third_party/Switch2RECOMP with .venv, third_party/gcc, bin/launcher at <project>
// Anything else (an engine folder chosen in the settings) is searched on its own.
bool isReleaseEngine(const QString& root) {
    return !root.isEmpty() && QFileInfo(QDir(absolute(root)).filePath("../toolchain")).isDir();
}

QStringList switchrSearchRoots(const QString& root) {
    if (root.isEmpty()) return {};
    const QString engine = absolute(root);
    const QString parent = QDir::cleanPath(engine + "/..");
    if (isReleaseEngine(engine)) return {engine, parent + "/toolchain", parent};
    QStringList roots{engine};
    if (QFileInfo(parent).fileName().compare("third_party", Qt::CaseInsensitive) == 0)
        roots << QDir::cleanPath(parent + "/..");
    return roots;
}

QString toolkitProjectRoot(const QString& root) {
    const QStringList roots = switchrSearchRoots(root);
    return roots.isEmpty() ? QString{} : roots.last();
}

QString recompCli(const QString& root) {
    const QString beside = QDir(absolute(root)).filePath("recomp_cli.py");   // release: shipped inside engine/
    if (QFileInfo(beside).isFile()) return beside;
    return QDir(toolkitProjectRoot(root)).filePath("src/recomp_cli.py");    // checkout
}

QString Game::executable() const { return QDir(buildDir).filePath(outputName + executableSuffix); }
QString Game::codeModule() const { return QDir(buildDir).filePath(outputName + moduleSuffix); }
QString Game::dataDir() const { return QDir(buildDir).filePath(outputName + "_data"); }
QString Game::iniPath() const { return QDir(buildDir).filePath(outputName + ".ini"); }
QString Game::format() const {
    if (source.endsWith(".nup", Qt::CaseInsensitive)) return "DIRECT NUP";
    if (source.endsWith(".nsp", Qt::CaseInsensitive)) return "DIRECT NSP";
    if (source.endsWith(".nro", Qt::CaseInsensitive)) return "HOMEBREW";
    return source.isEmpty() ? "NATIVE BUILD" : "EXTRACTED";
}
bool Game::hasBuild() const {
    return !buildIncomplete && !buildDir.isEmpty() && !outputName.isEmpty() && QFileInfo(executable()).isFile()
        && QFileInfo(codeModule()).isFile() && QDir(dataDir()).exists()
        && (source.isEmpty() || samePath(builtSource, source));
}
bool Game::sourceAvailable() const { return !source.isEmpty() && QFileInfo::exists(source); }
QJsonObject Game::json() const {
    return {{"id", id}, {"title", title}, {"source", source}, {"buildDir", buildDir},
        {"outputName", outputName}, {"cover", cover}, {"background", background}, {"renderer", renderer}, {"buttonLayout", buttonLayout},
        {"workers", workers}, {"optimization", optimization},
        {"builtSource", builtSource}, {"buildIncomplete", buildIncomplete}, {"lastPlayed", lastPlayed.toString(Qt::ISODate)},
        {"lastBuilt", lastBuilt.toString(Qt::ISODate)}};
}
Game Game::fromJson(const QJsonObject& o) {
    Game g;
    g.id = o["id"].toString(); g.title = o["title"].toString(); g.source = o["source"].toString();
    g.buildDir = o["buildDir"].toString(); g.outputName = o["outputName"].toString();
    g.cover = o["cover"].toString(); g.renderer = o["renderer"].toString("vulkan");
    g.background = o["background"].toString();
    g.workers = qBound(0, o["workers"].toInt(), 64);
    g.optimization = qBound(0, o["optimization"].toInt(2), 3);
    if (g.renderer != "vulkan" && g.renderer != "opengl") g.renderer = "opengl";
    g.buttonLayout = normalizeButtonLayout(o["buttonLayout"].toString());
    g.builtSource = o["builtSource"].toString();
    g.buildIncomplete = o["buildIncomplete"].toBool();
    g.lastPlayed = QDateTime::fromString(o["lastPlayed"].toString(), Qt::ISODate);
    g.lastBuilt = QDateTime::fromString(o["lastBuilt"].toString(), Qt::ISODate);
    return g;
}

bool Toolchain::canBuild() const { return problem().isEmpty(); }
QString Toolchain::problem() const {
    if (root.isEmpty()) return "The engine was not found. It ships in the engine folder beside RecompLauncher (third_party/Switch2RECOMP in a source checkout); set it in File > Settings > Engine if you moved it.";
    if (!QFileInfo(QDir(root).filePath("switchrecomp.py")).isFile()) return "The engine folder is incomplete: switchrecomp.py is missing. Extract the release again or set it in File > Settings > Engine.";
    if (python.isEmpty()) return "Python is missing or SWR_PYTHON is invalid. Check SWR_PYTHON, .venv/venv, or the portable Python bundle.";
    if (compiler.isEmpty()) return "GCC is missing. Extract the complete engine bundle or run the setup script.";
    if (runtime.isEmpty()) return "The native runtime is missing. Extract the complete engine bundle or build the runtime first.";
    return {};
}
Toolchain Toolchain::find(const QString& root) {
    Toolchain t;
    t.root = absolute(root);
    const QStringList roots = switchrSearchRoots(t.root);
#ifdef Q_OS_WIN
    t.python = pythonTool(roots, {".venv/Scripts/python.exe", "venv/Scripts/python.exe", "python/python.exe"}, "python.exe");
    for (const auto& search : roots) {
        if (t.compiler.isEmpty()) t.compiler = tool(search, {"gcc/Library/bin/x86_64-w64-mingw32-gcc.exe", "third_party/gcc/Library/bin/x86_64-w64-mingw32-gcc.exe", "gcc/bin/gcc.exe", "third_party/gcc/bin/gcc.exe", "w64devkit/bin/gcc.exe"});
        if (t.runtime.isEmpty()) t.runtime = tool(search, {"bin/runtime_win64_win32.exe"});
        if (t.dotnet.isEmpty()) t.dotnet = tool(search, {"dotnet/dotnet.exe"});
    }
    if (t.compiler.isEmpty()) t.compiler = QStandardPaths::findExecutable("gcc.exe");
    if (t.dotnet.isEmpty()) t.dotnet = QStandardPaths::findExecutable("dotnet.exe");
#else
    t.python = pythonTool(roots, {".venv/bin/python3", "venv/bin/python3", "python/bin/python3"}, "python3");
    for (const auto& search : roots) {
        if (t.compiler.isEmpty()) t.compiler = tool(search, {"gcc/bin/x86_64-conda-linux-gnu-gcc", "third_party/gcc/bin/x86_64-conda-linux-gnu-gcc", "gcc/bin/gcc"});
        if (t.runtime.isEmpty()) t.runtime = tool(search, {"bin/runtime_linux_x11"});
        if (t.dotnet.isEmpty()) t.dotnet = tool(search, {"dotnet/dotnet"});
    }
    if (t.compiler.isEmpty()) t.compiler = QStandardPaths::findExecutable("gcc");
    if (t.dotnet.isEmpty()) t.dotnet = QStandardPaths::findExecutable("dotnet");
#endif
    return t;
}
QProcessEnvironment Toolchain::environment() const {
    auto env = QProcessEnvironment::systemEnvironment();
    QStringList paths;
    const QStringList roots = switchrSearchRoots(root);
    for (const auto& search : roots) {
#ifdef Q_OS_WIN
        const QString shaderTools = QDir(search).filePath("shader-tools/Library/bin");
#else
        const QString shaderTools = QDir(search).filePath("shader-tools/bin");
#endif
        if (!shaderTools.isEmpty() && QDir(shaderTools).exists()) {
            paths << shaderTools; env.insert("NXSHADER_TOOLS", shaderTools); break;
        }
    }
    // the launcher's own folder: <project>/bin/launcher in a checkout, the release root in a release
    QString launcherDir;
    for (const auto& search : roots) if (launcherDir.isEmpty() && QDir(QDir(search).filePath("bin/launcher")).exists()) launcherDir = QDir(search).filePath("bin/launcher");
    if (launcherDir.isEmpty() && isReleaseEngine(root)) launcherDir = roots.last();
    if (!launcherDir.isEmpty()) paths << launcherDir;
    for (const auto& binary : {python, compiler, dotnet}) if (!binary.isEmpty()) paths << QFileInfo(binary).absolutePath();
    paths << env.value("PATH"); env.insert("PATH", paths.join(QDir::listSeparator()));
    if (!python.isEmpty()) env.insert("SWR_PYTHON", python);
    if (!dotnet.isEmpty()) env.insert("DOTNET_ROOT", QFileInfo(dotnet).absolutePath());
    if (!keys.isEmpty()) env.insert("SWR_PROD_KEYS", keys);
#ifdef Q_OS_LINUX
    QStringList libs;
    for (const auto& search : roots) for (const auto& lib : {QString("lib"), QString("gcc/lib"), QString("shader-tools/lib")}) {
        const QString path = QDir(search).filePath(lib);
        if (QDir(path).exists()) libs << path;
    }
    if (!env.value("LD_LIBRARY_PATH").isEmpty()) libs << env.value("LD_LIBRARY_PATH");
    if (!libs.isEmpty()) env.insert("LD_LIBRARY_PATH", libs.join(':'));
#endif
    return env;
}
QString Toolchain::discoverRoot() {
    const QDir app(QCoreApplication::applicationDirPath());
    if (QFileInfo(app.filePath("engine/switchrecomp.py")).isFile()) return QDir(app.filePath("engine")).absolutePath();   // release
    QDir dir = app;   // checkout: bin/launcher (or a build folder) somewhere below <project>
    for (int i = 0; i < 8; ++i) {
        if (QFileInfo(dir.filePath("switchrecomp.py")).isFile()) return dir.absolutePath();
        const QString vendored = dir.filePath("third_party/Switch2RECOMP/switchrecomp.py");
        if (QFileInfo(vendored).isFile()) return QFileInfo(vendored).absolutePath();
        if (!dir.cdUp()) break;
    }
    return {};
}

Library::Library(QString file) : path(std::move(file)) {}
bool Library::load(QString* error) {
    QFile file(path);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) { writable = false; return fail(error, file.errorString()); }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject() || doc.object()["schema"].toInt() != 1) {
        writable = false;
        return fail(error, "The library file is invalid or from an unsupported version. It was left untouched: " + path);
    }
    const auto o = doc.object();
    toolRoot = o["toolRoot"].toString(); outputRoot = o["outputRoot"].toString(); keysPath = o["keysPath"].toString();
    jobs = qBound(1, o["jobs"].toInt(8), 64);
    viewMode = qBound(0, o["viewMode"].toInt(), 1);
    coverWidth = qBound(130, o["coverWidth"].toInt(180), 240);
    showBackground = o["showBackground"].toBool(true);
    colorMode = o["colorMode"].toString("dark");
    if(colorMode!="light"&&colorMode!="dark")colorMode="dark";
    accentColor = o["accentColor"].toString("lime");
    buttonLayout = normalizeButtonLayout(o["buttonLayout"].toString());
    if (buttonLayout.isEmpty()) buttonLayout = "switch";
    geometry = QByteArray::fromBase64(o["geometry"].toString().toLatin1());
    dockState = QByteArray::fromBase64(o["dockState"].toString().toLatin1());
    games.clear();
    for (const auto& value : o["games"].toArray()) {
        auto game = Game::fromJson(value.toObject());
        if (!game.id.isEmpty() && !game.title.isEmpty() && !find(game.id)) games.append(game);
    }
    return true;
}
bool Library::save(QString* error) const {
    if (!writable) return fail(error, "Library could not be loaded; refusing to overwrite it.");
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return fail(error, "Cannot create the library directory.");
    QJsonArray entries;
    for (const auto& g : games) entries.append(g.json());
    QJsonObject o{{"schema", 1}, {"games", entries}, {"toolRoot", toolRoot}, {"outputRoot", outputRoot}, {"keysPath", keysPath}, {"jobs", jobs},
        {"viewMode", viewMode}, {"coverWidth", coverWidth}, {"showBackground", showBackground},
        {"colorMode", colorMode}, {"accentColor", accentColor}, {"buttonLayout", buttonLayout},
        {"geometry", QString::fromLatin1(geometry.toBase64())}, {"dockState", QString::fromLatin1(dockState.toBase64())}};
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    const auto bytes = QJsonDocument(o).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit()) return fail(error, file.errorString());
    return true;
}
Game* Library::find(const QString& id) { for (auto& g : games) if (g.id == id) return &g; return nullptr; }
const Game* Library::find(const QString& id) const { for (const auto& g : games) if (g.id == id) return &g; return nullptr; }
bool Library::add(Game g) {
    for (const auto& existing : games) {
        if ((!g.source.isEmpty() && samePath(existing.source, g.source)) || samePath(existing.executable(), g.executable())) return false;
    }
    games.append(std::move(g)); return true;
}
QString Library::importBuild(const Game& game) {
    for(const auto& existing:games)if(samePath(existing.executable(),game.executable()))return existing.id;
    for(auto& existing:games) {
        if(!game.source.isEmpty()&&samePath(existing.source,game.source)&&!existing.hasBuild()) {
            existing.buildDir=game.buildDir;existing.outputName=game.outputName;
            existing.builtSource=game.builtSource;existing.buildIncomplete=false;existing.renderer=game.renderer;
            if(existing.cover.isEmpty())existing.cover=game.cover;
            return existing.id;
        }
    }
    games.append(game);return game.id;
}
int Library::discoverBuilds(const QString& folder) {
    if(folder.isEmpty()||!QDir(folder).exists())return 0;
    QStringList folders{folder};
    for(const auto& dir:QDir(folder).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks))folders<<dir.absoluteFilePath();
    int count=0;
    for(const auto& dir:folders)for(const auto& module:QDir(dir).entryInfoList({QString("*")+moduleSuffix},QDir::Files)) {
        const auto name=module.fileName().chopped(QString(moduleSuffix).size());
        Game game;QString error;
        if(!gameFromExecutable(QDir(dir).filePath(name+executableSuffix),&game,&error))continue;
        bool known=false;for(const auto& g:games)if(samePath(g.executable(),game.executable())){known=true;break;}
        if(!known){importBuild(game);++count;}
    }
    return count;
}

QString outputNameFor(const QString& title) {
    QString result;
    for (auto ch : title) if (ch.isLetterOrNumber() || QString("-_ ").contains(ch)) result += ch;
    return result.trimmed().isEmpty() ? "Game" : result.trimmed();
}
bool samePath(const QString& left,const QString& right){
    if(left.isEmpty()||right.isEmpty())return left==right;
    const auto normalized=[](const QString& path){return QDir::cleanPath(QFileInfo(QDir::fromNativeSeparators(path)).absoluteFilePath());};
#ifdef Q_OS_WIN
    return normalized(left).compare(normalized(right),Qt::CaseInsensitive)==0;
#else
    return normalized(left)==normalized(right);
#endif
}
QString titleForSource(const QString& source) {
    QFileInfo info(source);
    QString name = info.isDir() ? info.fileName() : info.completeBaseName();
    name.remove(QRegularExpression("\\s*\\[[^\\]]*\\]"));
    return name.trimmed().isEmpty() ? "Untitled game" : name.trimmed();
}
bool validSource(const QString& source, QString* error) {
    QFileInfo info(source);
    if (info.isDir()) {
        QDir d(source);
        for (const auto& name : {"main", "exefs/main", "ExeFS/main"}) if (QFileInfo(d.filePath(name)).isFile()) return true;
        return fail(error, "This folder does not contain exefs/main or main.");
    }
    QFile file(source);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    const auto header = file.read(0x14);
    if (info.suffix().compare("nup", Qt::CaseInsensitive) == 0 && header.startsWith(QByteArray("NUP\1", 4))) return true;
    if (info.suffix().compare("nsp", Qt::CaseInsensitive) == 0 && header.startsWith("PFS0")) return true;
    if (info.suffix().compare("nro", Qt::CaseInsensitive) == 0 && header.mid(0x10, 4) == "NRO0") return true;
    return fail(error, "Select an indexed NUP, a standard application NSP, an NRO, or an extracted ExeFS folder.");
}
Game gameFromSource(const QString& source, const QString& outputRoot) {
    Game g;
    g.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    g.source = absolute(source); g.title = titleForSource(source); g.outputName = outputNameFor(g.title);
    const QString base = outputRoot.isEmpty() ? QDir(QFileInfo(source).absolutePath()).filePath("SwitchRecomp Builds") : outputRoot;
    g.buildDir = QDir(base).filePath(g.outputName);
    return g;
}
bool gameFromExecutable(const QString& path, Game* game, QString* error) {
    if (!QFileInfo(path).isFile()) return fail(error, "The executable does not exist.");
    Game g;
    g.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    g.title = QFileInfo(path).fileName();
    if (QString(executableSuffix).size() && g.title.endsWith(executableSuffix, Qt::CaseInsensitive)) g.title.chop(QString(executableSuffix).size());
    g.outputName = g.title; g.buildDir = QFileInfo(path).absolutePath();
    if (!g.hasBuild()) return fail(error, "A native build needs its executable, matching code module, and _data folder together.");
    QFile marker(QDir(g.dataDir()).filePath("romfs.nup"));
    if (marker.open(QIODevice::ReadOnly)) g.source = QString::fromUtf8(marker.readLine()).trimmed();
    if (g.source.isEmpty()) {
        QFile nspMarker(QDir(g.dataDir()).filePath("romfs.nsp"));
        if (nspMarker.open(QIODevice::ReadOnly) && nspMarker.readLine().trimmed() == "SWRNSP1")
            g.source = QString::fromUtf8(nspMarker.readLine()).trimmed();
    }
    g.builtSource = g.source; g.renderer = readRenderer(g.iniPath());
    for (const auto& name : {"icon.jpg", "icon.png", "icon_AmericanEnglish.dat"}) {
        const QString icon = QDir(g.dataDir()).filePath(name);
        if (QFileInfo(icon).isFile()) { g.cover = icon; break; }
    }
    *game = g; return true;
}
QString readRenderer(const QString& ini) {
    QFile file(ini);
    QString renderer = "opengl";
    if (!file.open(QIODevice::ReadOnly)) return renderer;
    for (const auto& line : QString::fromUtf8(file.readAll()).split('\n')) {
        const auto trimmed = line.trimmed();
        if (trimmed.startsWith("renderer=")) renderer = trimmed.mid(9).trimmed();
    }
    return renderer;
}
// Sets key=value in the INI, keeping every other line, comment and line ending; adds it under [section] if missing.
static bool writeIniKey(const QString& ini, const QString& section, const QString& keyName, const QString& value, QString* error) {
    QFile input(ini);
    QString text;
    if (input.exists()) {
        if (!input.open(QIODevice::ReadOnly)) return fail(error, input.errorString());
        text = QString::fromUtf8(input.readAll()); input.close();
    }
    const QString newline = text.contains("\r\n") ? "\r\n" : "\n";
    text.replace("\r\n", "\n");
    auto lines = text.split('\n');
    const QRegularExpression key("^\\s*" + QRegularExpression::escape(keyName) + "\\s*=");
    const QString line = keyName + "=" + value, header = "[" + section + "]";
    bool found = false;
    int at = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed() == header) at = i;
        if (key.match(lines[i]).hasMatch()) { lines[i] = line; found = true; }
    }
    if (!found) {
        if (at >= 0) lines.insert(at + 1, line);
        else { lines << header << line << ""; }
    }
    QSaveFile output(ini);
    if (!output.open(QIODevice::WriteOnly)) return fail(error, output.errorString());
    const auto bytes = lines.join(newline).toUtf8();
    if (output.write(bytes) != bytes.size() || !output.commit()) return fail(error, output.errorString());
    return true;
}
bool readDocked(const QString& ini) {
    QFile file(ini);
    bool docked = true;
    if (!file.open(QIODevice::ReadOnly)) return docked;
    for (const auto& line : QString::fromUtf8(file.readAll()).split('\n')) {
        const auto trimmed = line.trimmed();
        if (trimmed.startsWith("docked=")) docked = trimmed.mid(7).trimmed() != "0";
    }
    return docked;
}
bool writeDocked(const QString& ini, bool docked, QString* error) {
    // The engine moves an INI without settings_version (or below 2) to TV mode once; a choice made here is not one to move,
    // so such an INI gets version 2. A version the engine wrote stays.
    QFile file(ini);
    const bool versioned = file.open(QIODevice::ReadOnly) && QString::fromUtf8(file.readAll()).contains("settings_version=");
    file.close();
    return writeIniKey(ini, "Display", "docked", docked ? "1" : "0", error)
        && (versioned || writeIniKey(ini, "Other", "settings_version", "2", error));
}
bool writeRenderer(const QString& ini, const QString& renderer, QString* error) {
    if (renderer != "vulkan" && renderer != "opengl") return fail(error, "Unknown renderer.");
    return writeIniKey(ini, "Graphics", "renderer", renderer, error);
}
bool validButtonLayout(const QString& layout) { return layout.isEmpty() || layout == "switch" || layout == "xbox"; }
QString normalizeButtonLayout(const QString& layout) {
    if (layout == "switch" || layout == "nintendo" || layout == "position" || layout == "positions") return "switch";
    if (layout == "xbox" || layout == "label" || layout == "labels") return "xbox";
    return {};
}
QString effectiveButtonLayout(const Game& game, const QString& launcherDefault) {
    const QString own = normalizeButtonLayout(game.buttonLayout);
    if (!own.isEmpty()) return own;
    return normalizeButtonLayout(launcherDefault) == "xbox" ? "xbox" : "switch";
}
bool writeButtonLayout(const QString& ini, const QString& layout, QString* error) {
    if (layout != "switch" && layout != "xbox") return fail(error, "Unknown controller layout.");
    return writeIniKey(ini, "Controls", "button_layout", layout, error);
}
bool writeGameSettings(const Game& game, const QString& launcherLayout, QString* error) {
    return writeRenderer(game.iniPath(), game.renderer, error)
        && writeButtonLayout(game.iniPath(), effectiveButtonLayout(game, launcherLayout), error);
}

bool LibraryFilter::matches(const Game& game, bool running, bool building) const {
    if (!game.title.contains(text.trimmed(), Qt::CaseInsensitive)) return false;
    const bool nup = inPlacePackage(game.source);
    const bool nro = game.source.endsWith(".nro", Qt::CaseInsensitive);
    if (source == 1 && !nup) return false;
    if (source == 2 && !nro) return false;
    if (source == 3 && (nup || nro || game.source.isEmpty())) return false;
    if (source == 4 && !game.source.isEmpty()) return false;
    if (state == 1 && (!game.hasBuild() || running || building || (nup && !game.sourceAvailable()))) return false;
    if (state == 2 && (game.hasBuild() || building || running)) return false;
    if (state == 3 && !running) return false;
    return true;
}
bool inPlacePackage(const QString& path) {
    return path.endsWith(".nup", Qt::CaseInsensitive) || path.endsWith(".nsp", Qt::CaseInsensitive);
}
int LibraryFilter::activeCount() const { return (!text.trimmed().isEmpty() ? 1 : 0) + (state != 0 ? 1 : 0) + (source != 0 ? 1 : 0); }

CardState cardState(const Game& game, bool running, bool building, bool otherBuild, bool toolsReady) {
    if(building)return {"Cancel build","Building",true,false,true,false};
    if(running)return {"Stop","Running",true,false,false,true};
    if(game.hasBuild()){
        if(inPlacePackage(game.source)&&!game.sourceAvailable())
            return {"Play","Source offline",false,true,false,false};
        return {"Play","Ready to play",true,false,false,false};
    }
    return {"Build",game.buildIncomplete?"Build incomplete":"Needs build",
        !otherBuild&&toolsReady&&game.sourceAvailable(),true,false,false};
}
