// nxrs - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "window.h"
#include "theme.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QImageReader>
#include <QStandardPaths>
#include <QTimer>
#include <QDirIterator>
#include <QFile>

// Earlier builds kept the library and installed keys under RecompLauncher/RecompLauncher and, before that,
// RecompLauncher/SwitchRecompLauncher (org/app). Copy the newest of those once into nxrs; the old folders
// are left untouched.
static void migrateLegacyData() {
    const QDir current(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if (QFileInfo::exists(current.filePath("library.json"))) return;
    for (const char* old : {"../RecompLauncher/RecompLauncher", "../RecompLauncher/SwitchRecompLauncher"}) {
        const QDir legacy(current.absoluteFilePath(QString::fromLatin1(old)));
        if (!legacy.exists()) continue;
        QDirIterator it(legacy.absolutePath(), QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString source = it.next();
            const QString target = current.filePath(legacy.relativeFilePath(source));
            QDir().mkpath(QFileInfo(target).absolutePath());
            if (!QFileInfo::exists(target)) QFile::copy(source, target);
        }
        return;
    }
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QString{});   // no organization level: the data folder is %LOCALAPPDATA%\nxrs
    QApplication::setApplicationName("nxrs");
    QApplication::setApplicationVersion(NXRS_VERSION);
    QApplication::setStyle("Fusion");
    Theme::apply(app);
    QFont font("Segoe UI", 9); app.setFont(font);
    QApplication::setWindowIcon(Theme::appIcon());
    QCommandLineParser parser;     parser.setApplicationDescription("nxrs — native game library and build launcher");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"root", "Vendored engine (Switch2RECOMP) directory", "directory"});
    parser.addOption({"library", "Use a specific library file", "file"});
    parser.addOption({"import-build", "Import an existing native game executable", "exe"});
    parser.addOption({"demo", "Show a non-persistent preview library"});
    parser.addOption({"list-view", "Start in the details-list view"});
    parser.addOption({"show-console", "Show the build-output dock"});
    parser.addOption({"show-properties", "Open the selected game's Properties dialog"});
    parser.addOption({"show-filters", "Open the library search and filter popout"});
    parser.addOption({"preview-theme", "Preview dark or light mode without saving", "mode"});
    parser.addOption({"preview-accent", "Preview an accent without saving", "color"});
    parser.addOption({"select-title", "Select a library game by display title", "title"});
    parser.addOption({"check-image", "Validate an artwork image using deployed Qt plugins", "file"});
    parser.addOption({"show-appearance", "Open appearance settings"});
    parser.addOption({"show-controller", "Open controller settings"});
    parser.addOption({"properties-tab", "Tab for --show-properties (0 General, 1 Build, 2 Runtime, 3 Artwork, 4 DLC, 5 Updates)", "index"});
    parser.addOption({"install-dlc", "Install a DLC package for the --select-title game and exit (repeatable)", "package"});
    parser.addOption({"remove-dlc", "Remove a DLC package from the --select-title game and exit (repeatable)", "package"});
    parser.addOption({"install-update", "Add an update package to the --select-title game and exit (repeatable)", "package"});
    parser.addOption({"remove-update", "Remove an update package from the --select-title game and exit (repeatable)", "package"});
    parser.addOption({"update-choice", "Version to build for the --select-title game: newest, none or an installed package", "choice"});
    parser.addOption({"show-updates", "Open the selected game's Updates window (Game > Install Updates…)"});
    parser.addOption({"build", "Build the --select-title game with its saved options and exit"});
    parser.addOption({"engine", "Engine of the --select-title game, saved: turbo or nxrs (before --build and the DLC/update options)", "engine"});
    parser.addOption({"nxrs-build", "nxrs-build to use instead of the bundled one, saved in the library (empty: the bundled one)", "exe"});
    parser.addOption({"show-dlc", "Open the selected game's DLC window (Game > Install DLC…)"});
    parser.addOption({"show-menu", "Open a menu of the title bar (File, Game, Keys, View, Help)", "name"});
    parser.addOption({"screenshot", "Save a launcher preview and exit", "png"});
    parser.addOption({"screenshot-delay", "Milliseconds before --screenshot grabs the window (default 1100)", "ms"});
    parser.addOption({"show-engine-settings", "Open Settings > Engine (nxrs-build doctor runs there)"});
    parser.addOption({"show-play-settings", "Open Settings > Play (screen mode, background pause and mute: every game)"});
    parser.addOption({"nxrs-doctor", "Run nxrs-build doctor for the configured nxrs-build, print its checks and exit (0 = all ok)"});
    parser.addOption({"copy-saves", "Copy the --select-title game's saves from its build with the other engine (backup first) and exit"});
    parser.addOption({"smoke-build", "Build a synthetic test input and exit (diagnostics)", "input"});
    parser.addOption({"build-output", "Output folder for --smoke-build", "directory"});
    parser.process(app);
    // a library of its own (--library: tests, a portable library) leaves the default data folder alone
    if(!parser.isSet("library"))migrateLegacyData();
    if(parser.isSet("check-image")){QImageReader reader(parser.value("check-image"));const auto image=reader.read();if(image.isNull()){qCritical("Artwork decode failed: %s",qPrintable(reader.errorString()));return 1;}qInfo("Artwork decoded: %d x %d",image.width(),image.height());return 0;}
    QString libraryPath=parser.value("library");
    if(libraryPath.isEmpty()) libraryPath=QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("library.json");
    Window window(libraryPath,parser.value("root"),parser.isSet("demo"));
    if(parser.isSet("smoke-build") && (parser.value("build-output").isEmpty() || parser.value("library").isEmpty())) return 2;
    window.show();
    if(parser.isSet("import-build")) window.importBuild(parser.value("import-build"));
    if(parser.isSet("nxrs-build"))window.setNxrsBuild(parser.value("nxrs-build"));
    if(parser.isSet("select-title"))window.selectTitle(parser.value("select-title"));
    if(parser.isSet("engine")&&!window.setEngine(parser.value("engine"))){qCritical("Cannot set the engine: select a game that is not running or building, and use turbo or nxrs");return 2;}
    if(parser.isSet("preview-theme")||parser.isSet("preview-accent"))window.setAppearance(parser.value("preview-theme").isEmpty()?"dark":parser.value("preview-theme"),parser.value("preview-accent").isEmpty()?"lime":parser.value("preview-accent"));
    if(parser.isSet("list-view")) window.setViewMode(1);
    if(parser.isSet("show-console")) window.showBuildOutput();
    if(parser.isSet("show-properties")) QTimer::singleShot(0,&window,[&window,tab=parser.value("properties-tab").toInt()]{window.showProperties(tab);});
    if(parser.isSet("install-dlc")||parser.isSet("remove-dlc")) {
        // the DLC window's Add and Remove without dialogs and message boxes; the outcome is the exit code and one line per title or reason
        QObject::connect(&window,&Window::dlcFinished,&app,[](bool okay,const QString& detail){
            for(const auto& line:detail.split('\n',Qt::SkipEmptyParts)){if(okay)qInfo("DLC: %s",qPrintable(line));else qCritical("DLC error: %s",qPrintable(line));}
            QApplication::exit(okay?0:1);
        });
        QTimer::singleShot(0,&window,[&]{
            if(parser.isSet("remove-dlc"))window.removeDlc(parser.values("remove-dlc"));
            else window.installDlcPackages(parser.values("install-dlc"),false);
        });
    }
    if(parser.isSet("install-update")||parser.isSet("remove-update")||parser.isSet("update-choice")) {
        // the Updates window's actions without dialogs and message boxes; the outcome is the exit code and the window's status text
        QObject::connect(&window,&Window::updateFinished,&app,[](bool okay,const QString& detail){
            for(const auto& line:detail.split('\n',Qt::SkipEmptyParts)){if(okay)qInfo("Updates: %s",qPrintable(line));else qCritical("Updates error: %s",qPrintable(line));}
            QApplication::exit(okay?0:1);
        });
        QTimer::singleShot(0,&window,[&]{
            if(parser.isSet("remove-update"))window.removeUpdates(parser.values("remove-update"));
            else if(parser.isSet("update-choice"))window.chooseUpdate(parser.value("update-choice"));
            else window.installUpdatePackages(parser.values("install-update"),false);
        });
    }
    if(parser.isSet("build")) {
        QObject::connect(&window,&Window::smokeFinished,&app,[](bool okay){QApplication::exit(okay?0:1);});
        QTimer::singleShot(0,&window,[&window]{window.startBuild();});
    }
    if(parser.isSet("show-updates")) QTimer::singleShot(0,&window,[&window]{window.showUpdates();});
    if(parser.isSet("show-dlc")) QTimer::singleShot(0,&window,[&window]{window.showDlc();});
    if(parser.isSet("show-menu")) QTimer::singleShot(300,&window,[&window,name=parser.value("show-menu")]{window.showMenu(name);});
    if(parser.isSet("show-filters")) QTimer::singleShot(0,&window,[&window]{window.showFilters();});
    if(parser.isSet("show-appearance")) QTimer::singleShot(0,&window,[&window]{window.showAppearance();});
    if(parser.isSet("show-controller")) QTimer::singleShot(0,&window,[&window]{window.showControllerSettings();});
    if(parser.isSet("show-engine-settings")) QTimer::singleShot(0,&window,[&window]{window.showEngineSettings();});
    if(parser.isSet("show-play-settings")) QTimer::singleShot(0,&window,[&window]{window.showPlaySettings();});
    if(parser.isSet("nxrs-doctor")) {
        QTimer::singleShot(0,&window,[&window]{
            const auto d=window.nxrsDoctor();
            for(const auto& line:d.text().split('\n'))qInfo("%s",qPrintable(line));
            QApplication::exit(d.ok()?0:1);
        });
    }
    if(parser.isSet("copy-saves")) {
        QObject::connect(&window,&Window::saveCopyFinished,&app,[](bool okay,const QString& detail){
            for(const auto& line:detail.split('\n',Qt::SkipEmptyParts)){if(okay)qInfo("Saves: %s",qPrintable(line));else qCritical("Saves error: %s",qPrintable(line));}
            QApplication::exit(okay?0:1);
        });
        QTimer::singleShot(0,&window,[&window]{window.copySaves(false);});
    }
    if(parser.isSet("smoke-build")) {
        QObject::connect(&window,&Window::smokeFinished,&app,[](bool okay){QApplication::exit(okay?0:1);});
        QTimer::singleShot(0,&window,[&] {
            if(!window.importSource(parser.value("smoke-build"),false,parser.value("build-output"))) { QApplication::exit(2); return; }
            window.startBuild();
        });
    }
    if(parser.isSet("screenshot")) {
        QTimer::singleShot(parser.isSet("screenshot-delay")?parser.value("screenshot-delay").toInt():1100,&window,[&] {
            const QString path=QFileInfo(parser.value("screenshot")).absoluteFilePath();
            QDir().mkpath(QFileInfo(path).absolutePath());
            QWidget* target=QApplication::activeModalWidget();if(!target)target=QApplication::activePopupWidget();if(!target)target=&window;
            auto image=target->grab().toImage();
            if(parser.isSet("show-menu")&&target!=&window){
                // a menu is its own top-level window: show it where it hangs, over the main window
                QImage whole=window.grab().toImage();
                QPainter painter(&whole);painter.drawImage(window.mapFromGlobal(target->mapToGlobal(QPoint(0,0))),image);painter.end();
                image=whole;
            }
            if(image.width()>1600)image=image.scaledToWidth(1600,Qt::SmoothTransformation); image.setDevicePixelRatio(1);
            QApplication::exit(image.save(path)?0:1);
        });
    }
    return app.exec();
}
