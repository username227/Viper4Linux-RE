// Characterize named-preset file format, overwrite, staging and legacy
// migration without a GUI, a live DSP or any real user configuration files.
#include "viper-named-presets.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    QTemporaryDir sandbox;
    assert(sandbox.isValid());
    qputenv("XDG_DATA_HOME", sandbox.path().toUtf8());
    QCoreApplication app(argc, argv);

    const QString library = viper_presets::presetLibraryDirectory();
    assert(library.startsWith(sandbox.path() + "/"));
    assert(viper_presets::acceptablePresetName("Studio Warm"));
    assert(!viper_presets::acceptablePresetName(""));
    assert(!viper_presets::acceptablePresetName("bad/name"));
    assert(!viper_presets::acceptablePresetName("bad\\name"));
    assert(!viper_presets::acceptablePresetName(QString(71, QLatin1Char('x'))));

    QSettings settings(sandbox.path() + "/active.ini", QSettings::IniFormat);
    settings.setValue("preset/valid", true);
    settings.setValue("preset/master_enabled", false);
    settings.setValue("preset/enabled", true);
    settings.setValue("preset/limiter_output_percent", 87);
    settings.setValue("preset/bass_mode", 1);
    settings.setValue("preset/strength_tenths", 23);
    settings.setValue("ui/theme", "classic-v4a");
    settings.sync();
    assert(settings.status() == QSettings::NoError);

    const QString path = QDir(library).filePath("sample.ini");
    QString error;
    assert(viper_presets::writeNamedPreset(settings, "Studio Warm", path, &error));
    QSettings saved(path, QSettings::IniFormat);
    assert(saved.value("format/version").toInt() == 1);
    assert(saved.value("format/name").toString() == "Studio Warm");
    assert(!saved.value("effects/master_enabled", true).toBool());
    assert(saved.value("effects/enabled").toBool());
    assert(saved.value("effects/limiter_output_percent").toInt() == 87);
    assert(saved.value("effects/strength_tenths").toInt() == 23);
    assert(saved.value("effects/bass_mode").toInt() == 1);
    assert(!saved.contains("preset/valid"));
    assert(!saved.contains("effects/valid"));
    assert(!saved.contains("ui/theme"));
    assert(viper_presets::namedPresets().size() == 1);

    // Overwrite must replace old file content, including keys later removed.
    settings.setValue("preset/strength_tenths", 47);
    settings.remove("preset/bass_mode");
    assert(viper_presets::writeNamedPreset(settings, "Studio Warm", path, &error));
    saved.sync();
    assert(saved.value("effects/strength_tenths").toInt() == 47);
    assert(!saved.contains("effects/bass_mode"));
    assert(viper_presets::namedPresets().size() == 1);

    // An invalid or incomplete file cannot alter the staging area.
    const QString bad = sandbox.path() + "/invalid.ini";
    QSettings broken(bad, QSettings::IniFormat);
    broken.setValue("format/version", 2);
    broken.setValue("effects/master_enabled", false);
    broken.setValue("effects/enabled", true);
    broken.setValue("effects/limiter_output_percent", 60);
    broken.sync();
    const QStringList beforeInvalid = settings.allKeys();
    const int previousStrength = settings.value("preset/strength_tenths").toInt();
    assert(!viper_presets::stageNamedPreset(settings, bad, &error));
    assert(settings.allKeys() == beforeInvalid);
    assert(settings.value("preset/strength_tenths").toInt() == previousStrength);
    broken.setValue("format/version", 1);
    broken.remove("effects/limiter_output_percent");
    broken.sync();
    assert(!viper_presets::stageNamedPreset(settings, bad, &error));
    assert(settings.allKeys() == beforeInvalid);

    // Valid staging writes only the preset namespace, not active effects/UI.
    settings.setValue("master_enabled", true);
    settings.setValue("preset/strength_tenths", 99);
    assert(viper_presets::stageNamedPreset(settings, path, &error));
    settings.sync();
    assert(settings.value("master_enabled").toBool());
    assert(settings.value("ui/theme").toString() == "classic-v4a");
    assert(settings.value("preset/valid").toBool());
    assert(settings.value("preset/strength_tenths").toInt() == 47);
    assert(!settings.contains("preset/bass_mode"));
    assert(QFileInfo::exists(path));

    // One-slot legacy presets migrate exactly once into a UUID-named file.
    QSettings old(sandbox.path() + "/old-settings.ini", QSettings::IniFormat);
    old.setValue("preset/valid", true);
    old.setValue("preset/master_enabled", false);
    old.setValue("preset/enabled", true);
    old.setValue("preset/limiter_output_percent", 100);
    old.sync();
    assert(viper_presets::migrateLegacyPreset(old, &error));
    assert(old.value("presets/legacy_migrated").toBool());
    const auto found = viper_presets::namedPresets();
    assert(found.size() == 2);
    bool legacyFound = false;
    for (const auto& item : found) {
        if (item.name == "Previously Saved Preset") {
            legacyFound = true;
            assert(QFileInfo(item.path).completeBaseName().size() == 36);
            QSettings migrated(item.path, QSettings::IniFormat);
            assert(migrated.value("format/version").toInt() == 1);
            assert(!migrated.value("effects/master_enabled", true).toBool());
        }
    }
    assert(legacyFound);
    assert(viper_presets::migrateLegacyPreset(old, &error));
    assert(viper_presets::namedPresets().size() == 2);
    QSettings empty(sandbox.path() + "/never-saved.ini", QSettings::IniFormat);
    assert(viper_presets::migrateLegacyPreset(empty, &error));
    assert(empty.value("presets/legacy_migrated").toBool());
    assert(viper_presets::namedPresets().size() == 2);
    assert(QFileInfo::exists(path));

    std::cout << "PASS: named-preset INI format, overwrite, invalid staging, "
                 "legacy migration, isolation and idempotence\n";
}
