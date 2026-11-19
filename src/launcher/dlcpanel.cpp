// nxrs - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "dlcpanel.h"
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
#include <QUrl>
#include <QVBoxLayout>

namespace {
void fill(QListWidget* list, const QStringList& packages, const QVector<DlcTitle>& installed, bool built) {
    list->clear();
    for (const auto& package : packages) {
        auto* item = new QListWidgetItem(dlcDescription(package, installed, built), list);
        item->setToolTip(QDir::toNativeSeparators(package));
    }
}
}

DlcPanel::DlcPanel(const Game& game, bool isLocked, QWidget* parent)
    : QWidget(parent), items(game.dlc), installed(readDlcManifest(game.dataDir())), built(game.hasBuild()), locked(isLocked) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 8);
    list = new QListWidget;
    list->setObjectName("DlcList");
    list->setAccessibleName("Installed DLC");
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->setTextElideMode(Qt::ElideMiddle);
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
        folder->setEnabled(QFileInfo::exists(items.value(row)));
        const auto* chosen = menu.exec(list->viewport()->mapToGlobal(point));
        if (chosen == remove) removeRow(row);
        else if (chosen == folder) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(items.value(row)).absolutePath()));
    });
    auto* erase = new QShortcut(QKeySequence::Delete, list, [this] { removeRow(list->currentRow()); });
    erase->setContext(Qt::WidgetShortcut);
    refresh();
}

void DlcPanel::removeRow(int row) {
    if (locked || row < 0 || row >= items.size()) return;
    items.removeAt(row);
    refresh();
    list->setCurrentRow(qMin(row, items.size() - 1));
}

void DlcPanel::refresh() {
    fill(list, items, installed, built);
    if (items.isEmpty())
        note->setText("No DLC is installed for this game. Add it with Game > Install DLC… (NSP, NSZ, XCI, XCZ or NCA; "
                      "a package that also holds the game or its update works too).");
    else
        note->setText(QString(locked ? "DLC cannot be changed while the game is running or building."
                                     : "Right-click an entry and choose Remove (or press Delete) to uninstall it; it takes effect "
                                       "when you click OK. The original files are never changed. Add more with Game > Install DLC…"));
}

DlcDialog::DlcDialog(const Game& game, bool isLocked, QWidget* parent) : QDialog(parent), locked(isLocked) {
    setWindowTitle("DLC — " + game.title);
    setObjectName("DlcDialog");
    setAcceptDrops(true);
    resize(680, 400);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    list = new QListWidget;
    list->setObjectName("DlcList");
    list->setAccessibleName("Installed DLC");
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);   // Ctrl / Shift pick several to remove at once
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->setTextElideMode(Qt::ElideMiddle);
    layout->addWidget(list, 1);
    hint = new QLabel;
    hint->setObjectName("Muted");
    hint->setTextFormat(Qt::PlainText);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    status = new QLabel;
    status->setObjectName("DlcStatus");
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status->hide();
    layout->addWidget(status);
    progress = new QProgressBar;
    progress->setAccessibleName("DLC installation progress");
    progress->setRange(0, 0);
    progress->setTextVisible(false);
    progress->setFixedHeight(6);
    progress->hide();
    layout->addWidget(progress);
    auto* buttons = new QHBoxLayout;
    addButton = new QPushButton("Add DLC…");
    addButton->setObjectName("AccentButton");
    addButton->setToolTip("Install one or more DLC packages (NSP, NSZ, XCI, XCZ, NCA)");
    removeButton = new QPushButton("Remove");
    removeButton->setToolTip("Uninstall the selected DLC (Delete). The original files are not changed.");
    closeButton = new QPushButton("Close");
    for (auto* button : {addButton, removeButton, closeButton}) button->setAutoDefault(false);
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);
    connect(addButton, &QPushButton::clicked, this, [this] {
        addFiles(QFileDialog::getOpenFileNames(this, "Add DLC", startDir, dlcFileFilter));
    });
    connect(removeButton, &QPushButton::clicked, this, &DlcDialog::removeSelected);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(list, &QListWidget::itemSelectionChanged, this, &DlcDialog::updateButtons);
    connect(list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& point) {
        auto* item = list->itemAt(point);
        if (!item) return;
        if (!item->isSelected()) { list->clearSelection(); item->setSelected(true); }
        const int row = list->row(item);
        QMenu menu(this);
        auto* remove = menu.addAction(list->selectedItems().size() > 1 ? QString("Remove %1 packages").arg(list->selectedItems().size()) : QString("Remove"));
        remove->setEnabled(removeButton->isEnabled());
        auto* folder = menu.addAction("Open containing folder");
        folder->setEnabled(QFileInfo::exists(items.value(row)));
        const auto* chosen = menu.exec(list->viewport()->mapToGlobal(point));
        if (chosen == remove) removeSelected();
        else if (chosen == folder) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(items.value(row)).absolutePath()));
    });
    auto* erase = new QShortcut(QKeySequence::Delete, list, [this] { removeSelected(); });
    erase->setContext(Qt::WidgetShortcut);
    setGame(game);
}

