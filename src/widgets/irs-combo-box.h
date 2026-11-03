// ViPER4Linux-RE: IRS selector with immutable popup contents while open.
// Extracted unchanged from viper-gui.cpp; library refresh remains in the GUI.
#pragma once

#include <QAbstractItemView>
#include <QComboBox>
#include <QSizePolicy>
#include <functional>

// The IRS contents stay immutable while its popup is open. If the library
// changes during a selection, the next popup opening reconciles the list.
// This also handles a directory event delivered immediately after closing.
class IrsComboBox final : public QComboBox {
public:
    explicit IrsComboBox(QWidget* parent) : QComboBox(parent) {
        // A single very long filename must NEVER set the minimum form width.
        // The full value is preserved in itemData and the tooltip/detail label.
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        setMinimumContentsLength(12);
        setMinimumWidth(90);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        view()->setTextElideMode(Qt::ElideMiddle);
    }
    std::function<void()> beforeOpen;
    [[nodiscard]] bool popupIsOpen() const { return popup_open_; }
protected:
    void showPopup() override {
        if (beforeOpen) beforeOpen();
        popup_open_ = true;
        QComboBox::showPopup();
    }
    void hidePopup() override {
        QComboBox::hidePopup();
        popup_open_ = false;
    }
private:
    bool popup_open_ = false;
};
