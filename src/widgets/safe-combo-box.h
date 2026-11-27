// Drop-down selections must not change when the user wheels past them.
// Deliberately focused selectors keep Qt's native wheel and keyboard behavior.
#pragma once

#include <QComboBox>
#include <QWheelEvent>

class SafeComboBox : public QComboBox {
public:
    explicit SafeComboBox(QWidget* parent = nullptr) : QComboBox(parent) {
        // Keep Tab and click focus, but never acquire focus from the wheel.
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (hasFocus()) QComboBox::wheelEvent(event);
        else event->ignore(); // let the enclosing QScrollArea handle scrolling
    }
};
