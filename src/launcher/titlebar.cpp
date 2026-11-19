// nxrs - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "titlebar.h"
#include "theme.h"
#include <QApplication>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>
#include <QtMath>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#endif

namespace {
constexpr int BarHeight = 36, ButtonWidth = 46, IconSize = 18, IconLeft = 13;
#ifndef Q_OS_WIN
// Frameless windows on X11/Wayland have no resize border: resize from the outer few pixels of the window with
// QWindow::startSystemResize, which hands the move to the window manager (keeps its snapping and constraints).
class EdgeResizer : public QObject {
public:
    EdgeResizer(QWidget* window) : QObject(window), window(window) {}
    bool eventFilter(QObject* watched, QEvent* event) override {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget || widget->window() != window || window->isMaximized() || window->isFullScreen()) return false;
        if (event->type() != QEvent::MouseMove && event->type() != QEvent::MouseButtonPress) return false;
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QPoint p = window->mapFromGlobal(mouse->globalPosition().toPoint());
        const int b = 6;
        Qt::Edges edges;
        if (p.x() < b) edges |= Qt::LeftEdge;
        if (p.x() >= window->width() - b) edges |= Qt::RightEdge;
        if (p.y() < b) edges |= Qt::TopEdge;
        if (p.y() >= window->height() - b) edges |= Qt::BottomEdge;
        if (event->type() == QEvent::MouseMove && mouse->buttons() == Qt::NoButton) {
            Qt::CursorShape shape = Qt::ArrowCursor;
            if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)) shape = Qt::SizeFDiagCursor;
            else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge)) shape = Qt::SizeBDiagCursor;
            else if (edges & (Qt::LeftEdge | Qt::RightEdge)) shape = Qt::SizeHorCursor;
            else if (edges & (Qt::TopEdge | Qt::BottomEdge)) shape = Qt::SizeVerCursor;
            if (edges && !overriding) { QGuiApplication::setOverrideCursor(shape); overriding = true; }
            else if (edges && overriding) QGuiApplication::changeOverrideCursor(shape);
            else if (!edges && overriding) { QGuiApplication::restoreOverrideCursor(); overriding = false; }
            return false;
        }
        if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton && edges && window->windowHandle())
            return window->windowHandle()->startSystemResize(edges);
        return false;
    }
private:
    QWidget* window;
    bool overriding = false;
};
#endif
}

