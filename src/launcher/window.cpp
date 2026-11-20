// nxrs - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "window.h"
#include "artwork.h"
#include "dlcpanel.h"
#include "updatepanel.h"
#include "filterpopup.h"
#include "theme.h"
#include "titlebar.h"
#include "processwatch.h"
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
QLabel* textLabel(const QString& text, const char* name = "") {
    auto* label = new QLabel(text); label->setTextFormat(Qt::PlainText); label->setObjectName(name); return label;
}
QIcon commandIcon(const QString& kind) {
    QPixmap image(128,128); image.fill(Qt::transparent);
    QPainter p(&image); p.setRenderHint(QPainter::Antialiasing); p.scale(4,4);
    const auto& theme=Theme::colors();
    p.setPen(QPen(theme.text,1.65,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if(kind=="add"||kind=="folder") {
        QPainterPath path; path.moveTo(3,10);path.lineTo(3,7);path.quadTo(3,5,5,5);path.lineTo(12,5);path.lineTo(15,8);path.lineTo(26,8);path.quadTo(28,8,28,10);path.lineTo(28,25);path.lineTo(3,25);path.closeSubpath();
        p.fillPath(path,theme.raised);p.setPen(QPen(theme.text,1.4));p.drawPath(path);
        p.setPen(QPen(theme.accent,1.8));
        if(kind=="add"){p.drawLine(11,17,21,17);p.drawLine(16,12,16,22);}
        else {p.drawLine(11,17,21,17);p.drawLine(18,14,21,17);p.drawLine(18,20,21,17);}
    } else if(kind=="play") {
        p.setPen(QPen(theme.accent,1.7));p.setBrush(theme.selected);p.drawEllipse(QRectF(3,3,26,26));
        QPainterPath triangle;triangle.moveTo(13,10);triangle.lineTo(22,16);triangle.lineTo(13,22);triangle.closeSubpath();p.fillPath(triangle,theme.accent);
    } else if(kind=="build") {
        p.setPen(QPen(theme.text,1.6));
        QPainterPath tool;tool.moveTo(25,4);tool.lineTo(20,9);tool.lineTo(23,12);tool.lineTo(28,7);tool.cubicTo(31,14,24,19,19,15);tool.lineTo(8,27);tool.cubicTo(4,30,1,26,5,23);tool.lineTo(16,12);tool.cubicTo(12,7,18,1,25,4);p.drawPath(tool);
        p.drawEllipse(QPointF(7,25),1,1);
    } else if(kind=="stop") {
        p.setPen(QPen(theme.text,1.6));p.setBrush(theme.raised);p.drawRoundedRect(QRectF(6,6,20,20),3,3);
    } else if(kind=="settings") {
        QPainterPath gear;
        for(int i=0;i<32;++i){const double angle=(i*2.0*3.141592653589793/32.0)-3.141592653589793/2;const double radius=(i%4==1||i%4==2)?13:10;
            const QPointF pt(16+std::cos(angle)*radius,16+std::sin(angle)*radius);if(i==0)gear.moveTo(pt);else gear.lineTo(pt);}
        gear.closeSubpath();p.setPen(QPen(theme.text,1.5));p.setBrush(theme.raised);p.drawPath(gear);p.drawEllipse(QRectF(11,11,10,10));
    } else if(kind=="properties") {
        p.setPen(QPen(theme.text,1.8));
        p.drawLine(5,8,27,8);p.drawLine(5,16,27,16);p.drawLine(5,24,27,24);
        p.setBrush(theme.raised);p.drawRoundedRect(QRectF(10,5,5,6),1,1);p.drawRoundedRect(QRectF(21,13,5,6),1,1);p.drawRoundedRect(QRectF(7,21,5,6),1,1);
    } else if(kind=="grid") {
        p.setBrush(theme.raised);for(int y:{5,18})for(int x:{5,18})p.drawRoundedRect(QRectF(x,y,9,9),1,1);
    } else if(kind=="list") {
        for(int y:{7,16,25}){p.drawRect(QRectF(4,y-2,4,4));p.drawLine(13,y,28,y);}
    } else if(kind=="log") {
        p.drawRoundedRect(QRectF(6,3,20,26),2,2);for(int y:{10,16,22})p.drawLine(11,y,21,y);
    } else if(kind=="import") {
        p.drawRoundedRect(QRectF(4,5,24,22),3,3);p.drawLine(4,11,28,11);
        p.setPen(QPen(theme.accent,1.8));p.drawLine(16,14,16,24);p.drawLine(12,20,16,24);p.drawLine(20,20,16,24);
    } else if(kind=="filter") {
        p.drawEllipse(QRectF(4,3,16,16));p.drawLine(18,17,27,26);
        p.setPen(QPen(theme.accent,1.5));p.drawLine(3,24,13,24);p.drawLine(6,28,13,28);
    }
    return QIcon(image);
}
QString displayPath(const QString& path) { return path.isEmpty() ? QString("Not linked") : QDir::toNativeSeparators(path); }
QWidget* browseField(QLineEdit* edit, QWidget* parent, const QString& title, const QString& filter, bool folder=false) {
    auto* row=new QWidget;auto* layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);layout->setSpacing(5);
    auto* button=new QPushButton("…");button->setFixedWidth(30);button->setToolTip(title);layout->addWidget(edit,1);layout->addWidget(button);
    QObject::connect(button,&QPushButton::clicked,parent,[=]{
        const QString path=folder?QFileDialog::getExistingDirectory(parent,title,edit->text()):QFileDialog::getOpenFileName(parent,title,edit->text(),filter);
        if(!path.isEmpty())edit->setText(QDir::toNativeSeparators(path));
    });return row;
}
QFormLayout* tabForm(QTabWidget* tabs,const QString& name) {
    auto* page=new QWidget;auto* form=new QFormLayout(page);form->setContentsMargins(18,22,18,20);form->setHorizontalSpacing(15);form->setVerticalSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);tabs->addTab(page,name);return form;
}
}

Window::Window(const QString& file,const QString& root,bool isDemo):library(file),builder(this),dlcInstaller(this),updateProbe(this),demo(isDemo) {
    setWindowTitle("nxrs");resize(1280,800);setMinimumSize(920,600);setAcceptDrops(true);
    setDockOptions(QMainWindow::AllowTabbedDocks|QMainWindow::AnimatedDocks);
    QString error;library.load(&error);
    const QString installedKeys=QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("keys/prod.keys");
    if(library.keysPath.isEmpty()&&QFileInfo(installedKeys).isFile())library.keysPath=installedKeys;
    resolveToolRoot(library,Toolchain::discoverRoot());
    if(!root.isEmpty())library.toolRoot=QFileInfo(root).absoluteFilePath();
    Theme::apply(*qApp,library.colorMode,library.accentColor);
    if(demo)seedDemo();
    if(!demo)running=findRunningGames(library.games);
    createActions();createMenusAndToolbar();createLibrary();createBuildDock();createStatusBar();
    if(!demo&&!library.geometry.isEmpty())restoreGeometry(library.geometry);
    if(!demo&&!library.dockState.isEmpty())restoreState(library.dockState,1);
    setViewMode(library.viewMode);

    connect(&builder,&BuildManager::output,console,&QPlainTextEdit::appendPlainText);
    connect(&builder,&BuildManager::progress,this,[this](const QString& phase,int current,int total){
        buildPhase=phase;buildCurrent=current;buildTotal=total;
        if(auto* card=cards.value(builder.gameId()))card->setBuildProgress(phase,current,total);
        jobLabel->setText(total?QString("%1  %2/%3").arg(phase).arg(current).arg(total):phase);
        progress->setVisible(true);progress->setRange(0,total);progress->setValue(current);
    });
    connect(&builder,&BuildManager::finished,this,[this](const QString& id,bool okay,const QString& detail){
        if(auto* game=library.find(id)){
            if(okay){
                // a package with the game and its DLC in one file: the first build from it lists it as the game's DLC too
                if(builder.sourceHoldsDlc()&&adoptPackageDlc(*game,game->builtSource))console->appendPlainText("DLC: "+QFileInfo(game->source).fileName()+" is now on this game's DLC list");
                game->builtSource=game->source;game->builtUpdate=builder.appliedUpdate();game->buildIncomplete=false;game->lastBuilt=QDateTime::currentDateTime();game->hintsBuilt=builder.hintsAtStart;
            }
            save();
        }
        console->appendPlainText("\n"+detail);buildDock->setWindowTitle(okay?"Build output — complete":"Build output — stopped");
        progress->hide();jobLabel->setText(okay?"Build complete — ready to play":"Build stopped — see output");
        refreshLibrary();
        if(closing){emit smokeFinished(okay);QTimer::singleShot(0,this,&QWidget::close);return;}
        // a fresh build gets the game's DLC put (back) into its data folder; --build waits for that too
        const auto* built=library.find(id);
        if(okay&&built&&!built->dlc.isEmpty()&&startDlcSync(id,DlcBuilt)){buildDlcPending=true;return;}
        emit smokeFinished(okay);
    });
    connect(&updateProbe,&UpdateProbe::output,console,&QPlainTextEdit::appendPlainText);
    connect(&updateProbe,&UpdateProbe::finished,this,[this](const QString& id,const QVector<GameUpdate>& found,const QStringList& errors){
        progress->hide();jobLabel->clear();
        auto* game=library.find(id);if(!game){if(updateDialog)updateDialog->setBusy(false);return;}
        QStringList added;
        for(const auto& update:found){
            bool known=false;for(auto& have:game->updates)if(samePath(have.path,update.path)){have=update;known=true;}
            if(!known)game->updates.append(update);
            added<<update.label();
        }
        if(!found.isEmpty()){save();refreshLibrary();}
        if(updateDialog){updateDialog->setBusy(false);updateDialog->setGame(*game);}
        QString text=errors.join("\n\n");
        if(!added.isEmpty())text+=(text.isEmpty()?QString{}:"\n\n")+"Added: "+added.join(", ")+". "+updateOutcome(*game);
        updateReport(errors.isEmpty()?"Update added":"Update could not be added",text,!errors.isEmpty());
    });
    connect(&dlcInstaller,&DlcInstaller::output,console,&QPlainTextEdit::appendPlainText);
    connect(&dlcInstaller,&DlcInstaller::finished,this,[this](const QString& id,bool,const QStringList& errors){
        progress->hide();
        auto* game=library.find(id);if(!game){jobLabel->clear();if(dlcDialog)dlcDialog->setBusy(false);refreshLibrary();return;}
        // what the data folder holds now is the truth: a package the engine refused leaves the list
        const auto installed=readDlcManifest(game->dataDir());
        QStringList kept,titles;
        for(const auto& package:game->dlc){bool found=false;for(const auto& title:installed)found=found||samePath(title.source,package);if(found)kept<<package;}
        for(const auto& title:installed)titles<<title.titleId.toUpper()+(title.version?QString(" v%1").arg(title.version):QString{});
        game->dlc=kept;save();refreshLibrary();
        if(dlcReason==DlcBuilt&&buildDlcPending){buildDlcPending=false;emit smokeFinished(errors.isEmpty());}
        if(dlcDialog){dlcDialog->setBusy(false);dlcDialog->setGame(*game);}
        if(!errors.isEmpty()){jobLabel->setText("DLC was not installed — see the message");dlcReport("DLC could not be installed",errors.join("\n\n"),true);return;}
        jobLabel->setText(installed.isEmpty()?"DLC removed":QString("DLC installed — %1 title%2").arg(installed.size()).arg(installed.size()==1?"":"s"));
        if(dlcReason==DlcInstalled&&!installed.isEmpty())
            dlcReport("DLC installed",dlcDialog||dlcQuiet?"Installed: "+titles.join(", ")+". It is in effect the next time you play."
                :game->title+" now has this add-on content:\n\n"+titles.join("\n")+"\n\nIt is in effect the next time you play. Game > Install DLC… lists it and removes it.",false);
        else if(dlcReason==DlcChanged&&dlcDialog)
            dlcDialog->setStatus(installed.isEmpty()?"Removed. No DLC is installed now; the original files were not changed."
                :"Removed. Still installed: "+titles.join(", ")+". The original files were not changed.");
        else if(dlcQuiet)dlcReport({},installed.isEmpty()?QString("No DLC is installed now."):"Installed: "+titles.join(", "),false);
        dlcQuiet=false;
        if(dlcReason==DlcPlay&&dlcUpToDate(*game)){selectGame(id);play();}
    });
    new QShortcut(QKeySequence("Ctrl+F"),this,[this]{showFilters();});
    auto* galleryEnter=new QShortcut(QKeySequence(Qt::Key_Return),gallery,[this]{activateGame();});galleryEnter->setContext(Qt::WidgetShortcut);
    auto* tableEnter=new QShortcut(QKeySequence(Qt::Key_Return),table,[this]{activateGame();});tableEnter->setContext(Qt::WidgetShortcut);
    auto* poll=new QTimer(this);poll->setInterval(2500);
    connect(poll,&QTimer::timeout,this,[this]{
        if(!demo){const auto detected=findRunningGames(library.games);if(detected!=running){
            running=detected;refreshLibrary();}}
    });poll->start();
    refreshTools();refreshLibrary();
#ifdef Q_OS_WIN
    // Ask Windows for a dark native caption while retaining its standard window controls.
    if(HMODULE dwm=LoadLibraryW(L"dwmapi.dll")){
        using SetAttribute=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
        auto set=reinterpret_cast<SetAttribute>(GetProcAddress(dwm,"DwmSetWindowAttribute"));
        BOOL dark=library.colorMode!="light";if(set)set(reinterpret_cast<HWND>(winId()),20,&dark,sizeof dark);FreeLibrary(dwm);
    }
#endif
    if(!error.isEmpty())QTimer::singleShot(0,this,[this,error]{message("Library could not be loaded",error,true);});
}

