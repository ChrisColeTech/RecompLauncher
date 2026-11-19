// nxrs - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#pragma once
#include "library.h"
#include <QFile>
#include <QObject>
#include <QProcess>

class BuildManager : public QObject {
    Q_OBJECT
public:
    explicit BuildManager(QObject* parent = nullptr);
    bool start(const Game& game, const Toolchain& tools, int jobs, QString* error);
    void cancel();
    bool busy() const { return active; }
    QString gameId() const { return game.id; }
    static QStringList arguments(const Game& game, const Toolchain& tools, int jobs);
    // The engine said the package just built also holds add-on content (a 1G+1U+1D NSP).
    bool sourceHoldsDlc() const { return holdsDlc; }
    static bool reportsPackageDlc(const QString& line) { return line.startsWith("DLC: this package also holds add-on content"); }
    // Game::updateToken() of the game as it was handed to start(): what a finished build has applied.
    QString appliedUpdate() const { return game.updateToken(); }
signals:
    void output(const QString& text);
    void progress(const QString& phase, int current, int total);
    void finished(const QString& id, bool success, const QString& detail);
private:
    void readOutput();
    void parseLine(const QString& line);
    void complete(bool success, const QString& detail);
    QProcess process;
    QFile log;
    QByteArray pending;
    Game game;
    bool active = false, cancelled = false, holdsDlc = false;
};

// Game updates: asks the engine what update packages are (--update-info) and whether they belong to the game: the
// engine opens each one over the game's own package with the user's keys. Nothing is written anywhere.
class UpdateProbe : public QObject {
    Q_OBJECT
public:
    explicit UpdateProbe(QObject* parent = nullptr);
    bool start(const Game& game, const Toolchain& tools, const QStringList& packages, QString* error);
    bool busy() const { return active; }
    QString gameId() const { return game.id; }
signals:
    void output(const QString& text);
    // found: the packages the engine accepted; errors: its reasons for the others, one per package
    void finished(const QString& id, const QVector<GameUpdate>& found, const QStringList& errors);
private:
    void readOutput();
    void complete();
    QProcess process;
    QByteArray pending;
    QVector<GameUpdate> found;
    QStringList errors;
    Game game;
    bool active = false;
};

// Add-on content: runs the engine's --sync-dlc for one built game, after which its data folder holds exactly the
// packages of Game::dlc (new ones installed, removed ones deleted). Nothing is ever written to the packages.
class DlcInstaller : public QObject {
    Q_OBJECT
public:
    explicit DlcInstaller(QObject* parent = nullptr);
    bool start(const Game& game, const Toolchain& tools, QString* error);
    bool busy() const { return active; }
    QString gameId() const { return game.id; }
signals:
    void output(const QString& text);
    // errors: the engine's "DLC error: ..." lines without the prefix, one per package that was refused
    void finished(const QString& id, bool success, const QStringList& errors);
private:
    void readOutput();
    void complete(bool success);
    QProcess process;
    QByteArray pending;
    QStringList errors;
    Game game;
    bool active = false;
};
