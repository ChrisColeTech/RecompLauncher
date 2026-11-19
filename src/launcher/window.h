// nxrs - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#pragma once
#include "buildmanager.h"
#include <QMainWindow>
#include <QMap>
#include <QPointer>

class LibraryCanvas;
class GameCard;
class FilterPopup;
class DlcDialog;
class UpdateDialog;
class QFrame;
class QMenuBar;
class TitleBar;
class QAction;
class QComboBox;
class QDockWidget;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QSlider;
class QStackedWidget;
class QTreeWidget;

class Window : public QMainWindow {
    Q_OBJECT
public:
    Window(const QString& libraryPath, const QString& root, bool demo = false);
    bool importSource(const QString& source, bool interactive = true, const QString& output = {});
    bool importBuild(const QString& executable);
    void startBuild();
    void setViewMode(int mode);
    void showBuildOutput();
    void showProperties(int tab = 0);
    void showFilters();
    void setAppearance(const QString& mode, const QString& accent);
    void selectTitle(const QString& title);
    // The selected game's engine ("turbo" or "nxrs"; --engine): its build folder follows (Properties > Build > Engine).
    bool setEngine(const QString& engine);
    // nxrs-build to use instead of the bundled one (Settings > Engine; --nxrs-build); empty = the bundled one.
    void setNxrsBuild(const QString& path);
    void showAppearance();
    void showControllerSettings();
    void showMenu(const QString& name);   // pops a menu of the title bar open by its name ("Game")
    // Add DLC… of the DLC window without its file dialog: adds the packages to the selected game and installs them
    // into its build. Not interactive (--install-dlc): no message boxes, the outcome arrives with dlcFinished.
    bool installDlcPackages(const QStringList& files, bool interactive = true);
    // Remove of the DLC window for the selected game, without message boxes (--remove-dlc): dlcFinished reports.
    void removeDlc(const QStringList& packages);
    void showDlc() { installDlc(); }     // Game > Install DLC…: the game's DLC window
    // Game > Install Updates…: the game's Updates window, and its actions without dialogs or message boxes
    // (--install-update, --remove-update, --update-choice): updateFinished reports.
    void showUpdates() { installUpdates(); }
    bool installUpdatePackages(const QStringList& files, bool interactive = true);
    void removeUpdates(const QStringList& packages);
    void chooseUpdate(const QString& choice);   // "newest", "none" or a package path
signals:
    void smokeFinished(bool success);
    void dlcFinished(bool success, const QString& detail);
    void updateFinished(bool success, const QString& detail);
    void toolsChanged();
protected:
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    void closeEvent(QCloseEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
private:
    void createActions();
    void createMenusAndToolbar();
    void createLibrary();
    void createBuildDock();
    void createStatusBar();
    void refreshLibrary();
    void selectGame(const QString& id);
    void refreshSelection();
    void refreshTools();
    void save();
    void play();
    void activateGame();
    void stop();
    void cardAction(const QString& id);
    void openGameFile(const QString& suffix);
    void openFolder(bool saves = false);
    bool editGame(Game& game, int tab = 0, bool adding = false);
    void showSettings(int tab = 0);
    void installKeys();
    void removeGame();
    void installDlc();
    enum DlcReason { DlcInstalled, DlcBuilt, DlcPlay, DlcChanged };
    bool startDlcSync(const QString& id, DlcReason reason);
    void removeDlcPackages(const QString& id, const QStringList& packages);
    void installUpdates();
    void removeUpdatePackages(const QString& id, const QStringList& packages);
    void setUpdateChoice(const QString& id, const QString& choice);
    void updateReport(const QString& title, const QString& text, bool error);
    QString updateOutcome(const Game& game) const;   // what the next build will be, and whether it is needed
    void dlcReport(const QString& title, const QString& text, bool error);
    void showContextMenu(const QPoint& global);
    void showToolPopup();
    QString toolProblem() const;   // what keeps the engines the library uses from building; empty when nothing does
    void message(const QString& title, const QString& text, bool error = false);
    void seedDemo();
    Game* selected();
    QString statusFor(const Game& game) const;
    Library library;
    Toolchain tools;
    BuildManager builder;
    DlcInstaller dlcInstaller;
    UpdateProbe updateProbe;
    bool updateQuiet = false;
    DlcReason dlcReason = DlcInstalled;
    bool dlcQuiet = false;
    bool buildDlcPending = false;   // a finished build is putting its DLC into the data folder (smokeFinished waits)
    QString selectedId, healthText;
    bool demo = false, closing = false, dotnetReady = false;
    int toolsGeneration = 0;
    QMap<QString, qint64> running;
    TitleBar* titleBar = nullptr;
    QMenuBar* menus = nullptr;
    LibraryCanvas* canvas = nullptr;
    QMap<QString,GameCard*> cards;
    QStackedWidget* views = nullptr;
    QListWidget* gallery = nullptr;
    QTreeWidget* table = nullptr;
    QLineEdit* search = nullptr;
    QComboBox* sort = nullptr;
    FilterPopup* filters = nullptr;
    QWidget* filterAnchor = nullptr;
    QAction* filterAction = nullptr;
    QLabel *empty = nullptr, *countLabel = nullptr, *selectionLabel = nullptr, *toolStatus = nullptr, *jobLabel = nullptr;
    QSlider* coverSize = nullptr;
    QDockWidget* buildDock = nullptr;
    QPlainTextEdit* console = nullptr;
    QProgressBar* progress = nullptr;
    QPointer<QFrame> toolPopup;
    QPointer<DlcDialog> dlcDialog;
    QPointer<UpdateDialog> updateDialog;
    QString buildPhase;
    int buildCurrent = 0, buildTotal = 0;
    QAction *addAction = nullptr, *addFolderAction = nullptr, *importAction = nullptr, *playAction = nullptr,
        *buildAction = nullptr, *stopAction = nullptr, *propertiesAction = nullptr, *removeAction = nullptr,
        *folderAction = nullptr, *logAction = nullptr, *saveFolderAction = nullptr, *settingsAction = nullptr,
        *galleryAction = nullptr, *tableAction = nullptr, *backgroundAction = nullptr, *buildOptionsAction = nullptr,
        *installKeysAction = nullptr, *installDlcAction = nullptr, *installUpdatesAction = nullptr;
};
