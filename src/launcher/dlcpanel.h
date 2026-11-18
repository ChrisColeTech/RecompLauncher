// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#pragma once
#include "library.h"
#include <QDialog>
#include <QWidget>

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;

// The DLC page of a game's Properties: the add-on content packages installed for the game (Game::dlc) with what the
// engine installed from each (title id and version, from the data folder's dlc.manifest). An entry is removed from
// its context menu or with Delete; the caller applies packages() when the dialog is accepted. Packages are added
// in the DLC window (Game > Install DLC…), not here.
class DlcPanel : public QWidget {
    Q_OBJECT
public:
    DlcPanel(const Game& game, bool locked, QWidget* parent = nullptr);
    QStringList packages() const { return items; }
    void removeRow(int row);
    QListWidget* list = nullptr;
    QLabel* note = nullptr;
private:
    void refresh();
    QStringList items;
    QVector<DlcTitle> installed;
    bool built = false, locked = false;
};

// Game > Install DLC…: the game's DLC window. It lists the installed packages and works on them right away:
// Add DLC… (or dropping files on the window) takes any number of packages, Remove uninstalls the selected ones
// (several rows can be selected). The window only asks; the main window does the work (the list in library.json,
// the engine run) and reports back through setGame, setBusy and setStatus.
class DlcDialog : public QDialog {
    Q_OBJECT
public:
    DlcDialog(const Game& game, bool locked, QWidget* parent = nullptr);
    void setGame(const Game& game);                              // the list changed or an engine run finished
    void setBusy(bool busy, const QString& text = {});           // an engine run is in progress: no changes meanwhile
    void setStatus(const QString& text, bool error = false);     // the outcome of the last action, under the list
    void addFiles(const QStringList& files);                     // what the picker or a drop delivered
    void removeSelected();
    QStringList selectedPackages() const;
    QListWidget* list = nullptr;
    QPushButton *addButton = nullptr, *removeButton = nullptr, *closeButton = nullptr;
    QLabel *hint = nullptr, *status = nullptr;
    QProgressBar* progress = nullptr;
signals:
    void addRequested(const QStringList& files);
    void removeRequested(const QStringList& packages);
protected:
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
private:
    void refresh();
    void updateButtons();
    QStringList items;
    QVector<DlcTitle> installed;
    QString startDir;
    bool built = false, locked = false, busy = false;
};