void Window::createActions(){
    auto make=[this](const QString& name,const QString& icon,const QString& tip){auto* a=new QAction(commandIcon(icon),name,this);a->setProperty("iconKind",icon);a->setToolTip(tip);return a;};
    addAction=make("Add game","add","Add a NUP/NSP package or NRO (Ctrl+O)");addAction->setShortcut(QKeySequence("Ctrl+O"));
    connect(addAction,&QAction::triggered,this,[this]{for(const auto& p:QFileDialog::getOpenFileNames(this,"Add game",{},"Game packages (*.nup *.nsp *.xci *.nro)"))importSource(p);});
    addFolderAction=make("Add extracted folder…","folder","Add a folder containing ExeFS and RomFS");
    connect(addFolderAction,&QAction::triggered,this,[this]{auto p=QFileDialog::getExistingDirectory(this,"Add extracted game folder");if(!p.isEmpty())importSource(p);});
    importAction=make("Import build…","import","Add an already compiled game to your library");
    connect(importAction,&QAction::triggered,this,[this]{auto p=QFileDialog::getOpenFileName(this,"Import native build",{},QString("Native games (*%1)").arg(executableSuffix));if(!p.isEmpty())importBuild(p);});
    playAction=make("Play","play","Launch the selected game");connect(playAction,&QAction::triggered,this,&Window::play);
    buildAction=make("Build","build","Build the selected game using saved options (Ctrl+B)");buildAction->setShortcut(QKeySequence("Ctrl+B"));connect(buildAction,&QAction::triggered,this,&Window::startBuild);
    stopAction=make("Stop","stop","Cancel the active build or close the selected running game");connect(stopAction,&QAction::triggered,this,&Window::stop);
    propertiesAction=make("Properties","properties","Game files, build options, renderer, controller layout, and artwork (Alt+Enter)");propertiesAction->setShortcut(QKeySequence("Alt+Return"));connect(propertiesAction,&QAction::triggered,this,[this]{showProperties();});
    buildOptionsAction=new QAction("Build options…",this);connect(buildOptionsAction,&QAction::triggered,this,[this]{showProperties(1);});
    settingsAction=make("Settings","settings","Engine paths, defaults, controller layout, and appearance");connect(settingsAction,&QAction::triggered,this,[this]{showSettings();});
    installKeysAction=new QAction("Install keys…",this);connect(installKeysAction,&QAction::triggered,this,&Window::installKeys);
    folderAction=make("Open build folder","folder","Open generated game files");connect(folderAction,&QAction::triggered,this,[this]{openFolder();});
    saveFolderAction=new QAction("Open save folder",this);connect(saveFolderAction,&QAction::triggered,this,[this]{openFolder(true);});
    copySavesAction=new QAction("Copy saves from the Turbo build…",this);copySavesAction->setObjectName("CopySaves");
    copySavesAction->setToolTip("Copy this game's saves from its build with the other engine (this build's saves are backed up first)");
    connect(copySavesAction,&QAction::triggered,this,[this]{copySaves();});
    logAction=make("View game log","log","Open the runtime log");connect(logAction,&QAction::triggered,this,[this]{openGameFile(".log");});
    removeAction=new QAction("Remove from library…",this);connect(removeAction,&QAction::triggered,this,&Window::removeGame);
    installDlcAction=new QAction("Install DLC…",this);installDlcAction->setObjectName("InstallDlc");
    installDlcAction->setToolTip("Install or remove add-on content for the selected game (NSP, NSZ, XCI, XCZ, NCA)");connect(installDlcAction,&QAction::triggered,this,&Window::installDlc);
    installUpdatesAction=new QAction("Install Updates…",this);installUpdatesAction->setObjectName("InstallUpdates");
    installUpdatesAction->setToolTip("Install or remove game updates for the selected game (NSP, XCI, NCA) and choose the version to build");connect(installUpdatesAction,&QAction::triggered,this,&Window::installUpdates);
    auto* group=new QActionGroup(this);group->setExclusive(true);
    galleryAction=make("Gallery","grid","Cover gallery");tableAction=make("List","list","Sortable details list");
    for(auto* a:{galleryAction,tableAction}){a->setCheckable(true);group->addAction(a);}
    connect(galleryAction,&QAction::triggered,this,[this]{setViewMode(0);save();});
    connect(tableAction,&QAction::triggered,this,[this]{setViewMode(1);save();});
    backgroundAction=new QAction("Show selected game background",this);backgroundAction->setCheckable(true);backgroundAction->setChecked(library.showBackground);
    connect(backgroundAction,&QAction::toggled,this,[this](bool on){library.showBackground=on;refreshSelection();save();});
    filterAction=make("Search & Filter","filter","Search, filter, and choose the library view (Ctrl+F)");
    connect(filterAction,&QAction::triggered,this,[this]{if(filters&&filters->isVisible())filters->hide();else showFilters();});
}

void Window::createMenusAndToolbar(){
    // The menus share the client-drawn title bar row (VS Code-style); QMainWindow::menuBar() must not be called,
    // it would replace the title bar with a new QMenuBar.
    titleBar=new TitleBar(this);menus=titleBar->menuBar();setMenuWidget(titleBar);
    auto* file=menus->addMenu("&File");file->addAction(addAction);file->addAction(addFolderAction);file->addAction(importAction);
    file->addSeparator();file->addAction(settingsAction);
    file->addSeparator();file->addAction("Exit",QKeySequence("Alt+F4"),this,&QWidget::close);
    auto* game=menus->addMenu("&Game");game->addAction(playAction);game->addAction(stopAction);game->addAction(buildAction);game->addSeparator();game->addAction(installDlcAction);game->addAction(installUpdatesAction);game->addAction(copySavesAction);game->addSeparator();game->addAction(removeAction);
    auto* keys=menus->addMenu("&Keys");keys->addAction(installKeysAction);keys->addAction("Open keys folder",this,[this]{const auto path=QFileInfo(library.keysPath).absolutePath();if(QDir(path).exists())QDesktopServices::openUrl(QUrl::fromLocalFile(path));});
    auto* view=menus->addMenu("&View");view->setObjectName("ViewMenu");view->addAction(galleryAction);view->addAction(tableAction);view->addSeparator();view->addAction(filterAction);view->addAction(backgroundAction);
    view->addAction("Refresh library",QKeySequence("F5"),this,[this]{invalidateArtwork();refreshTools();refreshLibrary();});
    view->addSeparator();view->addAction("View console",QKeySequence("Ctrl+L"),this,&Window::showBuildOutput);
    auto* help=menus->addMenu("&Help");help->addAction("About nxrs",this,[this]{message("nxrs","Build and launch your own game dumps from one library.\nNo games, keys, or firmware are included.\n\nEngine: Switch2RECOMP (AGPL-3.0) — see legal/LICENSE.txt in the release (third_party/Switch2RECOMP/LICENSE.txt in a source checkout).\n\nNative controller input is supported. Vibration and some Switch 2 audio/video features remain incomplete.");});

    auto* bar=addToolBar("Commands");bar->setObjectName("Commands");bar->setMovable(false);bar->setIconSize({32,32});bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    auto* addButton=new QToolButton;addButton->setObjectName("AddGameButton");addButton->setDefaultAction(addAction);addButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);addButton->setIconSize({32,32});
    auto* menu=new QMenu(addButton);menu->addAction(addAction);menu->addAction(addFolderAction);menu->addAction(importAction);addButton->setMenu(menu);addButton->setPopupMode(QToolButton::MenuButtonPopup);bar->addWidget(addButton);
    auto* spacer=new QWidget;spacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);bar->addWidget(spacer);
    bar->addAction(filterAction);filterAnchor=bar->widgetForAction(filterAction);
    for(auto* button:bar->findChildren<QToolButton*>())button->setFocusPolicy(Qt::StrongFocus);
    filters=new FilterPopup(this);search=filters->search;sort=filters->sort;
    filters->galleryButton->setDefaultAction(galleryAction);filters->listButton->setDefaultAction(tableAction);
    coverSize=filters->coverSize;coverSize->setValue(library.coverWidth);
    connect(coverSize,&QSlider::valueChanged,this,[this](int size){
        library.coverWidth=size;if(!gallery)return;gallery->setGridSize({size+12,GameCard::heightFor(size)+12});
        for(int i=0;i<gallery->count();++i)gallery->item(i)->setSizeHint({size,GameCard::heightFor(size)});
        for(auto* card:cards)card->setCardWidth(size);gallery->doItemsLayout();
    });
    connect(coverSize,&QSlider::sliderReleased,this,&Window::save);
    connect(filters,&FilterPopup::filtersChanged,this,[this]{
        if(table){if(sort->currentIndex()==0)table->sortItems(0,Qt::AscendingOrder);else if(sort->currentIndex()==1)table->sortItems(4,Qt::DescendingOrder);else if(sort->currentIndex()==2)table->sortItems(1,Qt::AscendingOrder);}
        refreshLibrary();
    });
}

