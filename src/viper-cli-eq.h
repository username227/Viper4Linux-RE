#pragma once
// EQ preset selection shared with the GUI's built-in curves and QSettings store.
// Only invoked by the short-lived CLI, never a PipeWire RT callback.
#include "viper-control.h"
#include "viper-eq-presets.h"
#include <QSettings>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace viperbass {
namespace cli_eq {
inline QStringList presets(QSettings& settings) {
    QStringList names;
    for (const auto& preset : viper_eq::builtIns())
        names << "builtin:" + QString::fromLatin1(preset.name);
    settings.beginGroup("eq/user_presets");
    const auto users = settings.childGroups();
    settings.endGroup();
    for (const auto& name : users) names << "user:" + name;
    names << "custom";
    return names;
}

inline bool choose(QSettings& s, Controls& c, const QString& requested,
                   std::string& error, bool validateOnly = false, int bandsOverride = -1) {
    s.sync();
    const QStringList all = presets(s);
    QString selected;
    for (const QString& candidate : all) {
        if (candidate.compare(requested, Qt::CaseInsensitive) == 0) {
            selected = candidate;
            break;
        }
    }
    if (selected.isEmpty()) {
        // Bare names are accepted if unambiguous; explicit prefixes resolve collisions.
        for (const QString& candidate : all) {
            const auto display = candidate.mid(candidate.indexOf(':') + 1);
            if (display.compare(requested, Qt::CaseInsensitive) != 0) continue;
            if (!selected.isEmpty()) {
                error = "Ambiguous EQ preset name; use builtin: or user: prefix.";
                return false;
            }
            selected = candidate;
        }
    }
    if (selected.isEmpty()) {
        error = "Unknown EQ preset. Use 'eq-presets' to list available presets.";
        return false;
    }

    const int bands = viper_eq::validBands(
        bandsOverride < 0 ? c.eq_band_count.load() : bandsOverride);
    std::vector<int> values;
    if (selected.startsWith("builtin:")) {
        const auto name = selected.mid(8);
        for (const auto& item : viper_eq::builtIns()) {
            if (name == QString::fromLatin1(item.name)) {
                values = item.levels(bands);
                break;
            }
        }
    } else if (selected.startsWith("user:")) {
        const auto path = "eq/user_presets/" + selected.mid(5) + "/";
        const int storedBands = s.value(path + "bands").toInt();
        if (viper_eq::validBands(storedBands) != storedBands) {
            error = "Saved EQ preset has an invalid band count.";
            return false;
        }
        const auto tokens = s.value(path + "levels").toString().split(',');
        if (tokens.size() != storedBands) {
            error = "Saved EQ preset has an invalid number of band values.";
            return false;
        }
        std::vector<float> frequencies;
        const auto& table = viper_eq::frequencies(storedBands);
        for (int i = 0; i < storedBands; ++i) {
            bool valid = false;
            const int gain = tokens[i].toInt(&valid);
            if (!valid || gain < -1200 || gain > 1200) {
                error = "Saved EQ preset contains invalid band gain values.";
                return false;
            }
            frequencies.push_back(table[i]);
            values.push_back(gain);
        }
        values = viper_eq::interpolate(frequencies, values, bands);
    } else { // Custom: restore the current saved curve for this layout.
        const auto tokens = s.value("eq_levels_" + QString::number(bands)).toString().split(',');
        if (tokens.size() == bands) {
            for (const auto& token : tokens) {
                bool ok = false;
                int v = token.toInt(&ok);
                if (!ok) { values.clear(); break; }
                values.push_back(std::clamp(v, -1200, 1200));
            }
        }
        if (values.size() != static_cast<size_t>(bands)) {
            for (int i = 0; i < bands; ++i) values.push_back(c.eq_levels_hundredths[i].load());
        }
    }
    if (values.size() != static_cast<size_t>(bands)) {
        error = "Preset did not provide a valid EQ curve.";
        return false;
    }

    if (validateOnly) return true; // No shared-memory or settings changes.

    // Commit the settings first so an open GUI sees the correct preset label
    // when it notices the new shared-memory band values.
    QStringList serialized;
    for (int v : values) serialized << QString::number(v);
    s.setValue("eq/selected_preset", selected);
    s.setValue("eq_band_count", bands);
    s.setValue("eq_levels_" + QString::number(bands), serialized.join(','));
    s.sync();
    if (s.status() != QSettings::NoError) {
        error = "Could not save the selected EQ preset.";
        return false;
    }
    for (int i = 0; i < bands; ++i)
        c.eq_levels_hundredths[i].store(values[i]);
    c.eq_request_serial.fetch_add(1);
    return true;
}

inline void list(QSettings& s, std::ostream& out) {
    s.sync();
    out << "Built-in EQ presets:\n";
    for (const auto& item : viper_eq::builtIns())
        out << "  " << item.name << "\n";
    out << "User EQ presets:\n";
    s.beginGroup("eq/user_presets");
    const auto names = s.childGroups();
    s.endGroup();
    if (names.empty()) out << "  (none)\n";
    for (const auto& name : names) out << "  " << name.toStdString() << "\n";
    out << "Custom\n";
}
} // namespace cli_eq
} // namespace viperbass
