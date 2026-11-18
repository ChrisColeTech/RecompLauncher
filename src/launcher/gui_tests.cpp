// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "dlcpanel.h"
#include "filterpopup.h"
#include "window.h"
#include "theme.h"
#include "titlebar.h"
#include <QMainWindow>
#include <QMenuBar>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QTemporaryDir>
#include <QPushButton>
#include <QToolButton>
#include <QSignalSpy>
#include <QtTest>

class FilterPopupTests:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QApplication::setStyle("Fusion");Theme::apply(*qApp);}
    void clearsAsOneUpdate(){
        QWidget owner;FilterPopup popup(&owner);popup.search->setText("Bananza");popup.state->setCurrentIndex(2);popup.source->setCurrentIndex(1);popup.sort->setCurrentIndex(2);
        QSignalSpy changed(&popup,&FilterPopup::filtersChanged);popup.clearFilters();
        QCOMPARE(changed.count(),1);QVERIFY(popup.search->text().isEmpty());QCOMPARE(popup.state->currentIndex(),0);QCOMPARE(popup.source->currentIndex(),0);QCOMPARE(popup.sort->currentIndex(),0);
    }
    void titleBarHitRegions(){
        QMainWindow window;window.setWindowTitle("RecompLauncher");auto* bar=new TitleBar(&window);window.setMenuWidget(bar);
        bar->menuBar()->addMenu("&File");bar->menuBar()->addMenu("&Help");window.setCentralWidget(new QWidget);window.resize(900,500);window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto centre=[](QWidget* w){return w->geometry().center();};
        QCOMPARE(bar->hitTest(centre(bar->button(CaptionButton::Minimize))),TitleBar::MinimizeButton);
        QCOMPARE(bar->hitTest(centre(bar->button(CaptionButton::Maximize))),TitleBar::MaximizeButton);
        QCOMPARE(bar->hitTest(centre(bar->button(CaptionButton::Close))),TitleBar::CloseButton);
        auto* menus=bar->menuBar();const QRect file=menus->actionGeometry(menus->actions().first());
        QCOMPARE(bar->hitTest(menus->mapTo(bar,file.center())),TitleBar::Client);          // menu items stay clickable
        QCOMPARE(bar->hitTest(bar->iconRect().center()),TitleBar::Caption);
        QCOMPARE(bar->hitTest(QPoint(bar->width()/2,bar->height()/2)),TitleBar::Caption);  // empty row drags the window
        QCOMPARE(bar->hitTest(QPoint(bar->width()/2,bar->height()+5)),TitleBar::Client);
        QVERIFY(bar->button(CaptionButton::Close)->geometry().right()>=bar->width()-1);
#ifdef Q_OS_WIN
        QCOMPARE(TitleBar::nativeHitCode(TitleBar::Caption),2);       // HTCAPTION
        QCOMPARE(TitleBar::nativeHitCode(TitleBar::MaximizeButton),9);// HTMAXBUTTON (Snap Layouts)
        QCOMPARE(TitleBar::nativeHitCode(TitleBar::Client),1);        // HTCLIENT
        QCOMPARE(TitleBar::nativeHitCode(TitleBar::CloseButton),1);
#endif
        bar->setTopInset(7);QCOMPARE(bar->height(),43);QTRY_COMPARE(bar->button(CaptionButton::Close)->y(),7);   // maximized: frame hangs off-screen
        QCOMPARE(bar->hitTest(QPoint(bar->button(CaptionButton::Close)->geometry().center().x(),0)),TitleBar::CloseButton);   // screen-edge pixel still hits the button
        QCOMPARE(bar->iconRect().top(),7+(36-18)/2);bar->setTopInset(0);
        bar->toggleMaximized();QTRY_VERIFY(window.isMaximized());QCOMPARE(bar->button(CaptionButton::Maximize)->accessibleName(),QString("Restore"));
    }
    void titleBarIconFollowsAccent(){
        QMainWindow window;auto* bar=new TitleBar(&window);window.setMenuWidget(bar);window.resize(900,400);window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        const QRect icon=bar->iconRect();const QPoint edge(icon.center().x(),icon.top()+2);   // top band of the square, above the controller cut-out
        auto sample=[&]{const QImage image=bar->grab().toImage();return QColor(image.pixel(qRound(edge.x()*image.devicePixelRatio()),qRound(edge.y()*image.devicePixelRatio())));};
        Theme::apply(*qApp,"dark","lime");bar->update();QCOMPARE(sample().name(),Theme::colors().accent.name());
        Theme::apply(*qApp,"light","rose");bar->update();QCOMPARE(sample().name(),Theme::colors().accent.name());
        const QImage glyph=Theme::accentGlyph(QColor("#ff9cbd"),18,3).toImage();QCOMPARE(glyph.size(),QSize(54,54));
        QCOMPARE(qAlpha(glyph.pixel(27,32)),0);                                                // controller body is cut out
        Theme::apply(*qApp,"dark","lime");
    }
    void gameMenuInstallsDlc(){
        // demo mode: a preview library that is never saved, no game processes are looked for
        // The main window lives as long as the process (its tool checks run in the background), so it is not destroyed here.
        QTemporaryDir tmp;auto& window=*new Window(tmp.filePath("library.json"),tmp.path(),true);
        auto* bar=window.findChild<TitleBar*>();QVERIFY(bar);
        QMenu* game=nullptr;for(auto* action:bar->menuBar()->actions())if(action->text()=="&Game")game=action->menu();
        QVERIFY(game);QStringList names;for(auto* action:game->actions())names<<(action->isSeparator()?"-":action->text());
        QCOMPARE(names,QStringList({"Play","Stop","Build","-","Install DLC…","-","Remove from library…"}));
        auto* install=window.findChild<QAction*>("InstallDlc");QVERIFY(install);QVERIFY(game->actions().contains(install));
        QVERIFY(!install->isEnabled());                       // the preview library has nothing to install into
        window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.showMenu("game");QTRY_VERIFY(game->isVisible());game->close();window.hide();
    }
    void propertiesListAndRemoveDlc(){
        QTemporaryDir tmp;Game game;game.id="g";game.title="Kart";game.outputName="Kart";game.buildDir=tmp.filePath("build");
        QDir().mkpath(game.dataDir());
        for(const auto& path:{game.executable(),game.codeModule()}){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write("fixture");}
        const QString booster=tmp.filePath("Booster [v65536].nsp"),later=tmp.filePath("Later.nsz");
        for(const auto& path:{booster,later}){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write("PFS0");}
        {QFile f(game.dataDir()+"/dlc.manifest");QVERIFY(f.open(QIODevice::WriteOnly));f.write(("0100152000023001 65536 data "+booster+"\n").toUtf8());}
        game.dlc={booster,later};
        DlcPanel panel(game,false);
        QCOMPARE(panel.list->count(),2);
        QCOMPARE(panel.list->item(0)->text(),QString("Booster [v65536].nsp — 0100152000023001 v65536"));
        QCOMPARE(panel.list->item(1)->text(),QString("Later.nsz — not installed yet"));
        QCOMPARE(panel.list->item(0)->toolTip(),QDir::toNativeSeparators(booster));
        QVERIFY(panel.note->text().contains("Remove"));
        panel.show();QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.list->setFocus();panel.list->setCurrentRow(0);QTest::keyClick(panel.list,Qt::Key_Delete);      // Delete removes the entry
        QCOMPARE(panel.packages(),QStringList(later));QCOMPARE(panel.list->count(),1);
        panel.removeRow(0);QVERIFY(panel.packages().isEmpty());QVERIFY(panel.note->text().contains("Game > Install DLC"));
        panel.removeRow(0);QVERIFY(panel.packages().isEmpty());
        // a running or building game: the list shows, nothing can be removed
        DlcPanel locked(game,true);locked.removeRow(0);QCOMPARE(locked.packages(),game.dlc);QVERIFY(locked.note->text().contains("cannot be changed"));
    }
    void escapeClosesPopup(){
        QWidget owner;QPushButton anchor("Find games",&owner);owner.resize(400,300);anchor.resize(100,70);owner.show();
        FilterPopup popup(&owner);popup.openBelow(&anchor);QTRY_VERIFY(popup.isVisible());
        QVERIFY(!popup.search->accessibleName().isEmpty());QTest::keyClick(popup.search,Qt::Key_Escape);QTRY_VERIFY(!popup.isVisible());
    }
};
QTEST_MAIN(FilterPopupTests)
#include "gui_tests.moc"