void Window::createLibrary(){
    canvas=new LibraryCanvas;setCentralWidget(canvas);auto* layout=new QVBoxLayout(canvas);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
    views=new QStackedWidget;views->setStyleSheet("QStackedWidget { background: transparent; }");layout->addWidget(views,1);
    gallery=new QListWidget;gallery->setViewMode(QListView::IconMode);gallery->setResizeMode(QListView::Adjust);gallery->setMovement(QListView::Static);
    gallery->setAccessibleName("Game library gallery");
    gallery->setWrapping(true);gallery->setSpacing(8);gallery->setSelectionMode(QAbstractItemView::SingleSelection);gallery->setMouseTracking(true);gallery->setStyleSheet("QListWidget {background:transparent;padding:8px;}");
    gallery->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);gallery->viewport()->setAutoFillBackground(false);
    gallery->setItemDelegate(new GalleryDelegate(gallery));gallery->setGridSize({library.coverWidth+12,GameCard::heightFor(library.coverWidth)+12});
    gallery->setContextMenuPolicy(Qt::CustomContextMenu);views->addWidget(gallery);
    table=new QTreeWidget;table->setColumnCount(6);table->setHeaderLabels({"Name","Status","Format","Renderer","Last played","Build folder"});
    table->setAccessibleName("Game library details");
    table->setItemDelegate(new TableDelegate(table));
    table->setRootIsDecorated(false);table->setUniformRowHeights(true);table->setAlternatingRowColors(true);table->setSelectionMode(QAbstractItemView::SingleSelection);table->setSortingEnabled(true);table->sortItems(0,Qt::AscendingOrder);
    table->setColumnWidth(0,260);table->setColumnWidth(1,130);table->setColumnWidth(2,110);table->setColumnWidth(3,80);table->setColumnWidth(4,150);table->header()->setStretchLastSection(true);
    connect(table->header(),&QHeaderView::sortIndicatorChanged,this,[this](int column,Qt::SortOrder order){
        const int preset=column==0&&order==Qt::AscendingOrder?0:column==4&&order==Qt::DescendingOrder?1:column==1&&order==Qt::AscendingOrder?2:-1;
        QSignalBlocker block(sort);sort->setCurrentIndex(preset);
    });
    table->setContextMenuPolicy(Qt::CustomContextMenu);views->addWidget(table);
    empty=textLabel("Your library is empty.\n\nAdd a NUP or NRO, import an existing build,\nor drop a game package into this window.","Empty");empty->setAlignment(Qt::AlignCenter);layout->addWidget(empty,1);
    connect(gallery,&QListWidget::currentItemChanged,this,[this](QListWidgetItem* item){if(item)selectGame(item->data(Qt::UserRole).toString());});
    connect(table,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem* item){if(item)selectGame(item->data(0,Qt::UserRole).toString());});
    connect(gallery,&QListWidget::itemDoubleClicked,this,[this]{activateGame();});connect(table,&QTreeWidget::itemDoubleClicked,this,[this]{activateGame();});
    connect(gallery,&QListWidget::customContextMenuRequested,this,[this](QPoint p){if(auto* item=gallery->itemAt(p)){gallery->setCurrentItem(item);showContextMenu(gallery->viewport()->mapToGlobal(p));}});
    connect(table,&QTreeWidget::customContextMenuRequested,this,[this](QPoint p){if(auto* item=table->itemAt(p)){table->setCurrentItem(item);showContextMenu(table->viewport()->mapToGlobal(p));}});
}

void Window::createBuildDock(){
    buildDock=new QDockWidget("Build output",this);buildDock->setObjectName("BuildOutput");buildDock->setAllowedAreas(Qt::BottomDockWidgetArea|Qt::RightDockWidgetArea);
    auto* page=new QWidget;auto* layout=new QVBoxLayout(page);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
    auto* toolsBar=new QToolBar;toolsBar->setIconSize({16,16});
    toolsBar->addAction("Open log",this,[this]{const auto* g=library.find(builder.gameId());if(!g)g=selected();if(g)QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(g->buildDir).filePath(g->outputName+".build.log")));});
    toolsBar->addAction("Clear view",this,[this]{console->clear();});layout->addWidget(toolsBar);
    console=new QPlainTextEdit;console->setReadOnly(true);console->setMaximumBlockCount(12000);console->setPlaceholderText("Build output appears here. Select a game and choose Build to begin.");layout->addWidget(console,1);
    buildDock->setWidget(page);addDockWidget(Qt::BottomDockWidgetArea,buildDock);buildDock->hide();
    if(auto* view=menus->findChild<QMenu*>("ViewMenu"))view->addAction(buildDock->toggleViewAction());
}
void Window::createStatusBar(){
    statusBar()->setSizeGripEnabled(true);
    countLabel=textLabel("0 games","Muted");countLabel->setMinimumWidth(65);statusBar()->addWidget(countLabel);
    progress=new QProgressBar;progress->setAccessibleName("Native build progress");progress->setFixedWidth(150);progress->setTextVisible(false);progress->hide();statusBar()->addWidget(progress);
    selectionLabel=textLabel("No selection");selectionLabel->setMinimumWidth(100);selectionLabel->setMaximumWidth(410);statusBar()->addWidget(selectionLabel,1);
    jobLabel=textLabel("","Muted");jobLabel->setMaximumWidth(320);statusBar()->addPermanentWidget(jobLabel);
    toolStatus=textLabel("Checking…","ToolBadge");toolStatus->setCursor(Qt::PointingHandCursor);toolStatus->installEventFilter(this);statusBar()->addPermanentWidget(toolStatus);
    auto* settings=new QToolButton;settings->setObjectName("StatusSettings");settings->setDefaultAction(settingsAction);settings->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);settings->setIconSize({13,13});statusBar()->addPermanentWidget(settings);
}