CaptionButton::CaptionButton(Kind kind, QWidget* parent) : QAbstractButton(parent), role(kind) {
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    setAccessibleName(kind == Minimize ? "Minimize" : kind == Maximize ? "Maximize" : "Close");
}
QSize CaptionButton::sizeHint() const { return {ButtonWidth, BarHeight}; }
void CaptionButton::setNativeState(bool hover, bool pressed) {
    if (hover == hoverNative && pressed == downNative) return;
    hoverNative = hover; downNative = pressed; update();
}
void CaptionButton::enterEvent(QEnterEvent* event) { QAbstractButton::enterEvent(event); update(); }
void CaptionButton::leaveEvent(QEvent* event) { QAbstractButton::leaveEvent(event); update(); }
void CaptionButton::paintEvent(QPaintEvent*) {
    const auto& c = Theme::colors();
    const bool hot = hoverNative || underMouse(), down = downNative || isDown();
    const bool active = !window() || window()->isActiveWindow();
    QPainter p(this);
    QColor ink = active ? c.text : c.muted;
    if (role == Close && (hot || down)) {
        p.fillRect(rect(), down ? QColor("#b3261e") : QColor("#c42b1c"));   // Windows close-button red
        ink = Qt::white;
    } else if (down) p.fillRect(rect(), c.selected);
    else if (hot) p.fillRect(rect(), c.hover);
    p.setRenderHint(QPainter::Antialiasing);
    // 10x10 glyphs on the pixel grid, like the system caption buttons
    const qreal px = 1.0 / devicePixelRatioF();
    p.setPen(QPen(ink, qMax(1.0, devicePixelRatioF()) * px, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);
    const QPointF centre(width() / 2.0, height() / 2.0);
    const QRectF box(centre.x() - 5, centre.y() - 5, 10, 10);
    if (role == Minimize) p.drawLine(QPointF(box.left(), centre.y()), QPointF(box.right(), centre.y()));
    else if (role == Close) { p.drawLine(box.topLeft(), box.bottomRight()); p.drawLine(box.topRight(), box.bottomLeft()); }
    else if (window() && window()->isMaximized()) {
        const QRectF front = box.adjusted(0, 2, -2, 0);
        p.drawRect(front);
        p.drawLine(QPointF(front.left() + 2, box.top()), QPointF(box.right(), box.top()));
        p.drawLine(QPointF(box.right(), box.top()), QPointF(box.right(), front.bottom() - 2));
    } else p.drawRect(box);
}

TitleBar::TitleBar(QWidget* window) : QWidget(window), host(window) {
    setObjectName("TitleBar");
    setFixedHeight(BarHeight);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(IconLeft + IconSize + 7, 0, 0, 0);
    row->setSpacing(0);
    menus = new QMenuBar(this);
    menus->setNativeMenuBar(false);   // keep the menus in this row on desktops with a global menu
    menus->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    row->addWidget(menus, 0, Qt::AlignVCenter);
    row->addStretch(1);
    minimize = new CaptionButton(CaptionButton::Minimize, this);
    maximize = new CaptionButton(CaptionButton::Maximize, this);
    close = new CaptionButton(CaptionButton::Close, this);
    for (auto* b : {minimize, maximize, close}) row->addWidget(b);
    connect(minimize, &QAbstractButton::clicked, host, &QWidget::showMinimized);
    connect(maximize, &QAbstractButton::clicked, this, &TitleBar::toggleMaximized);
    connect(close, &QAbstractButton::clicked, host, &QWidget::close);
    host->installEventFilter(this);
    install();
}

CaptionButton* TitleBar::button(CaptionButton::Kind kind) const {
    return kind == CaptionButton::Minimize ? minimize : kind == CaptionButton::Maximize ? maximize : close;
}
QRect TitleBar::iconRect() const { return {IconLeft, inset + (BarHeight - IconSize) / 2, IconSize, IconSize}; }
// A maximized window hangs its frame off-screen. The client area keeps starting at the window's top edge in every
// state (so Qt's frame margins, which it also applies to the saved normal geometry, never change), and the bar
// pushes its content below the off-screen part instead.
void TitleBar::setTopInset(int pixels) {
    if (pixels == inset) return;
    inset = pixels;
    setFixedHeight(BarHeight + inset);
    layout()->setContentsMargins(IconLeft + IconSize + 7, inset, 0, 0);
    update();
}
void TitleBar::toggleMaximized() { if (host->isMaximized()) host->showNormal(); else host->showMaximized(); }

TitleBar::Part TitleBar::hitTest(const QPoint& local) const {
    if (!rect().contains(local)) return Client;
    for (auto* b : {minimize, maximize, close})   // the whole column, so the screen-edge pixel of a maximized window hits it
        if (b->isVisible() && local.x() >= b->x() && local.x() <= b->geometry().right() && local.y() <= b->geometry().bottom())
            return b == minimize ? MinimizeButton : b == maximize ? MaximizeButton : CloseButton;
    if (menus->isVisible() && menus->geometry().contains(local) && menus->actionAt(menus->mapFrom(this, local))) return Client;
    return Caption;   // icon, title, and every empty pixel of the row
}

void TitleBar::paintEvent(QPaintEvent*) {
    const auto& c = Theme::colors();
    QPainter p(this);
    p.fillRect(rect(), c.surface);
    p.fillRect(QRect(0, height() - 1, width(), 1), c.border);
    const qreal dpr = devicePixelRatioF();
    const QString key = c.accent.name() + QString::number(dpr);
    if (key != glyphKey) { glyph = Theme::accentGlyph(c.accent, IconSize, dpr); glyphKey = key; }
    p.drawPixmap(iconRect().topLeft(), glyph);
}

// Linux (and any non-Windows platform): the bar itself moves the window. On Windows the caption is a native
// HTCAPTION area, so these never see a press on empty bar space there.
void TitleBar::mousePressEvent(QMouseEvent* event) {
#ifndef Q_OS_WIN
    if (event->button() == Qt::LeftButton && hitTest(event->position().toPoint()) == Caption && host->windowHandle()) {
        host->windowHandle()->startSystemMove();
        event->accept();
        return;
    }
#endif
    QWidget::mousePressEvent(event);
}
void TitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && hitTest(event->position().toPoint()) == Caption) { toggleMaximized(); event->accept(); return; }
    QWidget::mouseDoubleClickEvent(event);
}

