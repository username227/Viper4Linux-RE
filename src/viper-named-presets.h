// ViPER4Linux-RE: native named-preset library operations.
// Extraction preserves existing format, migration and staging semantics.
#pragma once

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <algorithm>

namespace viper_presets {
    inline QString presetLibraryDirectory() {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + "/viper4linux/presets";
    }
    struct NamedPreset {
        QString path;
        QString name;
    };
    inline QList<NamedPreset> namedPresets() {
        QList<NamedPreset> found;
        QDir dir(presetLibraryDirectory());
        for (const QFileInfo& file : dir.entryInfoList({"*.ini"}, QDir::Files | QDir::NoSymLinks)) {
            QSettings preset(file.absoluteFilePath(), QSettings::IniFormat);
            if (preset.value("format/version").toInt() != 1) continue;
            const QString name = preset.value("format/name").toString().trimmed();
            if (!name.isEmpty()) found.append({file.absoluteFilePath(), name});
        }
        std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });
        return found;
    }
    inline bool acceptablePresetName(const QString& name) {
        if (name.isEmpty() || name.size() > 70) return false;
        for (QChar c : name)
            if (c == '/' || c == '\\' || c.isNull() || !c.isPrint()) return false;
        return true;
    }
    // Writes a complete snapshot with atomic QSettings/INI replacement.
    // Existing file identifiers are UUIDs, never derived from user input.
    inline bool writeNamedPreset(QSettings& settings, const QString& name, const QString& path, QString* error) {
        if (!QDir().mkpath(presetLibraryDirectory())) {
            *error = "Could not create the preset library directory.";
            return false;
        }
        QSettings preset(path, QSettings::IniFormat);
        preset.clear();
        preset.setValue("format/version", 1);
        preset.setValue("format/name", name);
        const auto keys = settings.allKeys();
        for (const QString& key : keys) {
            if (key.startsWith("preset/") && key != "preset/valid")
                preset.setValue("effects/" + key.mid(7), settings.value(key));
        }
        preset.sync();
        if (preset.status() != QSettings::NoError) {
            *error = "Could not write preset file: " + path;
            return false;
        }
        return true;
    }
    // Older versions stored exactly one snapshot in QSettings. Preserve it as
    // a standalone named preset before using that area as the staging buffer.
    inline bool migrateLegacyPreset(QSettings& settings, QString* error) {
        if (settings.value("presets/legacy_migrated", false).toBool()) return true;
        // Mark even a first-run, empty one-slot store as migrated. Otherwise
        // the next named Save would populate the staging keys and a future
        // dialog open would incorrectly import that save as a legacy preset.
        if (settings.value("preset/valid", false).toBool()) {
            const QString name = "Previously Saved Preset";
            const QString path = QDir(presetLibraryDirectory()).filePath(
                QUuid::createUuid().toString(QUuid::WithoutBraces) + ".ini");
            if (!writeNamedPreset(settings, name, path, error)) return false;
        }
        settings.setValue("presets/legacy_migrated", true);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            *error = "Could not record completion of the preset migration.";
            return false;
        }
        return true;
    }
    inline bool stageNamedPreset(QSettings& settings, const QString& path, QString* error) {
        QSettings preset(path, QSettings::IniFormat);
        if (preset.status() != QSettings::NoError ||
            preset.value("format/version").toInt() != 1 ||
            !preset.contains("effects/master_enabled") ||
            !preset.contains("effects/enabled") ||
            !preset.contains("effects/limiter_output_percent")) {
            *error = "This file is not a valid ViPER for Linux effect preset.";
            return false;
        }
        const QStringList keys = preset.allKeys();
        // Do not change the active configuration until the file is validated.
        settings.remove("preset");
        for (const QString& key : keys)
            if (key.startsWith("effects/"))
                settings.setValue("preset/" + key.mid(8), preset.value(key));
        settings.setValue("preset/valid", true);
        settings.sync();
        return true;
    }
} // namespace viper_presets