Game* Window::selected(){return library.find(selectedId);}
QString Window::statusFor(const Game& g)const{
    if(demo)return "Preview";
    if(builder.busy()&&builder.gameId()==g.id)return "Building";
    if(running.contains(g.id))return "Running";
    if(g.hasBuild()&&g.source.endsWith(".nup",Qt::CaseInsensitive)&&!g.sourceAvailable())return "Source offline";
    if(g.updatePending())return "Update changed — build again";
    return g.hasBuild()?"Ready to play":"Build needed";
}
void Window::save(){if(demo)return;QString error;if(!library.save(&error))message("Could not save library",error,true);}
void Window::message(const QString& title,const QString& text,bool error){if(error)QMessageBox::warning(this,title,text);else QMessageBox::information(this,title,text);}
void Window::refreshLibrary(){
    if(!gallery)return;
    QSignalBlocker a(gallery),b(table);cards.clear();gallery->clear();table->clear();table->setSortingEnabled(false);
    LibraryFilter query{search->text(),filters->state->currentIndex(),filters->source->currentIndex()};
    QVector<const Game*> games;for(const auto& g:library.games)if(query.matches(g,running.contains(g.id),builder.busy()&&builder.gameId()==g.id))games.append(&g);
    std::sort(games.begin(),games.end(),[this](const Game* x,const Game* y){
        if(sort->currentIndex()==1&&x->lastPlayed!=y->lastPlayed)return x->lastPlayed>y->lastPlayed;
        if(sort->currentIndex()==2&&statusFor(*x)!=statusFor(*y))return statusFor(*x)<statusFor(*y);
        return QString::localeAwareCompare(x->title,y->title)<0;
    });
    bool found=false;
    for(const Game* g:games){
        const auto status=statusFor(*g);auto* item=new QListWidgetItem;item->setData(Qt::UserRole,g->id);
        item->setText(g->title);item->setData(Qt::AccessibleTextRole,g->title+", "+status);
        item->setData(Qt::AccessibleDescriptionRole,g->format()+", "+g->renderer);
        item->setSizeHint({library.coverWidth,GameCard::heightFor(library.coverWidth)});
        item->setToolTip(g->title+"\n"+status+"\n"+displayPath(g->buildDir));gallery->addItem(item);
        auto state=cardState(*g,running.contains(g->id),builder.busy()&&builder.gameId()==g->id,builder.busy(),tools.canBuild(g->engine));
        if(demo)state.actionEnabled=false;
        auto* card=new GameCard(*g,state,library.coverWidth);cards.insert(g->id,card);gallery->setItemWidget(item,card);
        if(state.building)card->setBuildProgress(buildPhase,buildCurrent,buildTotal);
        connect(card,&GameCard::selected,this,&Window::selectGame);
        connect(card,&GameCard::primaryRequested,this,[this](const QString& id){QTimer::singleShot(0,this,[this,id]{cardAction(id);});});
        connect(card,&GameCard::propertiesRequested,this,[this](const QString& id){QTimer::singleShot(0,this,[this,id]{selectGame(id);showProperties();});});
        auto* row=new QTreeWidgetItem({g->title,status,g->format(),g->renderer,g->lastPlayed.isValid()?g->lastPlayed.toLocalTime().toString("yyyy-MM-dd HH:mm"):QString("—"),displayPath(g->buildDir)});
        row->setData(0,Qt::UserRole,g->id);row->setToolTip(5,displayPath(g->buildDir));table->addTopLevelItem(row);
        if(g->id==selectedId){gallery->setCurrentItem(item);table->setCurrentItem(row);found=true;}
    }
    if(!found){selectedId=games.isEmpty()?QString{}:games.front()->id;if(!games.isEmpty()){gallery->setCurrentRow(0);table->setCurrentItem(table->topLevelItem(0));}}
    table->setSortingEnabled(true);
    empty->setVisible(games.isEmpty());views->setVisible(!games.isEmpty());
    empty->setText(library.games.isEmpty()?"Your library is empty.\n\nAdd a NUP or NRO, import an existing build,\nor drop a game package into this window.":"No games match these filters.\nPress Ctrl+F to adjust or clear them.");
    filters->setResultCount(int(games.size()),int(library.games.size()));
    filterAction->setText(query.activeCount()?QString("Search && Filter (%1)").arg(query.activeCount()):"Search && Filter");
    filterAction->setToolTip(query.activeCount()?QString("%1 active filters — Ctrl+F to edit or clear").arg(query.activeCount()):"Search, filter, and sort your library (Ctrl+F)");
    countLabel->setText(query.activeCount()?QString("%1 / %2 games").arg(games.size()).arg(library.games.size()):QString("%1 game%2").arg(library.games.size()).arg(library.games.size()==1?"":"s"));refreshSelection();
}
void Window::selectGame(const QString& id){
    selectedId=id;QSignalBlocker a(gallery),b(table);
    for(int i=0;i<gallery->count();++i)if(gallery->item(i)->data(Qt::UserRole).toString()==id)gallery->setCurrentRow(i);
    for(int i=0;i<table->topLevelItemCount();++i)if(table->topLevelItem(i)->data(0,Qt::UserRole).toString()==id)table->setCurrentItem(table->topLevelItem(i));
    refreshSelection();
}
void Window::selectTitle(const QString& title){for(const auto& game:library.games)if(game.title.compare(title,Qt::CaseInsensitive)==0){selectGame(game.id);return;}}
bool Window::setEngine(const QString& engine){
    auto* game=selected();if(!game||demo||(engine!=engineTurbo&&engine!=engineNxrs))return false;
    if(running.contains(game->id)||(builder.busy()&&builder.gameId()==game->id))return false;
    if(setGameEngine(*game,engine)){save();refreshTools();refreshLibrary();}
    return true;
}
void Window::setNxrsBuild(const QString& path){
    library.nxrsBuild=path.isEmpty()?QString{}:QFileInfo(path).absoluteFilePath();save();refreshTools();refreshLibrary();
}
void Window::refreshSelection(){
    if(!selectionLabel)return;
    const auto* g=selected();const bool exists=g;const bool playing=exists&&running.contains(g->id);const bool building=exists&&builder.busy()&&builder.gameId()==g->id;
    playAction->setEnabled(exists&&!demo&&g->hasBuild()&&!playing&&!building);
    buildAction->setEnabled(exists&&!demo&&!builder.busy()&&!playing&&g->sourceAvailable()&&tools.canBuild(g->engine)&&!g->hasBuild());
    stopAction->setEnabled(building||playing);stopAction->setText(building?"Cancel build":"Stop");
    propertiesAction->setEnabled(exists);buildOptionsAction->setEnabled(exists&&!playing&&!building);removeAction->setEnabled(exists&&!playing&&!building);
    installDlcAction->setEnabled(exists&&!demo);   // the DLC window opens read-only for a game that is running or building
    installUpdatesAction->setEnabled(exists&&!demo);
    buildAction->setText(exists&&g->updatePending()?"Rebuild":"Build");
    folderAction->setEnabled(exists);saveFolderAction->setEnabled(exists&&g->hasBuild());
    copySavesAction->setText(exists&&g->nxrs()?"Copy saves from the Turbo build…":"Copy saves from the nxrs build…");
    copySavesAction->setEnabled(exists&&!demo&&g->hasBuild()&&!playing&&!building&&otherBuildHasSaves(*g));logAction->setEnabled(exists&&QFileInfo(QDir(g->buildDir).filePath(g->outputName+".log")).exists());
    const QString version=g?g->version():QString{};
    const QString info=g?g->title+" · "+g->format()+(version.isEmpty()?QString{}:" · v"+version)+" · "+g->renderer:"No selection";
    selectionLabel->setText(fontMetrics().elidedText(info,Qt::ElideRight,390));selectionLabel->setToolTip(g?info+"\n"+displayPath(g->source):info);
    canvas->setBackground(g?(g->background.isEmpty()?g->cover:g->background):QString{},library.showBackground);
    for(auto it=cards.begin();it!=cards.end();++it)it.value()->setSelected(it.key()==selectedId);
}
void Window::setViewMode(int mode){
    library.viewMode=mode==1?1:0;if(views)views->setCurrentIndex(library.viewMode);
    galleryAction->setChecked(library.viewMode==0);tableAction->setChecked(library.viewMode==1);
    if(coverSize)coverSize->setEnabled(library.viewMode==0);
    if(gallery&&table&&(!filters||!filters->isVisible())){if(library.viewMode==0)gallery->setFocus();else table->setFocus();}
}
void Window::showBuildOutput(){buildDock->show();resizeDocks({buildDock},{190},Qt::Vertical);buildDock->raise();}
void Window::showMenu(const QString& name){
    for(auto* action:menus->actions())if(action->menu()&&action->text().remove('&').compare(name,Qt::CaseInsensitive)==0){
        action->menu()->popup(menus->mapToGlobal(menus->actionGeometry(action).bottomLeft()));return;
    }
}
// Game > Install DLC…: the packages join the game's list; a built game gets them installed right away (the engine
// reads the package and refuses, with the reason, what it cannot install), an unbuilt one when its build finishes.
// Where the outcome of a DLC action goes: the DLC window when it is open, the caller of --install-dlc when there is
// no one to click a message box, a message box otherwise.
void Window::dlcReport(const QString& title,const QString& text,bool error){
    if(dlcDialog)dlcDialog->setStatus(text,error);
    else if(dlcQuiet){dlcQuiet=false;emit dlcFinished(!error,text);}
    else message(title,text,error);
}
// Game > Install DLC…: the game's DLC window. Add DLC… and Remove work on the list (Game.dlc in library.json) and on
// the build's data folder right away; an unbuilt game keeps the list and gets the DLC when its build finishes.
// ---- game updates. An update replaces the game's code: the list and the choice are kept in the library, and the
// game is built again to apply a change (Game::updatePending, the card says "Rebuild"). Its data is then read in
// place from the game's package and the update's.
QString Window::updateOutcome(const Game& game) const {
    const QString next=updateTokenLabel(game,game.updateToken());
    if(game.updatePending())return "Build the game again to get "+next+" (Build now, or Rebuild on its card); until then it cannot be played.";
    if(game.hasBuild())return "The current build already is "+next+".";
    return "The next build will be "+next+".";
}
void Window::updateReport(const QString& title,const QString& text,bool error){
    if(updateDialog)updateDialog->setStatus(text,error);
    else if(updateQuiet){updateQuiet=false;emit updateFinished(!error,text);}
    else message(title,text,error);
}
void Window::installUpdates(){
    auto* game=selected();if(!game||demo)return;
    const QString id=game->id;
    UpdateDialog dialog(*game,running.contains(id)||(builder.busy()&&builder.gameId()==id),this);updateDialog=&dialog;
    if(updateProbe.busy())dialog.setBusy(true,"Updates are being checked…");
    bool build=false;
    connect(&dialog,&UpdateDialog::addRequested,this,[this](const QStringList& files){installUpdatePackages(files);});
    connect(&dialog,&UpdateDialog::removeRequested,this,[this,id](const QStringList& packages){removeUpdatePackages(id,packages);});
    connect(&dialog,&UpdateDialog::choiceRequested,this,[this,id](const QString& choice){setUpdateChoice(id,choice);});
    connect(&dialog,&UpdateDialog::buildRequested,&dialog,[&dialog,&build]{build=true;dialog.accept();});
    dialog.exec();updateDialog=nullptr;refreshLibrary();
    if(build){selectGame(id);startBuild();}
}
bool Window::installUpdatePackages(const QStringList& files,bool interactive){
    updateQuiet=!interactive&&!updateDialog;
    auto* game=selected();if(!game||demo){updateReport("Cannot install the update","Select a game first.",true);return false;}
    const QString id=game->id;
    if(running.contains(id)||(builder.busy()&&builder.gameId()==id)||updateProbe.busy()){updateReport("Game is busy","Wait for this game's build, run or update check to finish first.",true);return false;}
    QStringList wanted,problems;
    for(const auto& file:files){
        const QString path=QFileInfo(file).absoluteFilePath();QString error;
        bool known=false;for(const auto& have:game->updates)known=known||samePath(have.path,path);
        for(const auto& have:wanted)known=known||samePath(have,path);
        if(known)continue;
        if(validUpdatePackage(path,&error))wanted<<path;else problems<<error;
    }
    if(wanted.isEmpty()){
        if(problems.isEmpty())updateReport("Update already installed","Those packages are already on this game's update list.",false);
        else updateReport("Cannot install the update",problems.join("\n"),true);
        return problems.isEmpty();
    }
    if(!problems.isEmpty()&&updateQuiet){updateReport("Cannot install the update",problems.join("\n"),true);return false;}
    // the engine opens each package over the game's own with the keys: that says its version and that it belongs here
    QString error;
    if(!updateProbe.start(*game,tools,wanted,&error)){updateReport("Cannot install the update",error,true);return false;}
    jobLabel->setText("Checking updates…");progress->setVisible(true);progress->setRange(0,0);
    console->appendPlainText("\nUpdates: "+game->title);
    if(updateDialog)updateDialog->setBusy(true,problems.isEmpty()?QString("Checking the update against the game…"):problems.join("\n")+"\nChecking the others…");
    else if(!problems.isEmpty())message("Cannot install the update",problems.join("\n"),true);
    return true;
}
void Window::removeUpdates(const QStringList& packages){
    updateQuiet=!updateDialog;
    auto* game=selected();if(!game||demo){updateReport("Cannot remove the update","Select a game first.",true);return;}
    removeUpdatePackages(game->id,packages);
}
void Window::removeUpdatePackages(const QString& id,const QStringList& packages){
    auto* game=library.find(id);if(!game)return;
    if(running.contains(id)||(builder.busy()&&builder.gameId()==id)||updateProbe.busy()){updateReport("Game is busy","Wait for this game's build, run or update check to finish first.",true);return;}
    int removed=0;
    for(const auto& package:packages)for(int i=game->updates.size()-1;i>=0;--i)if(samePath(game->updates[i].path,package)){
        if(samePath(game->updateChoice,package))game->updateChoice.clear();   // its choice goes with it: back to the newest
        game->updates.removeAt(i);++removed;
    }
    if(!removed){updateReport("Update not installed","That package is not on this game's update list.",true);return;}
    save();refreshLibrary();if(updateDialog)updateDialog->setGame(*game);
    updateReport("Update removed",QString("Removed %1 update%2; the original file%3 not changed. ").arg(removed).arg(removed==1?"":"s").arg(removed==1?" was":"s were")+updateOutcome(*game),false);
}
void Window::chooseUpdate(const QString& choice){
    updateQuiet=!updateDialog;
    auto* game=selected();if(!game||demo){updateReport("Cannot choose the update","Select a game first.",true);return;}
    setUpdateChoice(game->id,choice=="newest"?QString{}:choice);
}
void Window::setUpdateChoice(const QString& id,const QString& choice){
    auto* game=library.find(id);if(!game)return;
    if(running.contains(id)||(builder.busy()&&builder.gameId()==id)){updateReport("Game is busy","Wait for this game's build or run to finish first.",true);return;}
    QString next=choice;
    if(!next.isEmpty()&&next!="none"){
        bool found=false;for(const auto& update:game->updates)if(samePath(update.path,next)){next=update.path;found=true;}
        if(!found){updateReport("Update not installed","That package is not on this game's update list.",true);return;}
    }
    game->updateChoice=next;save();refreshLibrary();if(updateDialog)updateDialog->setGame(*game);
    updateReport("Version chosen",updateOutcome(*game),false);
}
void Window::installDlc(){
    auto* game=selected();if(!game||demo)return;
    const QString id=game->id;
    if(adoptInstalledDlc(*game))save();
    DlcDialog dialog(*game,running.contains(id)||(builder.busy()&&builder.gameId()==id),this);dlcDialog=&dialog;
    if(dlcInstaller.busy())dialog.setBusy(true,"A DLC installation is running…");
    connect(&dialog,&DlcDialog::addRequested,this,[this](const QStringList& files){installDlcPackages(files);});
    connect(&dialog,&DlcDialog::removeRequested,this,[this,id](const QStringList& packages){removeDlcPackages(id,packages);});
    dialog.exec();dlcDialog=nullptr;refreshSelection();
}
bool Window::installDlcPackages(const QStringList& files,bool interactive){
    dlcQuiet=!interactive&&!dlcDialog;
    auto* game=selected();if(!game||demo){dlcReport("Cannot install DLC","Select a game first.",true);return false;}
    const QString id=game->id;
    if(running.contains(id)||(builder.busy()&&builder.gameId()==id)||dlcInstaller.busy()){dlcReport("Game is busy","Wait for this game's build, run or DLC installation to finish first.",true);return false;}
    if(adoptInstalledDlc(*game))save();
    QStringList added,problems;
    for(const auto& file:files){
        const QString path=QFileInfo(file).absoluteFilePath();QString error;
        bool known=false;for(const auto& have:game->dlc+added)known=known||samePath(have,path);
        if(known)continue;
        if(validDlcPackage(path,&error))added<<path;else problems<<error;
    }
    if(added.isEmpty()){
        if(problems.isEmpty())dlcReport("DLC already installed","Those packages are already in this game's DLC list.",false);
        else dlcReport("Cannot install DLC",problems.join("\n"),true);
        return problems.isEmpty();
    }
    if(!problems.isEmpty()&&dlcQuiet){dlcReport("Cannot install DLC",problems.join("\n"),true);return false;}   // all or nothing from the command line
    game->dlc+=added;save();if(dlcDialog)dlcDialog->setGame(*game);
    if(!game->hasBuild()){
        dlcReport("DLC added",problems.join("\n")+(problems.isEmpty()?"":"\n\n")+QString("%1 package%2 added. The DLC is installed when %3 is built.").arg(added.size()).arg(added.size()==1?"":"s").arg(game->title),!problems.isEmpty());
        return true;
    }
    if(startDlcSync(id,DlcInstalled)){
        // the files that were not packages at all are reported now; the engine's verdict on the others follows
        if(!problems.isEmpty()){if(dlcDialog)dlcDialog->setStatus(problems.join("\n"),true);else message("Cannot install DLC",problems.join("\n"),true);}
        return true;
    }
    game=library.find(id);if(game){for(const auto& path:added)game->dlc.removeAll(path);save();if(dlcDialog)dlcDialog->setGame(*game);}
    return false;
}
void Window::removeDlc(const QStringList& packages){
    dlcQuiet=!dlcDialog;
    auto* game=selected();if(!game||demo){dlcReport("Cannot remove DLC","Select a game first.",true);return;}
    if(adoptInstalledDlc(*game))save();
    removeDlcPackages(game->id,packages);
}
// Remove in the DLC window: the packages leave the list and, for a built game, its data folder.
void Window::removeDlcPackages(const QString& id,const QStringList& packages){
    auto* game=library.find(id);if(!game)return;
    if(running.contains(id)||(builder.busy()&&builder.gameId()==id)||dlcInstaller.busy()){dlcReport("Game is busy","Wait for this game's build, run or DLC installation to finish first.",true);return;}
    int removed=0;
    for(const auto& package:packages)for(int i=game->dlc.size()-1;i>=0;--i)if(samePath(game->dlc[i],package)){game->dlc.removeAt(i);++removed;}
    if(!removed){dlcReport("DLC not installed","That package is not in this game's DLC list.",true);return;}
    save();if(dlcDialog)dlcDialog->setGame(*game);
    if(game->hasBuild()){if(!startDlcSync(id,DlcChanged)&&dlcDialog){game=library.find(id);if(game){adoptInstalledDlc(*game);save();dlcDialog->setGame(*game);}}}
    else dlcReport("DLC removed",QString("%1 package%2 taken off the list. The original files were not changed.").arg(removed).arg(removed==1?"":"s"),false);
}
bool Window::startDlcSync(const QString& id,DlcReason reason){
    const auto* game=library.find(id);if(!game)return false;
    QString error;if(!dlcInstaller.start(*game,tools,&error)){dlcReport("Cannot install DLC",error,true);return false;}
    dlcReason=reason;jobLabel->setText(game->dlc.isEmpty()||reason==DlcChanged?"Updating DLC…":"Installing DLC…");progress->setVisible(true);progress->setRange(0,0);
    if(dlcDialog)dlcDialog->setBusy(true,reason==DlcChanged?"Removing…":"Installing… a large package can take a while.");
    console->appendPlainText("\nDLC: "+game->title);refreshSelection();return true;
}
void Window::showFilters(){if(filters&&filterAnchor)filters->openBelow(filterAnchor);}
void Window::installKeys(){
    const QString source=QFileDialog::getOpenFileName(this,"Install Nintendo keys",{},"Nintendo key file (prod.keys);;All files (*)");if(source.isEmpty())return;
    QFile input(source);if(!input.open(QIODevice::ReadOnly)){message("Cannot install keys",input.errorString(),true);return;}
    const QByteArray data=input.readAll();
    if(!data.contains("header_key")&&!data.contains("header_key_source")){message("Invalid key file","This file does not contain the NCA header keys required for NSP packages.",true);return;}
    const QString destination=QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("keys/prod.keys");
    if(!QDir().mkpath(QFileInfo(destination).absolutePath())){message("Cannot install keys","The launcher could not create its keys folder.",true);return;}
    QSaveFile output(destination);if(!output.open(QIODevice::WriteOnly)||output.write(data)!=data.size()||!output.commit()){message("Cannot install keys",output.errorString(),true);return;}
    library.keysPath=destination;save();refreshTools();message("Keys installed","prod.keys was installed in the launcher data folder. Key contents are never copied into builds or release packages.");
}
void Window::setAppearance(const QString& mode,const QString& accent){
    Theme::apply(*qApp,mode,accent);
    for(auto* action:findChildren<QAction*>()){const auto kind=action->property("iconKind").toString();if(!kind.isEmpty())action->setIcon(commandIcon(kind));}
    for(auto* card:cards)card->refreshAppearance();if(canvas)canvas->update();if(titleBar)titleBar->update();if(gallery)gallery->viewport()->update();
#ifdef Q_OS_WIN
    if(HMODULE dwm=LoadLibraryW(L"dwmapi.dll")){using SetAttribute=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);auto set=reinterpret_cast<SetAttribute>(GetProcAddress(dwm,"DwmSetWindowAttribute"));BOOL dark=mode!="light";if(set)set(reinterpret_cast<HWND>(winId()),20,&dark,sizeof dark);FreeLibrary(dwm);}