bool TitleBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == host) {
        switch (event->type()) {
        case QEvent::WindowStateChange: maximize->setAccessibleName(host->isMaximized() ? "Restore" : "Maximize"); [[fallthrough]];
        case QEvent::ActivationChange:
            update(); for (auto* b : {minimize, maximize, close}) b->update();
            break;
#ifdef Q_OS_WIN
        case QEvent::WinIdChange: install(); break;
#endif
        default: break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TitleBar::install() {
#ifdef Q_OS_WIN
    // Re-run WM_NCCALCSIZE now that the window answers it (the handle may have been created before).
    if (const WId id = host->internalWinId())
        SetWindowPos(reinterpret_cast<HWND>(id), nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
#else
    static bool resizer = false;
    host->setWindowFlag(Qt::FramelessWindowHint, true);
    if (!resizer) { qApp->installEventFilter(new EdgeResizer(host)); resizer = true; }
#endif
}

#ifdef Q_OS_WIN
int TitleBar::nativeHitCode(Part part) {
    switch (part) {
    case Caption: return HTCAPTION;
    case MaximizeButton: return HTMAXBUTTON;   // Windows 11 shows Snap Layouts on hover of this code
    default: return HTCLIENT;                  // minimize/close stay ordinary Qt buttons
    }
}
namespace {
int frameThickness(HWND hwnd) {
    const UINT dpi = GetDpiForWindow(hwnd);
    return GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}
void showSystemMenu(HWND hwnd, POINT at) {
    HMENU menu = GetSystemMenu(hwnd, FALSE);
    if (!menu) return;
    const bool zoomed = IsZoomed(hwnd);
    MENUITEMINFOW info{};
    info.cbSize = sizeof info; info.fMask = MIIM_STATE;
    auto enable = [&](UINT item, bool on) { info.fState = on ? MFS_ENABLED : MFS_DISABLED; SetMenuItemInfoW(menu, item, FALSE, &info); };
    enable(SC_RESTORE, zoomed); enable(SC_MOVE, !zoomed); enable(SC_SIZE, !zoomed); enable(SC_MINIMIZE, true);
    enable(SC_MAXIMIZE, !zoomed); enable(SC_CLOSE, true);
    SetMenuDefaultItem(menu, zoomed ? SC_RESTORE : SC_MAXIMIZE, FALSE);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN),
                                        at.x, at.y, 0, hwnd, nullptr);
    if (command) PostMessageW(hwnd, WM_SYSCOMMAND, command, 0);
}
}
#endif

bool TitleBar::handleNativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG") return false;
    auto* msg = static_cast<MSG*>(message);
    HWND hwnd = msg->hwnd;
    if (!host->internalWinId() || hwnd != reinterpret_cast<HWND>(host->internalWinId())) return false;   // never winId(): it would re-enter window creation
    switch (msg->message) {
    case WM_NCCALCSIZE: {
        if (!msg->wParam) return false;
        // Let Windows compute the standard frame, then give the caption strip back to the client area. The side
        // and bottom borders stay native (invisible resize handles + DWM shadow). Maximized, the top frame is
        // off-screen; setTopInset() moves the bar's content below it.
        auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
        const LONG top = params->rgrc[0].top;
        const LRESULT standard = DefWindowProcW(hwnd, WM_NCCALCSIZE, msg->wParam, msg->lParam);
        params->rgrc[0].top = top;
        const int wanted = IsZoomed(hwnd) ? qCeil(frameThickness(hwnd) * 96.0 / GetDpiForWindow(hwnd)) : 0;
        if (wanted != inset) QMetaObject::invokeMethod(this, [this, wanted] { setTopInset(wanted); }, Qt::QueuedConnection);
        *result = standard;
        return true;
    }
    case WM_NCHITTEST: {
        const LRESULT standard = DefWindowProcW(hwnd, WM_NCHITTEST, msg->wParam, msg->lParam);
        if (standard != HTCLIENT) { *result = standard; return true; }   // left/right/bottom resize borders
        POINT pt{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        ScreenToClient(hwnd, &pt);
        RECT client; GetClientRect(hwnd, &client);
        const int border = frameThickness(hwnd);
        if (!IsZoomed(hwnd) && pt.y < border) {
            *result = pt.x < border * 2 ? HTTOPLEFT : pt.x >= client.right - border * 2 ? HTTOPRIGHT : HTTOP;
            return true;
        }
        const qreal dpr = host->devicePixelRatioF();
        const QPoint local = mapFrom(host, QPoint(qFloor(pt.x / dpr), qFloor(pt.y / dpr)));
        *result = nativeHitCode(hitTest(local));
        return true;
    }
    // The maximize button is HTMAXBUTTON, so its mouse traffic arrives as non-client messages. Handle them
    // here: DefWindowProc would otherwise paint the classic caption button over the bar.
    case WM_NCMOUSEMOVE:
        if (msg->wParam == HTMAXBUTTON) {
            maximize->setNativeState(true, maximize->nativePressed());
            TRACKMOUSEEVENT track{sizeof track, TME_LEAVE | TME_NONCLIENT, hwnd, 0};
            TrackMouseEvent(&track);
            *result = 0;
            return true;
        }
        maximize->setNativeState(false, false);
        return false;
    case WM_NCMOUSELEAVE:
    case WM_MOUSELEAVE:
        maximize->setNativeState(false, false);
        return false;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (msg->wParam == HTMAXBUTTON) { maximize->setNativeState(true, true); *result = 0; return true; }
        return false;
    case WM_NCLBUTTONUP:
        if (msg->wParam == HTMAXBUTTON) {
            const bool pressed = maximize->nativePressed();
            maximize->setNativeState(true, false);
            if (pressed) toggleMaximized();
            *result = 0;
            return true;
        }
        return false;
    case WM_NCRBUTTONUP:
        // Qt would turn the WM_CONTEXTMENU for this client-area caption into a Qt event; show the system menu.
        if (msg->wParam == HTCAPTION) {
            showSystemMenu(hwnd, POINT{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)});
            *result = 0;
            return true;
        }
        return false;
    default:
        return false;
    }
#else
    Q_UNUSED(eventType); Q_UNUSED(message); Q_UNUSED(result);
    return false;
#endif
}
