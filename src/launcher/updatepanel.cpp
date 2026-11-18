// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "updatepanel.h"
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

namespace {
void fill(QListWidget* list, const Game& game) {
    list->clear();
    for (const auto& update : game.updates) {
        auto* item = new QListWidgetItem(updateDescription(game, update), list);
        item->setToolTip(QDir::toNativeSeparators(update.path));
    }
}
// What the game is built as now and what the next build will be, when they differ.
QString pendingText(const Game& game) {
    if (!game.updatePending()) return {};
    return QString("The current build is %1. Build the game again to get %2: an update replaces the game's code.")
        .arg(updateTokenLabel(game, game.builtUpdate), updateTokenLabel(game, game.updateToken()));
}
}

UpdatePanel::UpdatePanel(const Game& game, bool isLocked, QWidget* parent) : QWidget(parent), state(game), locked(isLocked) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 8);
    list = new QListWidget;
    list->setObjectName("UpdateList");
    list->setAccessibleName("Installed updates");
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->setWordWrap(true);                                         // a long file name and its state stay readable
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setResizeMode(QListView::Adjust);
    layout->addWidget(list, 1);
    note = new QLabel;
    note->setObjectName("Muted");
    note->setTextFormat(Qt::PlainText);
    note->setWordWrap(true);
    layout->addWidget(note);
    connect(list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& point) {
        auto* item = list->itemAt(point);
        if (!item) return;
        const int row = list->row(item);
        QMenu menu(this);
        auto* remove = menu.addAction("Remove");
        remove->setEnabled(!locked);
        auto* folder = menu.addAction("Open containing folder");
        folder->setEnabled(QFileInfo::exists(state.updates.value(row).path));
        const auto* chosen = menu.exec(list->viewport()->mapToGlobal(point));
        if (chosen == remove) removeRow(row);
        else if (chosen == folder) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(state.updates.value(row).path).absolutePath()));
    });
    auto* erase = new QShortcut(QKeySequence::Delete, list, [this] { removeRow(list->currentRow()); });
    erase->setContext(Qt::WidgetShortcut);
    refresh();
}

void UpdatePanel::removeRow(int row) {
    if (locked || row < 0 || row >= state.updates.size()) return;
    state.updates.removeAt(row);
    refresh();
    list->setCurrentRow(qMin(row, state.updates.size() - 1));
}

void UpdatePanel::refresh() {
    fill(list, state);
    if (state.updates.isEmpty())
        note->setText("No update is installed for this game. Add one with Game > Install Updates… (NSP, XCI or NCA; the game "
                      "itself has to be an NSP package).");
    else if (locked) note->setText("Updates cannot be changed while the game is running or building.");
    else {
        const QString pending = pendingText(state);
        note->setText("Right-click an entry and choose Remove (or press Delete) to take it off; it takes effect when you click "
                      "OK and needs the game built again. Add updates and choose the version in Game > Install Updates…"
                      + (pending.isEmpty() ? QString{} : "\n" + pending));
    }
}

UpdateDialog::UpdateDialog(const Game& game, bool isLocked, QWidget* parent) : QDialog(parent), locked(isLocked) {
    setWindowTitle("Updates — " + game.title);
    setObjectName("UpdateDialog");
    setAcceptDrops(true);
    resize(700, 430);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    auto* versionRow = new QHBoxLayout;
    auto* versionLabel = new QLabel("Version to build");
    choice = new QComboBox;
    choice->setObjectName("UpdateChoice");
    choice->setAccessibleName("Version to build");
    choice->setToolTip("Which update the game is built with. One update applies at a time; the newest is the default.");
    versionRow->addWidget(versionLabel);
    versionRow->addWidget(choice, 1);
    layout->addLayout(versionRow);
    list = new QListWidget;
    list->setObjectName("UpdateList");
    list->setAccessibleName("Installed updates");
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->setWordWrap(true);                                         // a long file name and its state stay readable
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setResizeMode(QListView::Adjust);
    layout->addWidget(list, 1);
    hint = new QLabel;
    hint->setObjectName("Muted");
    hint->setTextFormat(Qt::PlainText);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    pending = new QLabel;
    pending->setObjectName("UpdatePending");
    pending->setTextFormat(Qt::PlainText);
    pending->setWordWrap(true);
    pending->hide();
    layout->addWidget(pending);
    status = new QLabel;
    status->setObjectName("UpdateStatus");
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status->hide();
    layout->addWidget(status);
    progress = new QProgressBar;
    progress->setAccessibleName("Update check progress");
    progress->setRange(0, 0);
    progress->setTextVisible(false);
    progress->setFixedHeight(6);
    progress->hide();
    layout->addWidget(progress);
    auto* buttons = new QHBoxLayout;
    addButton = new QPushButton("Add update…");
    addButton->setObjectName("AccentButton");
    addButton->setToolTip("Install one or more update packages (NSP, XCI, NCA)");
    removeButton = new QPushButton("Remove");
    removeButton->setToolTip("Take the selected updates off the list (Delete). The original files are not changed.");
    buildButton = new QPushButton("Build now");
    buildButton->setToolTip("Build the game again with the version chosen above");
    closeButton = new QPushButton("Close");
    for (auto* button : {addButton, removeButton, buildButton, closeButton}) button->setAutoDefault(false);
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();
    buttons->addWidget(buildButton);
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);
    connect(addButton, &QPushButton::clicked, this, [this] {
        addFiles(QFileDialog::getOpenFileNames(this, "Add update", startDir, updateFileFilter));
    });
    connect(removeButton, &QPushButton::clicked, this, &UpdateDialog::removeSelected);
    connect(buildButton, &QPushButton::clicked, this, [this] { if (!locked && !busy) emit buildRequested(); });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(choice, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        if (!locked && !busy) emit choiceRequested(choice->itemData(index).toString());
    });
    connect(list, &QListWidget::itemSelectionChanged, this, &UpdateDialog::updateButtons);
    connect(list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& point) {
        auto* item = list->itemAt(point);
        if (!item) return;
        if (!item->isSelected()) { list->clearSelection(); item->setSelected(true); }
        const int row = list->row(item);
        const QString path = state.updates.value(row).path;
        QMenu menu(this);
        auto* use = menu.addAction("Build with this update");
        use->setEnabled(!locked && !busy && list->selectedItems().size() == 1);
        auto* remove = menu.addAction(list->selectedItems().size() > 1 ? QString("Remove %1 updates").arg(list->selectedItems().size()) : QString("Remove"));
        remove->setEnabled(removeButton->isEnabled());
        auto* folder = menu.addAction("Open containing folder");
        folder->setEnabled(QFileInfo::exists(path));
        const auto* chosen = menu.exec(list->viewport()->mapToGlobal(point));
        if (chosen == use) emit choiceRequested(path);
        else if (chosen == remove) removeSelected();
        else if (chosen == folder) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
    auto* erase = new QShortcut(QKeySequence::Delete, list, [this] { removeSelected(); });
    erase->setContext(Qt::WidgetShortcut);
    setGame(game);
}