#endif
}

void Window::refreshTools(){
    tools=Toolchain::find(library.toolRoot);tools.keys=library.keysPath;tools.findNxrs(library.nxrsBuild);dotnetReady=false;const int generation=++toolsGeneration;
    const auto state=[](const QString& p){return p.isEmpty()?QString("Missing"):QDir::toNativeSeparators(p);};
    const QString nxrs=tools.nxrsBuild.isEmpty()?QString("Missing"):QDir::toNativeSeparators(tools.nxrsBuild)
        +QString(" (contract %1%2)").arg(tools.nxrsContract).arg(tools.nxrsVersion.isEmpty()?QString{}:", "+tools.nxrsVersion);
    healthText=QString("Turbo\nPython: %1\nGCC: %2\nRuntime: %3\n\nnxrs engine\nnxrs-build: %5\n\nKeys: %4\n.NET 10: checking…").arg(state(tools.python),state(tools.compiler),state(tools.runtime),state(tools.keys),nxrs);
    // ready when the engines the library's games use can build (Turbo for an empty library)
    bool turboUsed=library.games.isEmpty(),nxrsUsed=false;for(const auto& g:library.games)(g.nxrs()?nxrsUsed:turboUsed)=true;
    const bool ready=(!turboUsed||tools.canBuild())&&(!nxrsUsed||tools.canBuild(engineNxrs));
    toolStatus->setProperty("badge",ready?"ok":"error");
    toolStatus->setText(ready?"Engine ready":"Setup needed");
    toolStatus->setToolTip(ready?displayPath(tools.root)+"\nClick for details":toolProblem()+"\nClick for details");
    style()->unpolish(toolStatus);style()->polish(toolStatus);
    emit toolsChanged();
    if(tools.dotnet.isEmpty()){healthText.replace("checking…","Not found");emit toolsChanged();return;}
    auto* check=new QProcess(this);
    connect(check,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,check,generation](int code,QProcess::ExitStatus){
        if(generation==toolsGeneration){dotnetReady=code==0&&QString::fromUtf8(check->readAllStandardOutput()).contains(QRegularExpression("Microsoft\\.NETCore\\.App 10\\."));healthText.replace("checking…",dotnetReady?"Ready":"Not found");emit toolsChanged();}
        check->deleteLater();
    });
    connect(check,&QProcess::errorOccurred,this,[this,check,generation](QProcess::ProcessError error){if(error==QProcess::FailedToStart){if(generation==toolsGeneration){healthText.replace("checking…","Could not start dotnet");emit toolsChanged();}check->deleteLater();}});
    check->start(tools.dotnet,{"--list-runtimes"});QTimer::singleShot(10000,check,[check]{if(check->state()!=QProcess::NotRunning)check->kill();});
}

