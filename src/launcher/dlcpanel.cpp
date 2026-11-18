// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "dlcpanel.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QShortcut>
#include <QUrl>
#include <QVBoxLayout>

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
    list->clear();
    for (const auto& package : items) {
        auto* item = new QListWidgetItem(dlcDescription(package, installed, built), list);
        item->setToolTip(QDir::toNativeSeparators(package));
    }
    if (items.isEmpty())
        note->setText("No DLC is installed for this game. Add it with Game > Install DLC… (NSP, NSZ, XCI, XCZ or NCA; "
                      "a package that also holds the game or its update works too).");
    else
        note->setText(QString(locked ? "DLC cannot be changed while the game is running or building."
                                     : "Right-click an entry and choose Remove (or press Delete) to uninstall it; it takes effect "
                                       "when you click OK. Add more with Game > Install DLC…. The original files are never changed."));
}