void UpdateDialog::setGame(const Game& game) {
    const QStringList chosen = selectedPackages();
    state = game;
    startDir = state.updates.isEmpty() ? QFileInfo(state.source).absolutePath() : QFileInfo(state.updates.last().path).absolutePath();
    refresh();
    for (int row = 0; row < state.updates.size(); ++row)
        for (const auto& package : chosen)
            if (samePath(package, state.updates[row].path)) list->item(row)->setSelected(true);
    updateButtons();
}

void UpdateDialog::setBusy(bool isBusy, const QString& text) {
    busy = isBusy;
    progress->setVisible(busy);
    if (busy && !text.isEmpty()) setStatus(text);
    updateButtons();
}

void UpdateDialog::setStatus(const QString& text, bool error) {
    status->setText(text);
    status->setVisible(!text.isEmpty());
    status->setProperty("error", error);
    status->setStyleSheet(error ? "color: #ff7a7a;" : QString{});
}

void UpdateDialog::addFiles(const QStringList& files) {
    if (files.isEmpty() || locked || busy) return;
    emit addRequested(files);
}

QStringList UpdateDialog::selectedPackages() const {
    QStringList packages;
    if (!list) return packages;
    for (int row = 0; row < list->count() && row < state.updates.size(); ++row)
        if (list->item(row)->isSelected()) packages << state.updates[row].path;
    return packages;
}

void UpdateDialog::removeSelected() {
    const QStringList packages = selectedPackages();
    if (packages.isEmpty() || locked || busy) return;
    emit removeRequested(packages);
}

void UpdateDialog::refresh() {
    fill(list, state);
    {
        const QSignalBlocker quiet(choice);
        choice->clear();
        const GameUpdate* newest = nullptr;
        for (const auto& update : state.updates) if (!newest || update.version > newest->version) newest = &update;
        choice->addItem(newest ? "Newest installed update — " + newest->label() : QString("Newest installed update"), QString());
        for (const auto& update : state.updates) choice->addItem(update.label() + " — " + QFileInfo(update.path).fileName(), update.path);
        choice->addItem("The game without updates", QString("none"));
        int index = 0;
        if (state.updateChoice == "none") index = choice->count() - 1;
        else if (!state.updateChoice.isEmpty())
            for (int i = 0; i < state.updates.size(); ++i) if (samePath(state.updates[i].path, state.updateChoice)) index = i + 1;
        choice->setCurrentIndex(index);
    }
    if (locked) hint->setText("Updates cannot be changed while the game is running or building.");
    else if (state.updates.isEmpty())
        hint->setText("No update is installed for this game. Click Add update… or drop files here: NSP, XCI or NCA, several at "
                      "once. The game itself has to be an NSP package; a package that holds another game's update is refused.");
    else
        hint->setText("Add update… installs more packages, several at once. Select one or more entries and click Remove to take "
                      "them off. One update applies at a time: the newest, unless Version to build says otherwise. The original "
                      "files are never changed; the update's data is read from its package while you play, so keep it in place.");
    const QString text = pendingText(state);
    pending->setText(text);
    pending->setVisible(!text.isEmpty());
    canBuild = !text.isEmpty();
}

void UpdateDialog::updateButtons() {
    addButton->setEnabled(!locked && !busy);
    removeButton->setEnabled(!locked && !busy && !list->selectedItems().isEmpty());
    choice->setEnabled(!locked && !busy);
    buildButton->setEnabled(!locked && !busy && canBuild);
}

void UpdateDialog::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls() && !locked && !busy) event->acceptProposedAction();
}

void UpdateDialog::dropEvent(QDropEvent* event) {
    QStringList files;
    for (const auto& url : event->mimeData()->urls())
        if (url.isLocalFile()) files << url.toLocalFile();
    event->acceptProposedAction();
    addFiles(files);
}