// The engine's button_layout values; the same two choices in Settings > Controller and Properties > Runtime.
static void addButtonLayouts(QComboBox* box){
    box->addItem("Switch — by position (A right, B bottom)","switch");box->addItem("Xbox — by label (A bottom, B right)","xbox");
    box->setItemData(box->count()-2,"Switch positions on every pad: PlayStation Circle = A, Cross = B, Triangle = X, Square = Y",Qt::ToolTipRole);
    box->setItemData(box->count()-1,"Printed Xbox letters: PlayStation Cross = A, Circle = B, Square = X, Triangle = Y",Qt::ToolTipRole);
}
static QString buttonLayoutName(const QString& layout){return layout=="xbox"?"Xbox":"Switch";}
bool Window::editGame(Game& game,int tab,bool adding){
    const bool locked=running.contains(game.id)||(builder.busy()&&builder.gameId()==game.id);
    QDialog dialog(this);dialog.setWindowTitle(adding?"Add game":game.title+" — Properties");dialog.resize(670,400);
    auto* layout=new QVBoxLayout(&dialog);layout->setContentsMargins(10,10,10,10);auto* tabs=new QTabWidget;layout->addWidget(tabs,1);
    auto* general=tabForm(tabs,"General");auto* title=new QLineEdit(game.title);auto* source=new QLineEdit(game.source.isEmpty()?QString{}:displayPath(game.source));
    general->addRow("Display name",title);auto* sourceField=browseField(source,&dialog,"Choose source","Game packages (*.nup *.nsp *.xci *.nro);;All files (*)");sourceField->setEnabled(!locked);general->addRow("Source",sourceField);
    auto* folder=new QPushButton("Choose extracted folder…");folder->setEnabled(!locked);general->addRow("",folder);connect(folder,&QPushButton::clicked,&dialog,[&dialog,source]{auto p=QFileDialog::getExistingDirectory(&dialog,"Choose extracted game folder");if(!p.isEmpty())source->setText(QDir::toNativeSeparators(p));});
    general->addRow("Format",textLabel(game.format()));if(!game.version().isEmpty())general->addRow("Version",textLabel(game.version()));general->addRow("Status",textLabel(adding?"Not built":statusFor(game)));
    auto* pathsNote=textLabel("NUP/NSP game data stays in the original package. Changing the source requires a rebuild.","Muted");pathsNote->setWordWrap(true);general->addRow(pathsNote);
    auto* build=tabForm(tabs,"Build");auto* destination=new QLineEdit(displayPath(game.buildDir));build->addRow("Build folder",browseField(destination,&dialog,"Choose build output folder",{},true));
    auto* name=new QLineEdit(game.outputName);name->setReadOnly(!adding);build->addRow("Executable name",name);
    auto* workers=new QSpinBox;workers->setRange(0,64);workers->setSpecialValueText("Use default");workers->setValue(game.workers);build->addRow("Compiler workers",workers);
    auto* optimization=new QComboBox;optimization->addItems({"O0 — no optimization","O1 — quick build","O2 — balanced (recommended)","O3 — maximum optimization"});optimization->setCurrentIndex(game.optimization);build->addRow("Optimization",optimization);
    auto* engine=new QComboBox;engine->addItem("Turbo — Switch2RECOMP",QString(engineTurbo));engine->addItem("nxrs engine — nxrs-build",QString(engineNxrs));
    engine->setCurrentIndex(game.nxrs()?1:0);
    engine->setToolTip("The engine that builds this game. Each engine builds into its own folder (the nxrs one ends in [nxrs]), so both builds can exist; switch back any time.");
    build->addRow("Engine",engine);
    // nxrs compiles at one measured flag set: the optimization level is Turbo's
    auto* runtime=tabForm(tabs,"Runtime");auto* renderer=new QComboBox;renderer->addItem("Vulkan — native NVN","vulkan");renderer->addItem("OpenGL — compatibility","opengl");renderer->setCurrentIndex(qMax(0,renderer->findData(game.renderer)));runtime->addRow("Graphics backend",renderer);
    // the nxrs engine has one renderer: shown as fixed text, not a choice
    auto* vulkanOnly=textLabel("Vulkan");vulkanOnly->setObjectName("NxrsRenderer");runtime->addRow("Graphics backend",vulkanOnly);
    auto* buttonLayout=new QComboBox;buttonLayout->addItem("Use default — "+buttonLayoutName(library.buttonLayout),QString());addButtonLayouts(buttonLayout);
    buttonLayout->setCurrentIndex(qMax(0,buttonLayout->findData(game.buttonLayout)));runtime->addRow("Controller layout",buttonLayout);
    auto* screenMode=new QComboBox;screenMode->setObjectName("GameScreenMode");
    screenMode->addItem(QString("Use default — ")+(library.docked?"TV mode":"Handheld"),QString());screenMode->addItem("TV mode — sharper","tv");screenMode->addItem("Handheld — lighter","handheld");
    screenMode->setCurrentIndex(qMax(0,screenMode->findData(game.screenMode)));
    screenMode->setToolTip("TV mode renders at the game's TV resolution (Switch 1920x1080, Switch 2 up to 2560x1440 and more): sharper. Handheld renders at its handheld resolution (1280x720 / 1920x1080): lighter on the GPU. The default is in Settings > Play.");
    runtime->addRow("Screen mode",screenMode);
    auto* runtimeNote=textLabel({},"Muted");runtimeNote->setWordWrap(true);runtime->addRow(runtimeNote);
    // Turbo's choices as they were; for the nxrs engine: no OpenGL, no -O (it compiles at one measured flag set), and the
    // screen mode as what it is on a PC, a quality choice (the build starts it from the game's profile, op_mode)
    const auto engineChanged=[engine,optimization,build,runtime,renderer,vulkanOnly,runtimeNote]{
        const bool nx=engine->currentData().toString()==engineNxrs;
        build->setRowVisible(optimization,!nx);runtime->setRowVisible(renderer,!nx);runtime->setRowVisible(vulkanOnly,nx);
        runtimeNote->setText(nx?"Other runtime options remain in the game's INI; the background settings are in Settings > Play. The screen mode and the controller layout are saved right away (their defaults are in Settings > Play and Settings > Controller)."
                               :"Vulkan needs the bundled shader tools, Python, and .NET 10.\n\nOther runtime options remain in the game's INI; the background settings are in Settings > Play. Renderer changes are applied when you next play; the screen mode and the controller layout are saved right away (their defaults are in Settings > Play and Settings > Controller).");
    };engineChanged();
    connect(engine,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[engine,destination,engineChanged]{
        destination->setText(QDir::toNativeSeparators(buildFolderFor(destination->text().trimmed(),engine->currentData().toString())));
        engineChanged();
    });
    auto* openIni=new QPushButton("Open game settings file");openIni->setEnabled(QFileInfo(game.iniPath()).exists());runtime->addRow("",openIni);connect(openIni,&QPushButton::clicked,&dialog,[&game]{QDesktopServices::openUrl(QUrl::fromLocalFile(game.iniPath()));});
    auto* artwork=tabForm(tabs,"Artwork");auto* cover=new QLineEdit(game.cover);auto* background=new QLineEdit(game.background);
    artwork->addRow("Cover image",browseField(cover,&dialog,"Choose cover","Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    artwork->addRow("Background image",browseField(background,&dialog,"Choose background artwork","Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    auto* note=textLabel("The background follows your selected game and is dimmed behind the gallery. Leave it empty to use the cover. Clear both paths to restore generated artwork.","Muted");note->setWordWrap(true);artwork->addRow(note);
    DlcPanel* dlc=nullptr;if(!adding){dlc=new DlcPanel(game,locked||dlcInstaller.busy());tabs->addTab(dlc,"DLC");}
    UpdatePanel* updates=nullptr;if(!adding){updates=new UpdatePanel(game,locked||updateProbe.busy());tabs->addTab(updates,"Updates");}
    tabs->widget(1)->setEnabled(!locked);tabs->widget(2)->setEnabled(!locked);
    tabs->setCurrentIndex(qBound(0,tab,tabs->count()-1));
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);if(adding)buttons->button(QDialogButtonBox::Ok)->setText("Add game");layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        if(title->text().trimmed().isEmpty()||destination->text().trimmed().isEmpty()){QMessageBox::warning(&dialog,"Missing details","A title and build folder are required.");return;}
        for(const auto& pair:{qMakePair(cover->text().trimmed(),game.cover),qMakePair(background->text().trimmed(),game.background)}){
            if(!pair.first.isEmpty()&&pair.first!=pair.second){QImageReader reader(pair.first);if(!reader.canRead()){QMessageBox::warning(&dialog,"Cannot load artwork",reader.errorString()+"\n"+pair.first);return;}}
        }
        game.title=title->text().trimmed();game.source=source->text().trimmed().isEmpty()?QString{}:QFileInfo(QDir::fromNativeSeparators(source->text().trimmed())).absoluteFilePath();
        game.buildDir=QFileInfo(QDir::fromNativeSeparators(destination->text().trimmed())).absoluteFilePath();
        setGameEngine(game,engine->currentData().toString());
        if(adding)game.outputName=outputNameFor(name->text());game.workers=workers->value();game.optimization=optimization->currentIndex();game.renderer=engine->currentData().toString()==engineNxrs?QString("vulkan"):renderer->currentData().toString();game.buttonLayout=buttonLayout->currentData().toString();game.screenMode=screenMode->currentData().toString();
        game.cover=QDir::fromNativeSeparators(cover->text().trimmed());game.background=QDir::fromNativeSeparators(background->text().trimmed());
        if(dlc)game.dlc=dlc->packages();
        if(updates)game.updates=updates->updates();
        dialog.accept();
    });
    return dialog.exec()==QDialog::Accepted;
}
void Window::showProperties(int tab){
    auto* game=selected();if(!game)return;const auto id=game->id;
    if(adoptInstalledDlc(*game))save();
    Game edited=*game;
    if(editGame(edited,tab)){
        bool dlcChanged=false;
        if(auto* current=library.find(id)){
            current->title=edited.title;current->source=edited.source;current->buildDir=edited.buildDir;
            if(current->engine!=edited.engine){   // the other engine's folder, and what is built there
                current->engine=edited.engine;current->builtSource=edited.builtSource;current->builtUpdate=edited.builtUpdate;
                current->buildIncomplete=edited.buildIncomplete;current->updates=edited.updates;
            }
            current->renderer=edited.renderer;current->workers=edited.workers;current->optimization=edited.optimization;
            current->cover=edited.cover;current->background=edited.background;
            dlcChanged=current->dlc!=edited.dlc;current->dlc=edited.dlc;
            // an update removed in Properties > Updates; the card then asks for a rebuild when the build had it applied
            if(current->updates.size()!=edited.updates.size()){
                current->updates=edited.updates;
                bool kept=current->updateChoice.isEmpty()||current->updateChoice=="none";
                for(const auto& update:current->updates)kept=kept||samePath(update.path,current->updateChoice);
                if(!kept)current->updateChoice.clear();
            }
            const bool screenChanged=effectiveDocked(*current,library.docked)!=effectiveDocked(edited,library.docked);current->screenMode=edited.screenMode;
            {QString error;if(screenChanged&&current->hasBuild()&&!writeDocked(current->iniPath(),effectiveDocked(*current,library.docked),&error))message("Cannot save game settings",error,true);}
            const bool layoutChanged=current->buttonLayout!=edited.buttonLayout;current->buttonLayout=edited.buttonLayout;
            // already-built games get the new layout in their INI now, not only at the next Play
            QString error;if(layoutChanged&&current->hasBuild()&&!writeButtonLayout(current->iniPath(),effectiveButtonLayout(*current,library.buttonLayout),&error))
                message("Cannot save game settings",error,true);
        }
        invalidateArtwork();save();refreshLibrary();
        // removed DLC leaves the built game's data folder now, not at the next Play
        if(dlcChanged){const auto* current=library.find(id);if(current&&current->hasBuild())startDlcSync(id,DlcChanged);}
    }
}
void Window::showAppearance(){showSettings(3);}
void Window::showControllerSettings(){showSettings(2);}
void Window::showSettings(int initialTab){
    QDialog dialog(this);dialog.setWindowTitle("nxrs — Settings");dialog.resize(690,440);auto* layout=new QVBoxLayout(&dialog);auto* tabs=new QTabWidget;layout->addWidget(tabs,1);
    auto* toolkit=tabForm(tabs,"Engine");auto* root=new QLineEdit(library.toolRoot);root->setPlaceholderText("The engine bundled with this launcher");toolkit->addRow("Engine folder",browseField(root,&dialog,"Choose the vendored engine folder (optional)",{},true));
    auto* nxrsBuild=new QLineEdit(QDir::toNativeSeparators(library.nxrsBuild));nxrsBuild->setPlaceholderText("The nxrs engine bundled with this launcher");
    toolkit->addRow("nxrs-build",browseField(nxrsBuild,&dialog,"Choose nxrs-build (a development engine)",QString("nxrs-build (nxrs-build%1);;All files (*)").arg(executableSuffix)));
    auto* health=textLabel(healthText,"Muted");health->setWordWrap(true);health->setTextInteractionFlags(Qt::TextSelectableByMouse);toolkit->addRow(health);
    connect(this,&Window::toolsChanged,&dialog,[this,health]{health->setText(healthText);});
    // nxrs-build doctor: Vulkan devices, free disk, toolchain and the rest, run when the page opens
    auto* doctor=textLabel(tools.nxrsBuild.isEmpty()?QString("nxrs-build not found: no checks."):QString("Checking (nxrs-build doctor)…"),"Muted");
    doctor->setObjectName("NxrsDoctor");doctor->setWordWrap(true);doctor->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* recheck=new QPushButton("Check again");recheck->setEnabled(!tools.nxrsBuild.isEmpty());
    auto* doctorBox=new QWidget;auto* doctorLayout=new QVBoxLayout(doctorBox);doctorLayout->setContentsMargins(0,0,0,0);doctorLayout->addWidget(doctor);doctorLayout->addWidget(recheck,0,Qt::AlignLeft);
    toolkit->addRow("nxrs checks",doctorBox);
    const auto runDoctor=[this,doctor,recheck,&dialog]{
        if(tools.nxrsBuild.isEmpty())return;
        doctor->setText("Checking (nxrs-build doctor)…");recheck->setEnabled(false);
        auto* p=new QProcess(&dialog);p->setProgram(tools.nxrsBuild);p->setArguments(nxrsDoctorArguments());
        p->setWorkingDirectory(QFileInfo(tools.nxrsBuild).absolutePath());p->setProcessEnvironment(tools.environment());p->setProcessChannelMode(QProcess::MergedChannels);
        connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),doctor,[doctor,recheck,p](int code,QProcess::ExitStatus status){
            const auto d=parseNxrsDoctor(QString::fromUtf8(p->readAll()),status==QProcess::NormalExit?code:-1);
            doctor->setText((d.ok()?QString("All checks passed."):QString("%1 check(s) failed.").arg(qMax(1,d.failed())))+"\n"+d.text());
            recheck->setEnabled(true);p->deleteLater();
        });
        connect(p,&QProcess::errorOccurred,doctor,[doctor,recheck,p](QProcess::ProcessError e){if(e==QProcess::FailedToStart){doctor->setText("nxrs-build doctor did not start: "+p->errorString());recheck->setEnabled(true);}});
        p->start();QTimer::singleShot(60000,p,[p]{if(p->state()!=QProcess::NotRunning)p->kill();});
    };
    connect(recheck,&QPushButton::clicked,&dialog,runDoctor);QTimer::singleShot(0,&dialog,runDoctor);
    // the setup guide is a source-checkout document; a release has nothing to set up and does not ship docs/
    const QString guidePath=QDir(toolkitProjectRoot(library.toolRoot)).filePath("docs/BUILD_ENVIRONMENT_GUIDE.md");
    if(QFileInfo(guidePath).isFile()){auto* guide=new QPushButton("Open setup guide");toolkit->addRow("",guide);connect(guide,&QPushButton::clicked,&dialog,[guidePath]{QDesktopServices::openUrl(QUrl::fromLocalFile(guidePath));});}
    auto* builds=tabForm(tabs,"Build defaults");auto* output=new QLineEdit(library.outputRoot);output->setPlaceholderText("Empty: create builds beside the source package");builds->addRow("Build location",browseField(output,&dialog,"Choose default output folder",{},true));
    auto* workers=new QSpinBox;workers->setRange(1,64);workers->setValue(library.jobs);builds->addRow("Compiler workers",workers);
    auto* controller=tabForm(tabs,"Controller");auto* buttonLayout=new QComboBox;addButtonLayouts(buttonLayout);
    buttonLayout->setCurrentIndex(qMax(0,buttonLayout->findData(library.buttonLayout)));controller->addRow("Button layout",buttonLayout);
    auto* controllerNote=textLabel("Applies to Xbox, PlayStation and other controllers. Switch keeps the Switch pad's positions, so on a PlayStation pad Circle is A and Cross is B. Xbox follows the printed Xbox letters: Cross is A, Square is X.\n\nSaved into every built game that uses the default (a running game switches live). Override it per game in Properties > Runtime; single buttons can be remapped with button_map in the game's INI.","Muted");
    controllerNote->setWordWrap(true);controller->addRow(controllerNote);
    auto* playTab=new QWidget;auto* play=new QFormLayout(playTab);play->setContentsMargins(18,22,18,20);play->setHorizontalSpacing(15);play->setVerticalSpacing(12);play->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto* pauseDefault=new QCheckBox("Pause the game when its window is in the background");pauseDefault->setObjectName("PauseOnFocusLoss");pauseDefault->setChecked(library.pauseOnFocusLoss);
    auto* screenMode=new QComboBox;screenMode->setObjectName("ScreenMode");screenMode->addItem("TV mode — sharper",true);screenMode->addItem("Handheld — lighter",false);
    screenMode->setCurrentIndex(library.docked?0:1);
    screenMode->setToolTip("TV mode renders each game at its TV resolution (Switch 1920x1080, Switch 2 up to 2560x1440 and more): sharper. Handheld renders at its handheld resolution (1280x720 / 1920x1080): lighter on the GPU.");
    play->addRow("Screen mode",screenMode);
    play->addRow(pauseDefault);
    auto* muteDefault=new QCheckBox("Mute the game when its window is in the background");muteDefault->setObjectName("MuteOnFocusLoss");muteDefault->setChecked(library.muteOnFocusLoss);
    play->addRow(muteDefault);
    auto* askUser=new QCheckBox("Ask who is playing (when several profiles exist)");askUser->setObjectName("AskUser");askUser->setChecked(library.askUser);
    play->addRow(askUser);
    auto* playNote=textLabel("The screen mode is the default for every game without its own (Properties > Runtime); the rest apply to every game. Off: a game keeps running, and its sound playing, while you use another window, as a PC game does. Built games get a change in their settings file right away, the others when they are built.","Muted");playNote->setWordWrap(true);play->addRow(playNote);
    auto* appearance=tabForm(tabs,"Appearance");
    auto* mode=new QComboBox;mode->addItem("Dark","dark");mode->addItem("Light","light");mode->setCurrentIndex(library.colorMode=="light"?1:0);appearance->addRow("Color mode",mode);
    auto* accent=new QComboBox;for(const auto& color:Theme::accents())accent->addItem(color.label,color.id);accent->setCurrentIndex(qMax(0,accent->findData(library.accentColor)));appearance->addRow("Accent color",accent);
    const auto updateColors=[this,mode,accent]{
        for(int i=0;i<accent->count();++i){QPixmap swatch(18,18);swatch.fill(Theme::colorsFor(mode->currentData().toString(),accent->itemData(i).toString()).accent);accent->setItemIcon(i,QIcon(swatch));}
        setAppearance(mode->currentData().toString(),accent->currentData().toString());
    };
    connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,updateColors);connect(accent,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,updateColors);updateColors();
    auto* backdrop=new QCheckBox("Show selected game artwork behind the gallery");backdrop->setChecked(library.showBackground);appearance->addRow(backdrop);
    auto* location=textLabel("Library file:\n"+displayPath(library.path),"Muted");location->setWordWrap(true);location->setTextInteractionFlags(Qt::TextSelectableByMouse);appearance->addRow(location);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        const QString chosenNxrs=QDir::fromNativeSeparators(nxrsBuild->text().trimmed());
        if(builder.busy()&&(root->text()!=library.toolRoot||output->text()!=library.outputRoot||workers->value()!=library.jobs||chosenNxrs!=library.nxrsBuild)){QMessageBox::information(&dialog,"Build in progress","Engine/build defaults cannot change during a build. Appearance can still be changed.");return;}
        const auto selectedRoot=QDir::fromNativeSeparators(root->text().trimmed());
        if(!selectedRoot.isEmpty()&&selectedRoot!=library.toolRoot&&!QFileInfo(QDir(selectedRoot).filePath("switchrecomp.py")).isFile()){QMessageBox::warning(&dialog,"Engine not found","Select the vendored engine folder (the one containing switchrecomp.py).");return;}
        if(selectedRoot.isEmpty()){library.toolRoot=Toolchain::discoverRoot();library.toolRootChosen=false;}   // empty = the bundled engine
        else if(const QString chosen=QFileInfo(selectedRoot).absoluteFilePath();chosen!=library.toolRoot){
            library.toolRoot=chosen;library.toolRootChosen=QDir::cleanPath(chosen)!=QDir::cleanPath(Toolchain::discoverRoot());
        }
        if(!chosenNxrs.isEmpty()&&chosenNxrs!=library.nxrsBuild&&!QFileInfo::exists(chosenNxrs)){QMessageBox::warning(&dialog,"nxrs-build not found","Select nxrs-build"+QString(executableSuffix)+" or the folder that holds it.");return;}
        library.nxrsBuild=chosenNxrs.isEmpty()?QString{}:QFileInfo(chosenNxrs).absoluteFilePath();
        library.outputRoot=QDir::fromNativeSeparators(output->text().trimmed());library.jobs=workers->value();library.showBackground=backdrop->isChecked();
        library.colorMode=mode->currentData().toString();library.accentColor=accent->currentData().toString();
        const QString layout=buttonLayout->currentData().toString();
        if(layout!=library.buttonLayout){
            library.buttonLayout=layout;int failed=0;QString error;
            for(const auto& g:library.games)if(g.buttonLayout.isEmpty()&&g.hasBuild()&&!writeButtonLayout(g.iniPath(),layout,&error))failed++;
            if(failed)message("Cannot save game settings",QString("The controller layout could not be written to %1 game(s): %2\nIt is written again when you play.").arg(failed).arg(error),true);
        }
        if(pauseDefault->isChecked()!=library.pauseOnFocusLoss||muteDefault->isChecked()!=library.muteOnFocusLoss||screenMode->currentData().toBool()!=library.docked||askUser->isChecked()!=library.askUser){
            library.pauseOnFocusLoss=pauseDefault->isChecked();library.muteOnFocusLoss=muteDefault->isChecked();library.docked=screenMode->currentData().toBool();library.askUser=askUser->isChecked();int failed=0;QString error;
            for(const auto& g:library.games)if(g.hasBuild()&&!writePlaySettings(g.iniPath(),library.playFor(g),&error))failed++;
            if(failed)message("Cannot save game settings",QString("The play settings could not be written to %1 game(s): %2\nThey are written again when you play.").arg(failed).arg(error),true);
        }
        backgroundAction->setChecked(library.showBackground);save();refreshTools();refreshSelection();dialog.accept();
    });tabs->addTab(playTab,"Play");tabs->setCurrentIndex(qBound(0,initialTab,4));dialog.exec();setAppearance(library.colorMode,library.accentColor);
}

