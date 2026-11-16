// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#pragma once
#include "library.h"
#include <QMap>
QMap<QString,qint64> findRunningGames(const QVector<Game>& games);
bool requestGameClose(qint64 pid, const QString& executable);
