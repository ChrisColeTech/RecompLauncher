// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#pragma once
#include "library.h"
#include <QDialog>
#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;

// The Updates page of a game's Properties: the update packages installed for the game (Game::updates) with their
// versions and which one the build applies. An entry is removed from its context menu or with Delete; the caller
// applies updates() when the dialog is accepted. Updates are added, and the version is chosen, in the Updates window
// (Game > Install Updates…).
class UpdatePanel : public QWidget {
    Q_OBJECT
public:
    UpdatePanel(const Game& game, bool locked, QWidget* parent = nullptr);
    QVector<GameUpdate> updates() const { return state.updates; }
    void removeRow(int row);
    QListWidget* list = nullptr;
    QLabel* note = nullptr;
private:
    void refresh();
    Game state;
    bool locked = false;
};

// Game > Install Updates…: the game's Updates window, the DLC window's twin. It lists the installed update packages
// and works on the list right away: Add update… (or dropping files on the window) takes any number of packages,
// Remove takes the selected ones off (several rows can be selected), and "Version to build" says which one applies:
// the newest (the default), a particular one, or none. An update replaces the game's code, so a change here needs
// the game built again: the window says so and Build now starts it. The window only asks; the main window does the
// work and reports back through setGame, setBusy and setStatus.
class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    UpdateDialog(const Game& game, bool locked, QWidget* parent = nullptr);
    void setGame(const Game& game);
    void setBusy(bool busy, const QString& text = {});
    void setStatus(const QString& text, bool error = false);
    void addFiles(const QStringList& files);
    void removeSelected();
    QStringList selectedPackages() const;
    QListWidget* list = nullptr;
    QComboBox* choice = nullptr;
    QPushButton *addButton = nullptr, *removeButton = nullptr, *buildButton = nullptr, *closeButton = nullptr;
    QLabel *hint = nullptr, *status = nullptr, *pending = nullptr;
    QProgressBar* progress = nullptr;
signals:
    void addRequested(const QStringList& files);
    void removeRequested(const QStringList& packages);
    void choiceRequested(const QString& choice);   // "" newest, "none", or a package path
    void buildRequested();
protected:
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
private:
    void refresh();
    void updateButtons();
    Game state;
    QString startDir;
    bool locked = false, busy = false, canBuild = true;
};