bool Window::importSource(const QString& source,bool interactive,const QString& output){
    QString error;if(!validSource(source,&error)){if(interactive)message("Cannot add game",error,true);return false;}
    Game game=gameFromSource(source,library.outputRoot);if(!output.isEmpty())game.buildDir=QFileInfo(output).absoluteFilePath();
    for(const auto& g:library.games)if(samePath(g.source,game.source)){selectedId=g.id;refreshLibrary();return true;}
    if(interactive&&!editGame(game,0,true))return false;
    if(!library.add(game)){if(interactive)message("Already in library","That source or native build is already in the library.");return false;}
    selectedId=game.id;save();refreshLibrary();return true;
}
bool Window::importBuild(const QString& executable){
    QString error;Game game;if(!gameFromExecutable(executable,&game,&error)){message("Cannot import build",error,true);return false;}
    for(const auto& g:library.games)if(samePath(g.source,game.source)&&(running.contains(g.id)||(builder.busy()&&builder.gameId()==g.id))){message("Game is busy","Wait for this game's build or run to finish before importing it.");return false;}
    selectedId=library.importBuild(game);save();refreshLibrary();return true;
}
void Window::startBuild(){
    auto* game=selected();if(!game||demo||running.contains(game->id))return;
    game->outputName=outputNameFor(game->outputName);   // a name from an older library keeps step with what the engine writes
    Game build=*game;build.buttonLayout=effectiveButtonLayout(*game,library.buttonLayout);
    builder.play=library.play();   // writeGameSettings applies the game's own screen mode
    QString error;if(!builder.start(build,tools,library.jobs,&error)){emit smokeFinished(false);message("Cannot start build",error,true);return;}
    game->buildIncomplete=true;save();console->clear();console->appendPlainText("Source: "+displayPath(game->source)+"\nOutput: "+displayPath(game->buildDir)+"\n");
    buildDock->setWindowTitle("Build output — "+game->title);showBuildOutput();refreshLibrary();
}
void Window::activateGame(){if(auto* game=selected()){if(game->hasBuild())play();else showProperties(1);}}
void Window::cardAction(const QString& id){selectGame(id);if(demo)return;auto* game=selected();if(!game)return;if((builder.busy()&&builder.gameId()==id)||running.contains(id))stop();else if(game->hasBuild())play();else startBuild();}
void Window::play(){
    auto* game=selected();if(!game||demo||!game->hasBuild()||running.contains(game->id)||(builder.busy()&&builder.gameId()==game->id))return;
    if(inPlacePackage(game->source)&&!game->sourceAvailable()){message("Source unavailable","Reconnect the drive containing the source package, or update the source in Properties and rebuild.",true);return;}
    // Turbo's Vulkan renderer translates shaders while it runs with Python and .NET; nxrs translated them in the build
    if(!game->nxrs()&&game->renderer=="vulkan"&&(!dotnetReady||tools.python.isEmpty())){message("Shader tools need setup","Vulkan requires Python and .NET 10. Check File > Settings > Engine.",true);return;}
    if(!game->nxrs()&&game->renderer=="vulkan"&&!QFileInfo(QDir(game->buildDir).filePath("tools/nxshader/nxs_live.py")).exists()){
        message("Shader tools are missing","Rebuild this game with the current engine to stage its Vulkan shader tools before playing.",true);return;
    }
    // add-on content: the data folder has to hold the game's DLC list before the game starts
    if(dlcInstaller.busy())return;
    if(adoptInstalledDlc(*game))save();
    if(!dlcUpToDate(*game)){startDlcSync(game->id,DlcPlay);return;}
    // switched engines: the other build's saves are offered once (a copy, after a backup of this build's)
    if(game->savesOffered.isEmpty()&&otherBuildHasSaves(*game)){
        const QString other=game->nxrs()?"Turbo":"nxrs";
        game->savesOffered=game->nxrs()?engineTurbo:engineNxrs;save();
        const auto answer=QMessageBox::question(this,"Copy saves",QString("The %1 build of “%2” has saves. Copy them into this build before playing?\n\nThis build's save folder is backed up first (save-backup-<time> beside it). Asked once; Game > Copy saves does it later.").arg(other,game->title),QMessageBox::Yes|QMessageBox::No,QMessageBox::No);
        if(answer==QMessageBox::Yes){copySaves(true,false);game=selected();if(!game)return;}
    }
    QString error;if(!writeGameSettings(*game,library.buttonLayout,library.play(),&error)){message("Cannot save game settings",error,true);return;}
    QProcess process;process.setProgram(game->executable());process.setWorkingDirectory(game->buildDir);auto env=tools.environment();
    process.setProcessEnvironment(env);qint64 pid=0;if(!process.startDetached(&pid)){message("Could not launch",process.errorString(),true);return;}
    running[game->id]=pid;game->lastPlayed=QDateTime::currentDateTime();save();refreshLibrary();jobLabel->setText("Game launched");
}
void Window::stop(){
    auto* game=selected();if(!game)return;
    if(builder.busy()&&builder.gameId()==game->id){builder.cancel();return;}
    if(!running.contains(game->id))return;
    if(requestGameClose(running.value(game->id),game->executable()))jobLabel->setText("Close requested — waiting for game");
    else message("Could not close game","Close the game from its own window.");
}
void Window::openGameFile(const QString& suffix){auto* game=selected();if(!game)return;const auto path=QDir(game->buildDir).filePath(game->outputName+suffix);if(!QFileInfo::exists(path)){message("File not available","Run this game once to create its log and settings.");return;}QDesktopServices::openUrl(QUrl::fromLocalFile(path));}
void Window::openFolder(bool saves){auto* game=selected();if(!game)return;const QString path=saves?QDir(game->dataDir()).filePath("save"):game->buildDir;if(!QDir(path).exists()){message("Folder not available",saves?"This game has not created a save folder yet.":"Build this game first to create its output folder.");return;}QDesktopServices::openUrl(QUrl::fromLocalFile(path));}
void Window::copySaves(bool interactive,bool confirm){
    auto* game=selected();
    const auto done=[this,interactive](bool okay,const QString& title,const QString& detail){
        if(interactive)message(title,detail,!okay);else console->appendPlainText("\n"+title+": "+detail);
        emit saveCopyFinished(okay,detail);
    };
    if(!game||demo){done(false,"Cannot copy saves","Select a game first.");return;}
    if(running.contains(game->id)||(builder.busy()&&builder.gameId()==game->id)){done(false,"Cannot copy saves","Close the game (or wait for its build) first.");return;}
    const QString other=game->nxrs()?"Turbo":"nxrs";
    if(interactive&&confirm&&QMessageBox::question(this,"Copy saves",QString("Copy the saves of the %1 build of “%2” into this build?\n\nThis build's save folder is backed up first; the %1 build's saves are only read.").arg(other,game->title))!=QMessageBox::Yes)return;
    QString backup,report,error;
    if(!copySavesFromOtherEngine(*game,&backup,&report,&error)){done(false,"Cannot copy saves",error);return;}
    game->savesOffered=game->nxrs()?engineTurbo:engineNxrs;save();refreshSelection();
    done(true,"Saves copied",report+(backup.isEmpty()?QString("\nThis build had no saves; nothing to back up."):"\nBackup of this build's saves: "+QDir::toNativeSeparators(backup)));
}
void Window::removeGame(){
    auto* game=selected();if(!game||running.contains(game->id)||(builder.busy()&&builder.gameId()==game->id))return;
    if(QMessageBox::question(this,"Remove from library","Remove “"+game->title+"” from the library?\n\nYour package, builds, and saves will not be deleted.")!=QMessageBox::Yes)return;
    for(int i=0;i<library.games.size();++i)if(library.games[i].id==selectedId){library.games.removeAt(i);break;}
    selectedId.clear();save();refreshLibrary();
}
void Window::showContextMenu(const QPoint& point){QMenu menu(this);menu.addAction(playAction);menu.addAction(buildAction);menu.addAction(propertiesAction);menu.addSeparator();menu.addAction(folderAction);menu.addAction(saveFolderAction);menu.addAction(logAction);menu.addSeparator();menu.addAction(removeAction);menu.exec(point);}

