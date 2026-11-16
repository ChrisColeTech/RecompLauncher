// RecompLauncher - native game library and build launcher.
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
    bool active = false, cancelled = false;
};
