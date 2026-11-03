#pragma once
#include "viper-named-presets.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QComboBox>
#include <QWidget>
#include <QUuid>
#include <cmath>
#include <functional>

namespace viper_presets_ui {
using viper_presets::NamedPreset;
struct PresetDialogActions {
    std::function<bool(QString*)> migrateLegacyPreset;
    std::function<QList<NamedPreset>()> namedPresets;
    std::function<bool(const QString&)> acceptablePresetName;
    std::function<QString()> presetLibraryDirectory;
    std::function<void()> snapshotToPresetStaging;
    std::function<bool(const QString&, const QString&, QString*)> writeNamedPreset;
    std::function<void(const QString&)> statusMessage;
    std::function<QString()> outputMemoryDirectory;
    std::function<void()> updateAudioOutputDisplay;
    std::function<bool(const QString&, QString*)> stageNamedPreset;
    std::function<void()> restoreFromPresetStaging;
    std::function<void()> resetEffectsWithConfirmation;
};
    inline void showPresetManager(QWidget* parent, const PresetDialogActions& actions) {
        QString error;
        if (!actions.migrateLegacyPreset(&error)) {
            QMessageBox::warning(parent, "Preset Migration", error +
                "\nYour original saved preset has not been modified.");
            return;
        }
        QDialog dialog(parent);
        dialog.setWindowTitle("Effect Presets");
        dialog.setMinimumSize(450, 355);
        auto* column = new QVBoxLayout(&dialog);
        auto* description = new QLabel(
            "Save complete effect settings under different names. "
            "Loading a preset replaces the current effect settings, not your GUI preferences.", &dialog);
        description->setWordWrap(true);
        column->addWidget(description);
        auto* list = new QListWidget(&dialog);
        list->setObjectName("viperNamedPresetList");
        column->addWidget(list, 1);
        auto* buttons = new QHBoxLayout();
        auto* save = new QPushButton("Save Preset…", &dialog);
        auto* load = new QPushButton("Load", &dialog);
        auto* rename = new QPushButton("Rename", &dialog);
        auto* remove = new QPushButton("Delete", &dialog);
        auto* assign = new QPushButton("Apply to Audio Output…", &dialog);
        buttons->addWidget(save);
        buttons->addWidget(load);
        buttons->addWidget(rename);
        buttons->addWidget(remove);
        column->addLayout(buttons);
        column->addWidget(assign);
        auto* bottom = new QHBoxLayout();
        auto* reset = new QPushButton("Reset Effects to Defaults…", &dialog);
        auto* close = new QPushButton("Close", &dialog);
        bottom->addWidget(reset);
        bottom->addStretch();
        bottom->addWidget(close);
        column->addLayout(bottom);
        auto selectedPath = [list] {
            auto* selected = list->currentItem();
            return selected ? selected->data(Qt::UserRole).toString() : QString();
        };
        auto refresh = [&, list](const QString& prefer = QString()) {
            list->clear();
            for (const auto& preset : actions.namedPresets()) {
                auto* item = new QListWidgetItem(preset.name, list);
                item->setData(Qt::UserRole, preset.path);
                if (preset.path == prefer) list->setCurrentItem(item);
            }
            if (!list->currentItem() && list->count()) list->setCurrentRow(0);
        };
        auto updateSelection = [list, load, rename, remove, assign] {
            const bool selected = list->currentItem() != nullptr;
            load->setEnabled(selected);
            rename->setEnabled(selected);
            remove->setEnabled(selected);
            assign->setEnabled(selected);
        };
        QObject::connect(list, &QListWidget::itemSelectionChanged, &dialog, updateSelection);
        refresh(); updateSelection();
        QObject::connect(save, &QPushButton::clicked, &dialog, [&] {
            // An existing preset can be selected without retyping its name.
            // The first entry creates a new preset and is always selected by default.
            const auto existing = actions.namedPresets();
            QStringList choices = {"New Preset…"};
            for (const auto& preset : existing) choices << preset.name;
            bool accepted = false;
            const QString choice = QInputDialog::getItem(&dialog, "Save Effect Preset",
                "Save current effect settings as:", choices, 0, false, &accepted);
            if (!accepted) return;
            QString name;
            QString path;
            if (choice == choices.first()) {
                name = QInputDialog::getText(&dialog, "New Effect Preset",
                    "Preset name:", QLineEdit::Normal, "", &accepted).trimmed();
                if (!accepted) return;
                if (!actions.acceptablePresetName(name)) {
                    QMessageBox::warning(&dialog, "Invalid Name",
                        "Enter 1–70 characters without slashes or control characters.");
                    return;
                }
                // A name entered under New Preset may still match an existing
                // preset; never silently overwrite it.
                for (const auto& preset : existing) {
                    if (preset.name.compare(name, Qt::CaseInsensitive) == 0) {
                        name = preset.name;
                        path = preset.path;
                        break;
                    }
                }
            } else {
                for (const auto& preset : existing) {
                    if (preset.name == choice) {
                        name = preset.name;
                        path = preset.path;
                        break;
                    }
                }
                if (path.isEmpty()) return;
            }
            if (!path.isEmpty() &&
                QMessageBox::question(&dialog, "Replace Preset",
                    "Replace the saved preset '" + name + "' with the current settings?",
                    QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                != QMessageBox::Yes) return;
            if (path.isEmpty()) path = QDir(actions.presetLibraryDirectory()).filePath(
                QUuid::createUuid().toString(QUuid::WithoutBraces) + ".ini");
            actions.snapshotToPresetStaging();
            QString error;
            if (!actions.writeNamedPreset(name, path, &error)) {
                QMessageBox::warning(&dialog, "Save Failed", error);
                return;
            }
            refresh(path);
            actions.statusMessage("Effect preset saved: " + name);
        });
        QObject::connect(assign, &QPushButton::clicked, &dialog, [&] {
            const QString preset = selectedPath();
            if (preset.isEmpty()) return;
            QFile inventoryFile(actions.outputMemoryDirectory() + "/inventory.json");
            if (!inventoryFile.open(QIODevice::ReadOnly)) {
                QMessageBox::warning(&dialog, "Audio Outputs", "No remembered Audio Outputs found.");
                return;
            }
            const QJsonDocument inventory = QJsonDocument::fromJson(inventoryFile.readAll());
            const QJsonObject outputs = inventory.object().value("outputs").toObject();
            if (inventory.object().value("format").toInt() != 1 || outputs.isEmpty()) {
                QMessageBox::warning(&dialog, "Audio Outputs", "No remembered Audio Outputs found.");
                return;
            }
            QString current;
            QFile statusFile(QDir(qEnvironmentVariable("XDG_RUNTIME_DIR"))
                .filePath("viper4linux-output-status.json"));
            if (statusFile.open(QIODevice::ReadOnly)) {
                const QJsonObject status = QJsonDocument::fromJson(statusFile.readAll()).object();
                if (std::abs(status.value("timestamp").toDouble() -
                             double(QDateTime::currentSecsSinceEpoch())) < 5.0)
                    current = status.value("active").toObject().value("key").toString();
            }
            QDialog picker(&dialog);
            picker.setWindowTitle("Assign Preset to Audio Output");
            picker.setMinimumWidth(440);
            auto* layout = new QVBoxLayout(&picker);
            auto* explanation = new QLabel(
                "Select the physical Audio Output whose saved effect settings will be replaced.", &picker);
            explanation->setWordWrap(true);
            layout->addWidget(explanation);
            auto* targets = new QComboBox(&picker);
            for (auto it = outputs.begin(); it != outputs.end(); ++it) {
                const QString name = it.value().toObject().value("description").toString();
                targets->addItem(name + (it.key() == current ? " (active)" : " (remembered)"),
                                 it.key());
            }
            layout->addWidget(targets);
            auto* controls = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &picker);
            controls->button(QDialogButtonBox::Ok)->setText("Apply Preset");
            layout->addWidget(controls);
            QObject::connect(controls, &QDialogButtonBox::accepted, &picker, &QDialog::accept);
            QObject::connect(controls, &QDialogButtonBox::rejected, &picker, &QDialog::reject);
            if (picker.exec() != QDialog::Accepted) return;
            const QString target = targets->currentData().toString();
            if (target.size() != 64) return;
            const QString selectedName = targets->currentText();
            if (QMessageBox::question(&dialog, "Replace Audio Output Settings",
                "Assign '" + list->currentItem()->text() + "' to " + selectedName + "?\n\n"
                "This replaces that output's saved effect settings. "
                "The original named preset and other Audio Outputs are unchanged.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                != QMessageBox::Yes) return;
            QString program = "/usr/bin/viper4linux-output";
            QStringList args = {"--assign-preset", preset, "--target", target};
            if (QCoreApplication::applicationDirPath() != "/usr/bin") {
                program = "python3";
                args.prepend(QDir(QCoreApplication::applicationDirPath())
                    .filePath("../src/viper_output_info.py"));
            }
            QProcess process;
            process.start(program, args);
            if (!process.waitForStarted(3000) || !process.waitForFinished(13000) ||
                process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
                QMessageBox::warning(&dialog, "Preset Assignment Failed",
                    QString::fromUtf8(process.readAllStandardError()).trimmed() +
                    "\n" + QString::fromUtf8(process.readAllStandardOutput()).trimmed());
                return;
            }
            actions.statusMessage("Preset assigned to " + selectedName);
            actions.updateAudioOutputDisplay();
        });
        QObject::connect(load, &QPushButton::clicked, &dialog, [&] {
            const QString path = selectedPath();
            if (path.isEmpty()) return;
            if (QMessageBox::question(&dialog, "Load Effect Preset",
                "Load '" + list->currentItem()->text() + "'?\n\n"
                "This replaces the current effect settings. Unsaved changes will be lost. "
                "Your other saved presets will remain untouched.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                != QMessageBox::Yes) return;
            QString error;
            if (!actions.stageNamedPreset(path, &error)) {
                QMessageBox::warning(&dialog, "Load Failed", error);
                return;
            }
            actions.restoreFromPresetStaging();
            actions.statusMessage("Effect preset loaded: " + list->currentItem()->text());
        });
        QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, [load](QListWidgetItem*) {
            load->click();
        });
        QObject::connect(rename, &QPushButton::clicked, &dialog, [&] {
            const QString path = selectedPath();
            if (path.isEmpty()) return;
            bool accepted = false;
            const QString oldName = list->currentItem()->text();
            const QString name = QInputDialog::getText(&dialog, "Rename Preset",
                "New name:", QLineEdit::Normal, oldName, &accepted).trimmed();
            if (!accepted || name == oldName) return;
            if (!actions.acceptablePresetName(name)) {
                QMessageBox::warning(&dialog, "Invalid Name", "Use 1–70 characters without slashes.");
                return;
            }
            for (const auto& preset : actions.namedPresets()) {
                if (preset.path != path && preset.name.compare(name, Qt::CaseInsensitive) == 0) {
                    QMessageBox::warning(&dialog, "Duplicate Name", "A preset already has that name.");
                    return;
                }
            }
            QSettings file(path, QSettings::IniFormat);
            file.setValue("format/name", name);
            file.sync();
            if (file.status() != QSettings::NoError)
                QMessageBox::warning(&dialog, "Rename Failed", "Could not update the preset file.");
            else refresh(path);
        });
        QObject::connect(remove, &QPushButton::clicked, &dialog, [&] {
            const QString path = selectedPath();
            if (path.isEmpty()) return;
            if (QMessageBox::question(&dialog, "Delete Preset",
                "Permanently delete the saved preset '" + list->currentItem()->text() + "'? "
                "Your currently active effect settings will not change.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                != QMessageBox::Yes) return;
            if (!QFile::remove(path))
                QMessageBox::warning(&dialog, "Delete Failed", "Could not delete preset file.");
            else refresh();
        });
        QObject::connect(reset, &QPushButton::clicked, &dialog, [&] {
            actions.resetEffectsWithConfirmation();
        });
        QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
        dialog.exec();
    }
} // namespace viper_presets_ui
