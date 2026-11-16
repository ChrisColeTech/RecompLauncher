// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#pragma once
#include "buildmanager.h"
#include <QMainWindow>
#include <QMap>

class LibraryCanvas;
class GameCard;
class FilterPopup;
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
    void showAppearance();
signals:
    void smokeFinished(bool success);
    void toolsChanged();
protected:
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    void closeEvent(QCloseEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;
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
    void showContextMenu(const QPoint& global);
    void showToolPopup();
    void message(const QString& title, const QString& text, bool error = false);
    void seedDemo();
    Game* selected();
    QString statusFor(const Game& game) const;
    Library library;
    Toolchain tools;
    BuildManager builder;
    QString selectedId, healthText;
    bool demo = false, closing = false, dotnetReady = false;
    int toolsGeneration = 0;
    QMap<QString, qint64> running;
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
    QString buildPhase;
    int buildCurrent = 0, buildTotal = 0;
    QAction *addAction = nullptr, *addFolderAction = nullptr, *importAction = nullptr, *playAction = nullptr,
        *buildAction = nullptr, *stopAction = nullptr, *propertiesAction = nullptr, *removeAction = nullptr,
        *folderAction = nullptr, *logAction = nullptr, *saveFolderAction = nullptr, *settingsAction = nullptr,
        *galleryAction = nullptr, *tableAction = nullptr, *backgroundAction = nullptr, *buildOptionsAction = nullptr,
        *installKeysAction = nullptr;
};
