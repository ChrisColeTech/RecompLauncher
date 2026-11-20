// nxrs - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "dlcpanel.h"
#include "updatepanel.h"
#include "filterpopup.h"
#include "window.h"
#include "theme.h"
#include "titlebar.h"
#include <QMainWindow>
#include <QMenuBar>
#include <QApplication>
#include <QComboBox>
#include <QSignalSpy>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QProgressBar>
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
        QMainWindow window;window.setWindowTitle("nxrs");auto* bar=new TitleBar(&window);window.setMenuWidget(bar);
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
        QCOMPARE(names,QStringList({"Play","Stop","Build","-","Install DLC…","Install Updates…","Copy saves from the nxrs build…","-","Remove from library…"}));
        auto* copy=window.findChild<QAction*>("CopySaves");QVERIFY(copy);QVERIFY(!copy->isEnabled());   // no other build, no saves
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
    void dlcWindowAddsAndRemovesSeveral(){
        QTemporaryDir tmp;Game game;game.id="g";game.title="Kart";game.outputName="Kart";game.buildDir=tmp.filePath("build");
        QDir().mkpath(game.dataDir());
        for(const auto& path:{game.executable(),game.codeModule()}){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write("fixture");}
        const QString first=tmp.filePath("First.nsp"),second=tmp.filePath("Second.nsz"),third=tmp.filePath("Third.xci"),more=tmp.filePath("More.nsp"),other=tmp.filePath("Other.nca");
        for(const auto& path:{first,second,third}){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write("PFS0");}
        {QFile f(game.dataDir()+"/dlc.manifest");QVERIFY(f.open(QIODevice::WriteOnly));
         f.write(("0100152000023001 65536 data "+first+"\n0100152000023002 3 data "+second+"\n0100152000023003 1 data "+second+"\n").toUtf8());}
        game.dlc={first,second,third};
        DlcDialog dialog(game,false);
        QCOMPARE(dialog.windowTitle(),QString("DLC — Kart"));
        QCOMPARE(dialog.list->count(),3);
        QCOMPARE(dialog.list->item(1)->text(),QString("Second.nsz — 0100152000023002 v3, 0100152000023003 v1"));   // one package, two titles
        QCOMPARE(dialog.list->item(2)->text(),QString("Third.xci — not installed yet"));
        QVERIFY(dialog.addButton->isEnabled());QVERIFY(!dialog.removeButton->isEnabled());                      // nothing selected yet
        QSignalSpy removed(&dialog,&DlcDialog::removeRequested),added(&dialog,&DlcDialog::addRequested);
        // two entries selected, one click: both are asked to go
        dialog.list->item(0)->setSelected(true);dialog.list->item(2)->setSelected(true);QVERIFY(dialog.removeButton->isEnabled());
        QTest::mouseClick(dialog.removeButton,Qt::LeftButton);
        QCOMPARE(removed.count(),1);QCOMPARE(removed[0][0].toStringList(),QStringList({first,third}));
        QCOMPARE(dialog.list->count(),3);                                                                         // the window only asks
        game.dlc={second};dialog.setGame(game);                                                                   // the main window's answer
        QCOMPARE(dialog.list->count(),1);QVERIFY(!dialog.removeButton->isEnabled());
        dialog.list->item(0)->setSelected(true);dialog.list->setFocus();dialog.show();QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QTest::keyClick(dialog.list,Qt::Key_Delete);QCOMPARE(removed.count(),2);QCOMPARE(removed[1][0].toStringList(),QStringList(second));
        // several packages in one go (the picker and a drop both end in addFiles)
        dialog.addFiles({more,other});QCOMPARE(added.count(),1);QCOMPARE(added[0][0].toStringList(),QStringList({more,other}));
        dialog.addFiles({});QCOMPARE(added.count(),1);
        // while the engine runs nothing else can be started, and the outcome shows under the list
        dialog.setBusy(true,"Installing…");QVERIFY(!dialog.addButton->isEnabled());QVERIFY(!dialog.removeButton->isEnabled());QVERIFY(!dialog.progress->isHidden());
        QCOMPARE(dialog.status->text(),QString("Installing…"));
        dialog.addFiles({more});dialog.removeSelected();QCOMPARE(added.count(),1);QCOMPARE(removed.count(),2);
        dialog.setBusy(false);QVERIFY(dialog.addButton->isEnabled());QVERIFY(dialog.progress->isHidden());
        dialog.setStatus("More.nsp: the ticket is bound to one console",true);
        QCOMPARE(dialog.status->text(),QString("More.nsp: the ticket is bound to one console"));QVERIFY(dialog.status->property("error").toBool());QVERIFY(!dialog.status->isHidden());
        dialog.setStatus("Installed: 0100152000023001 v65536.");QVERIFY(!dialog.status->property("error").toBool());
        game.dlc.clear();dialog.setGame(game);QVERIFY(dialog.hint->text().contains("Add DLC…"));QVERIFY(dialog.hint->text().contains("several at once"));
        // a running or building game: the list shows, nothing can be changed
        game.dlc={first};DlcDialog locked(game,true);locked.list->item(0)->setSelected(true);
        QVERIFY(!locked.addButton->isEnabled());QVERIFY(!locked.removeButton->isEnabled());
        QSignalSpy lockedRemoved(&locked,&DlcDialog::removeRequested);locked.removeSelected();locked.addFiles({more});QCOMPARE(lockedRemoved.count(),0);
        QVERIFY(locked.hint->text().contains("cannot be changed"));
    }
    void updatesWindowAddsRemovesAndChooses(){
        QTemporaryDir tmp;Game game;game.id="g";game.title="Arceus";game.outputName="Arceus";game.buildDir=tmp.filePath("build");
        game.source=tmp.filePath("Game [v0].nsp");game.builtSource=game.source;
        QDir().mkpath(game.dataDir());
        for(const auto& path:{game.executable(),game.codeModule(),game.source}){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write("PFS0");}
        const QString older=tmp.filePath("Update [v65536].nsp"),newer=tmp.filePath("Update [v262144].nsp"),more=tmp.filePath("More.xci");
        for(const auto& path:{older,newer}){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write("PFS0");}
        game.updates={{older,"01001f5010dfa000",65536,"1.0.1"},{newer,"01001f5010dfa000",262144,"1.1.1"}};
        game.builtUpdate=newer;QVERIFY(game.hasBuild());
        UpdateDialog dialog(game,false);
        QCOMPARE(dialog.windowTitle(),QString("Updates — Arceus"));
        QCOMPARE(dialog.list->count(),2);
        QCOMPARE(dialog.list->item(0)->text(),QString("Update [v65536].nsp — 1.0.1 (v65536)"));
        QCOMPARE(dialog.list->item(1)->text(),QString("Update [v262144].nsp — 1.1.1 (v262144) — applied"));
        // the version to build: newest by default, each installed update, or none
        QCOMPARE(dialog.choice->count(),4);QCOMPARE(dialog.choice->currentIndex(),0);
        QCOMPARE(dialog.choice->itemText(0),QString("Newest installed update — 1.1.1 (v262144)"));
        QCOMPARE(dialog.choice->itemData(1).toString(),older);QCOMPARE(dialog.choice->itemData(3).toString(),QString("none"));
        QVERIFY(dialog.pending->isHidden());QVERIFY(!dialog.buildButton->isEnabled());            // the build is what is chosen
        QSignalSpy removed(&dialog,&UpdateDialog::removeRequested),added(&dialog,&UpdateDialog::addRequested),
            chosen(&dialog,&UpdateDialog::choiceRequested),build(&dialog,&UpdateDialog::buildRequested);
        QVERIFY(!dialog.removeButton->isEnabled());
        dialog.list->item(0)->setSelected(true);dialog.list->item(1)->setSelected(true);QVERIFY(dialog.removeButton->isEnabled());
        QTest::mouseClick(dialog.removeButton,Qt::LeftButton);
        QCOMPARE(removed.count(),1);QCOMPARE(removed[0][0].toStringList(),QStringList({older,newer}));
        dialog.addFiles({more,older});QCOMPARE(added.count(),1);QCOMPARE(added[0][0].toStringList(),QStringList({more,older}));
        dialog.choice->setCurrentIndex(1);emit dialog.choice->activated(1);
        QCOMPARE(chosen.count(),1);QCOMPARE(chosen[0][0].toString(),older);
        // the main window answers: the older update is chosen now, so the build is behind and can be started here
        game.updateChoice=older;QVERIFY(game.updatePending());dialog.setGame(game);
        QCOMPARE(dialog.choice->currentIndex(),1);
        QCOMPARE(dialog.list->item(0)->text(),QString("Update [v65536].nsp — 1.0.1 (v65536) — selected, build the game to apply it"));
        QCOMPARE(dialog.list->item(1)->text(),QString("Update [v262144].nsp — 1.1.1 (v262144) — in the current build, not selected"));
        QVERIFY(!dialog.pending->isHidden());QVERIFY(dialog.pending->text().contains("update 1.1.1 (v262144)"));QVERIFY(dialog.pending->text().contains("update 1.0.1 (v65536)"));
        QVERIFY(dialog.buildButton->isEnabled());QTest::mouseClick(dialog.buildButton,Qt::LeftButton);QCOMPARE(build.count(),1);
        // busy and its outcome
        dialog.setBusy(true,"Checking the update against the game…");QVERIFY(!dialog.addButton->isEnabled());QVERIFY(!dialog.choice->isEnabled());QVERIFY(!dialog.buildButton->isEnabled());
        dialog.addFiles({more});QCOMPARE(added.count(),1);
        dialog.setBusy(false);dialog.setStatus("More.xci: this update is for 0100152000022000, not for this game",true);
        QVERIFY(dialog.status->property("error").toBool());QVERIFY(dialog.addButton->isEnabled());
        // none left: the game without updates
        game.updates.clear();game.updateChoice.clear();dialog.setGame(game);
        QCOMPARE(dialog.list->count(),0);QCOMPARE(dialog.choice->count(),2);QVERIFY(dialog.hint->text().contains("Add update…"));
        QVERIFY(dialog.pending->text().contains("the game as its package is"));
        // a running or building game: read-only
        game.updates={{newer,"01001f5010dfa000",262144,"1.1.1"}};UpdateDialog locked(game,true);locked.list->item(0)->setSelected(true);
        QVERIFY(!locked.addButton->isEnabled());QVERIFY(!locked.removeButton->isEnabled());QVERIFY(!locked.choice->isEnabled());
        QSignalSpy lockedRemoved(&locked,&UpdateDialog::removeRequested);locked.removeSelected();QCOMPARE(lockedRemoved.count(),0);
        // Properties > Updates: the same list, removal applied by the caller
        UpdatePanel panel(game,false);QCOMPARE(panel.list->count(),1);QVERIFY(panel.list->item(0)->text().endsWith("— applied"));
        panel.removeRow(0);QVERIFY(panel.updates().isEmpty());QVERIFY(panel.note->text().contains("Game > Install Updates"));
        UpdatePanel fixed(game,true);fixed.removeRow(0);QCOMPARE(fixed.updates().size(),1);
    }
    void escapeClosesPopup(){
        QWidget owner;QPushButton anchor("Find games",&owner);owner.resize(400,300);anchor.resize(100,70);owner.show();
        FilterPopup popup(&owner);popup.openBelow(&anchor);QTRY_VERIFY(popup.isVisible());
        QVERIFY(!popup.search->accessibleName().isEmpty());QTest::keyClick(popup.search,Qt::Key_Escape);QTRY_VERIFY(!popup.isVisible());
    }
};
QTEST_MAIN(FilterPopupTests)
#include "gui_tests.moc"