bool Window::nativeEvent(const QByteArray& eventType,void* message,qintptr* result){
    if(titleBar&&titleBar->handleNativeEvent(eventType,message,result))return true;
    return QMainWindow::nativeEvent(eventType,message,result);
}
bool Window::eventFilter(QObject* watched,QEvent* event){
    if(watched==toolStatus&&event->type()==QEvent::MouseButtonRelease){showToolPopup();return true;}
    if(toolPopup&&watched==toolPopup&&event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape){toolPopup->close();return true;}
    return QMainWindow::eventFilter(watched,event);
}

QString Window::toolProblem() const {
    bool turboUsed=library.games.isEmpty(),nxrsUsed=false;for(const auto& g:library.games)(g.nxrs()?nxrsUsed:turboUsed)=true;
    QStringList problems;
    if(turboUsed&&!tools.canBuild())problems<<"Turbo: "+tools.problem();
    if(nxrsUsed&&!tools.canBuild(engineNxrs))problems<<"nxrs engine: "+tools.nxrsProblem();
    return problems.join("\n\n");
}
void Window::showToolPopup(){
    if(toolPopup){toolPopup->close();toolPopup.clear();}
    const QString problem=toolProblem();const bool ready=problem.isEmpty();
    auto* frame=new QFrame(this,Qt::Popup|Qt::FramelessWindowHint);
    toolPopup=frame;
    frame->setAttribute(Qt::WA_TranslucentBackground);
    frame->setAttribute(Qt::WA_DeleteOnClose);
    frame->installEventFilter(this);
    auto* outer=new QVBoxLayout(frame);outer->setContentsMargins(14,10,14,18);
    auto* card=new QFrame;card->setObjectName("FilterCard");outer->addWidget(card);
    auto* shadow=new QGraphicsDropShadowEffect(card);shadow->setBlurRadius(24);shadow->setOffset(0,6);shadow->setColor(QColor(0,0,0,190));card->setGraphicsEffect(shadow);
    auto* layout=new QVBoxLayout(card);layout->setContentsMargins(18,16,18,16);layout->setSpacing(10);
    auto* heading=new QHBoxLayout;
    auto* title=new QLabel(ready?"Build tools":"Build setup problem");title->setObjectName("PopupTitle");heading->addWidget(title);heading->addStretch();
    auto* close=new QPushButton("×");close->setAccessibleName("Close build tools popup");close->setFixedWidth(30);close->setToolTip("Close (Esc)");heading->addWidget(close);
    layout->addLayout(heading);
    if(ready){
        auto* body=new QLabel(healthText);body->setObjectName("Muted");body->setWordWrap(true);body->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(body);
    } else {
        auto* body=new QLabel(problem);body->setWordWrap(true);body->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(body);
        auto* divider=new QFrame;divider->setObjectName("FilterDivider");divider->setFixedHeight(1);layout->addWidget(divider);
        auto* health=new QLabel(healthText);health->setObjectName("Muted");health->setWordWrap(true);health->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(health);
    }
    auto* actions=new QHBoxLayout;actions->addStretch();
    auto* settings=new QPushButton("Open Settings");settings->setObjectName("AccentButton");actions->addWidget(settings);layout->addLayout(actions);
    connect(close,&QPushButton::clicked,frame,&QWidget::close);
    connect(settings,&QPushButton::clicked,frame,[this,frame]{frame->close();showSettings();});
    frame->setFixedWidth(380);frame->adjustSize();
    QPoint location=toolStatus->mapToGlobal(QPoint(toolStatus->width()-frame->width(),-frame->height()-6));
    if(auto* screen=toolStatus->screen()){
        const QRect available=screen->availableGeometry();
        location.setX(qBound(available.left(),location.x(),qMax(available.left(),available.right()-frame->width()+1)));
        location.setY(qBound(available.top(),location.y(),qMax(available.top(),available.bottom()-frame->height()+1)));
    }
    frame->move(location);frame->show();frame->raise();frame->setFocus();
}
void Window::dragEnterEvent(QDragEnterEvent* event){if(event->mimeData()->hasUrls())event->acceptProposedAction();}
void Window::dropEvent(QDropEvent* event){for(const auto& url:event->mimeData()->urls()){if(!url.isLocalFile())continue;const auto path=url.toLocalFile();Game game;QString error;if(gameFromExecutable(path,&game,&error))importBuild(path);else importSource(path);}event->acceptProposedAction();}
void Window::closeEvent(QCloseEvent* event){
    if(builder.busy()){if(!closing&&QMessageBox::question(this,"Build in progress","Cancel the build and close the launcher?")!=QMessageBox::Yes){event->ignore();return;}closing=true;builder.cancel();event->ignore();return;}
    library.geometry=saveGeometry();library.dockState=saveState(1);save();event->accept();
}
void Window::seedDemo(){
    library.games.clear();library.viewMode=0;library.coverWidth=175;
    const QStringList names{"Donkey Kong Bananza","Mario Kart World","Homebrew collection","Another game","Test application","Local collection"};
    for(int i=0;i<names.size();++i){Game game;game.id=QString::number(i);game.title=names[i];game.outputName=names[i];game.source="Games/"+names[i]+(i==2?".nro":".nup");game.buildDir="Games/Native builds/"+names[i];library.games.append(game);}selectedId="0";
}
