// nxrs - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#pragma once
#include <QAbstractButton>
#include <QPixmap>
#include <QWidget>

class QMenuBar;

// One of the three window buttons, drawn in the app theme. The maximize button is hit-tested as HTMAXBUTTON on
// Windows (for Snap Layouts), so Windows reports its hover/press as non-client messages; nativeHover/nativePressed
// carry that state because Qt never sees those as mouse events.
class CaptionButton : public QAbstractButton {
    Q_OBJECT
public:
    enum Kind { Minimize, Maximize, Close };
    CaptionButton(Kind kind, QWidget* parent = nullptr);
    Kind kind() const { return role; }
    void setNativeState(bool hover, bool pressed);
    bool nativePressed() const { return downNative; }
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    Kind role;
    bool hoverNative = false, downNative = false;
};

// Client-drawn title bar for the main window: themed app glyph, the menu bar (VS Code-style, menus share the
// caption row) and the window buttons; no title text (the taskbar and Alt-Tab still show the window title). Installed with QMainWindow::setMenuWidget().
// Windows keeps the native frame styles (WS_THICKFRAME | WS_CAPTION) and only removes the caption from the
// non-client area (WM_NCCALCSIZE), so snap, shadow, minimize/restore animation, Alt+Space and edge resizing stay
// native; Linux uses Qt::FramelessWindowHint with QWindow::startSystemMove/startSystemResize.
class TitleBar : public QWidget {
    Q_OBJECT
public:
    enum Part { Client, Caption, MinimizeButton, MaximizeButton, CloseButton };
    explicit TitleBar(QWidget* window);
    QMenuBar* menuBar() const { return menus; }
    CaptionButton* button(CaptionButton::Kind kind) const;
    // What a point (in this widget's coordinates) is: caption (drag/double-click/snap), a window button, or
    // client (menu items and anything else that takes clicks).
    Part hitTest(const QPoint& local) const;
    QRect iconRect() const;
    void setTopInset(int pixels);   // logical px of the bar hidden off-screen (maximized on Windows)
    // Forwarded from the main window's QWidget::nativeEvent; returns true when the message was handled.
    bool handleNativeEvent(const QByteArray& eventType, void* message, qintptr* result);
#ifdef Q_OS_WIN
    static int nativeHitCode(Part part);
#endif
    void toggleMaximized();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;
private:
    void install();
    QWidget* host;
    QMenuBar* menus;
    CaptionButton *minimize, *maximize, *close;
    int inset = 0;
    QPixmap glyph;
    QString glyphKey;
};
