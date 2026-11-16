// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#pragma once
#include <QFrame>
class QComboBox;
class QLabel;
class QLineEdit;
class QKeyEvent;
class QToolButton;
class QSlider;

class FilterPopup : public QFrame {
    Q_OBJECT
public:
    explicit FilterPopup(QWidget* parent);
    void openBelow(QWidget* anchor);
    void setResultCount(int visible, int total);
    void clearFilters();
    QLineEdit* search = nullptr;
    QComboBox *state = nullptr, *source = nullptr, *sort = nullptr;
    QToolButton *galleryButton = nullptr, *listButton = nullptr;
    QSlider* coverSize = nullptr;
signals:
    void filtersChanged();
protected:
    void keyPressEvent(QKeyEvent* event) override;
private:
    QLabel* summary = nullptr;
};