void DlcDialog::setGame(const Game& game) {
    const QStringList chosen = selectedPackages();
    items = game.dlc;
    installed = readDlcManifest(game.dataDir());
    built = game.hasBuild();
    startDir = items.isEmpty() ? QFileInfo(game.source).absolutePath() : QFileInfo(items.last()).absolutePath();
    refresh();
    for (int row = 0; row < items.size(); ++row)
        for (const auto& package : chosen)
            if (samePath(package, items[row])) list->item(row)->setSelected(true);
    updateButtons();
}

void DlcDialog::setBusy(bool isBusy, const QString& text) {
    busy = isBusy;
    progress->setVisible(busy);
    if (busy && !text.isEmpty()) setStatus(text);
    updateButtons();
}

void DlcDialog::setStatus(const QString& text, bool error) {
    status->setText(text);
    status->setVisible(!text.isEmpty());
    status->setProperty("error", error);
    status->setStyleSheet(error ? "color: #ff7a7a;" : QString{});
}

void DlcDialog::addFiles(const QStringList& files) {
    if (files.isEmpty() || locked || busy) return;
    emit addRequested(files);
}

QStringList DlcDialog::selectedPackages() const {
    QStringList packages;
    if (!list) return packages;
    for (int row = 0; row < list->count() && row < items.size(); ++row)
        if (list->item(row)->isSelected()) packages << items[row];
    return packages;
}

void DlcDialog::removeSelected() {
    const QStringList packages = selectedPackages();
    if (packages.isEmpty() || locked || busy) return;
    emit removeRequested(packages);
}

void DlcDialog::refresh() {
    fill(list, items, installed, built);
    if (locked) hint->setText("DLC cannot be changed while the game is running or building.");
    else if (items.isEmpty())
        hint->setText("No DLC is installed for this game. Click Add DLC… or drop files here: NSP, NSZ, XCI, XCZ or NCA, "
                      "several at once; a package that also holds the game or its update works too.");
    else
        hint->setText(QString(built ? "Add DLC… installs more packages, several at once. Select one or more entries and click Remove "
                                      "to uninstall them. The original files are never changed."
                                    : "These packages are installed when the game is built. Select entries and click Remove to "
                                      "take them off the list."));
}

void DlcDialog::updateButtons() {
    addButton->setEnabled(!locked && !busy);
    removeButton->setEnabled(!locked && !busy && !list->selectedItems().isEmpty());
}

void DlcDialog::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls() && !locked && !busy) event->acceptProposedAction();
}

void DlcDialog::dropEvent(QDropEvent* event) {
    QStringList files;
    for (const auto& url : event->mimeData()->urls())
        if (url.isLocalFile()) files << url.toLocalFile();
    event->acceptProposedAction();
    addFiles(files);
}
