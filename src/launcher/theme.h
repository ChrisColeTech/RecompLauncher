// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QVector>
class QApplication;
namespace Theme {
struct Colors {
    QColor canvas, surface, raised, field, text, muted, accent, accentText, selected, border, hover;
    bool light = false;
};
struct Accent { QString id, label, dark, light; };
QVector<Accent> accents();
Colors colorsFor(const QString& mode, const QString& accent);
const Colors& colors();
void apply(QApplication& app, const QString& mode = "dark", const QString& accent = "lime");
QString stylesheet(const Colors& colors);
QIcon appIcon();
// The app glyph in a given accent (rounded square, controller cut out), for the in-app title bar.
QPixmap accentGlyph(const QColor& accent, int size, qreal devicePixelRatio = 1.0);
}
