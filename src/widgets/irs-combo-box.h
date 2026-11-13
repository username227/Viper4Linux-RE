// ViPER4Linux-RE: IRS/DDC selector with immutable popup contents while open.
// The shared file-list refresh is used by both correction libraries.
#pragma once

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QList>
#include <QPair>
#include <QSignalBlocker>
#include <QStringList>
#include <QSizePolicy>
#include <algorithm>
#include <functional>

// Library contents stay immutable while their popup is open. If the files
// change during a selection, the next popup opening reconciles the list.
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
    // Reconcile a correction-library dropdown without emitting selection
    // changes (which would otherwise trigger an unnecessary DSP reload).
    // The caller defers this while the popup is open and owns the detail label.
    void refreshFiles(const QString& directory, const QStringList& filters,
                      const QString& selected, QFileSystemWatcher* watcher) {
        QDir().mkpath(directory);
        if (watcher && !watcher->directories().contains(directory))
            watcher->addPath(directory);
        const QDir dir(directory);
        QList<QPair<QString, QString>> entries;
        entries.append({"None", QString()});
        for (const QString& name : dir.entryList(filters, QDir::Files | QDir::Readable,
                                                 QDir::Name | QDir::IgnoreCase))
            entries.append({name, dir.absoluteFilePath(name)});
        bool selectedFound = selected.isEmpty();
        for (const auto& entry : entries) {
            if (entry.second == selected) { selectedFound = true; break; }
        }
        // Older settings may still refer to a profile outside the library.
        if (!selectedFound)
            entries.append({"External: " + QFileInfo(selected).fileName(), selected});

        bool identical = entries.size() == count();
        if (identical) {
            for (int i = 0; i < entries.size(); ++i) {
                if (itemText(i) != entries[i].first ||
                    itemData(i).toString() != entries[i].second) {
                    identical = false;
                    break;
                }
            }
        }
        const QSignalBlocker block(this);
        if (!identical) {
            clear();
            for (const auto& entry : entries) {
                const int index = count();
                addItem(entry.first, entry.second);
                // A popup may elide the name; retain the complete tooltip.
                setItemData(index, entry.first, Qt::ToolTipRole);
            }
        }
        setCurrentIndex(std::max(0, findData(selected)));
    }
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
