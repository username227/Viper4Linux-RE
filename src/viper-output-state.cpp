// All physical Audio Output snapshots use a native, complete DSP IPC round trip.
// This executable is invoked by the non-realtime Python monitor, never from audio.
#include "viper-control.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>
#include <QVariant>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <string>

using viperbass::Controls;
using Atom = std::atomic<std::int32_t>;

template <typename Owner> struct Field {
    const char* key;
    Atom Owner::* member;
    int low;
    int high;
};
static const Field<Controls> fields[] = {
    { "enabled", &Controls::enabled, 0, 1 },
    { "master_enabled", &Controls::master_enabled, 0, 1 },
    { "bass_mode", &Controls::bass_mode, 0, 2 },
    { "bass_mono_enabled", &Controls::bass_mono_enabled, 0, 1 },
    { "bass_mono_mode", &Controls::bass_mono_mode, 0, 2 },
    { "bass_mono_strength_centi", &Controls::bass_mono_strength_centi, 50, 1000 },
    { "bass_mono_frequency_hz", &Controls::bass_mono_frequency_hz, 15, 150 },
    { "psycho_enabled", &Controls::psycho_enabled, 0, 1 },
    { "psycho_cutoff_hz", &Controls::psycho_cutoff_hz, 60, 150 },
    { "psycho_intensity_percent", &Controls::psycho_intensity_percent, 0, 100 },
    { "psycho_order", &Controls::psycho_order, 2, 5 },
    { "psycho_original_percent", &Controls::psycho_original_percent, 0, 100 },
    { "dynamic_enabled", &Controls::dynamic_enabled, 0, 1 },
    { "dynamic_x_low", &Controls::dynamic_x_low, 0, 2400 },
    { "dynamic_x_high", &Controls::dynamic_x_high, 0, 12000 },
    { "dynamic_y_low", &Controls::dynamic_y_low, 0, 200 },
    { "dynamic_y_high", &Controls::dynamic_y_high, 0, 300 },
    { "dynamic_side_low_percent", &Controls::dynamic_side_low_percent, 0, 100 },
    { "dynamic_side_high_percent", &Controls::dynamic_side_high_percent, 0, 100 },
    { "dynamic_strength_percent", &Controls::dynamic_strength_percent, 0, 100 },
    { "strength_centi", &Controls::strength_centi, 50, 1000 },
    { "frequency_hz", &Controls::frequency_hz, 15, 150 },
    { "clarity_enabled", &Controls::clarity_enabled, 0, 1 },
    { "clarity_mode", &Controls::clarity_mode, 0, 2 },
    { "clarity_gain_milli", &Controls::clarity_gain_milli, 0, 4500 },
    { "spectrum_enabled", &Controls::spectrum_enabled, 0, 1 },
    { "spectrum_frequency_hz", &Controls::spectrum_frequency_hz, 2200, 8200 },
    { "spectrum_exciter_milli", &Controls::spectrum_exciter_milli, 0, 6000 },
    { "tube_enabled", &Controls::tube_enabled, 0, 1 },
    { "tube_model", &Controls::tube_model, 0, 2 },
    { "tube_drive_percent", &Controls::tube_drive_percent, 0, 100 },
    { "tube_mix_percent", &Controls::tube_mix_percent, 0, 100 },
    { "analogx_enabled", &Controls::analogx_enabled, 0, 1 },
    { "analogx_mode", &Controls::analogx_mode, 0, 2 },
    { "crossfeed_enabled", &Controls::crossfeed_enabled, 0, 1 },
    { "crossfeed_preset", &Controls::crossfeed_preset, 0, 2 },
    { "fet_enabled", &Controls::fet_enabled, 0, 1 },
    { "fet_threshold_db", &Controls::fet_threshold_db, -48, 0 },
    { "fet_ratio_tenths", &Controls::fet_ratio_tenths, 10, 100 },
    { "fet_knee_db", &Controls::fet_knee_db, 0, 12 },
    { "fet_makeup_db", &Controls::fet_makeup_db, 0, 24 },
    { "fet_attack_ms", &Controls::fet_attack_ms, 1, 100 },
    { "fet_release_ms", &Controls::fet_release_ms, 5, 500 },
    { "mb_enabled", &Controls::mb_enabled, 0, 1 },
    { "mb_band_count", &Controls::mb_band_count, 3, 5 },
    { "ddc_enabled", &Controls::ddc_enabled, 0, 1 },
    { "speaker_enabled", &Controls::speaker_enabled, 0, 1 },
    { "imager_enabled", &Controls::imager_enabled, 0, 1 },
    { "imager_low_width_percent", &Controls::imager_low_width_percent, 0, 200 },
    { "imager_mid_width_percent", &Controls::imager_mid_width_percent, 0, 200 },
    { "imager_high_width_percent", &Controls::imager_high_width_percent, 0, 200 },
    { "imager_low_crossover_hz", &Controls::imager_low_crossover_hz, 80, 400 },
    { "imager_high_crossover_hz", &Controls::imager_high_crossover_hz, 2000, 8000 },
    { "headphone_enabled", &Controls::headphone_enabled, 0, 1 },
    { "headphone_quality", &Controls::headphone_quality, 0, 4 },
    { "diff_enabled", &Controls::diff_enabled, 0, 1 },
    { "diff_delay_ms", &Controls::diff_delay_ms, 1, 20 },
    { "diff_reverse", &Controls::diff_reverse, 0, 1 },
    { "diff_mix_percent", &Controls::diff_mix_percent, 0, 100 },
    { "diff_lp_hz", &Controls::diff_lp_hz, 0, 20000 },
    { "field_enabled", &Controls::field_enabled, 0, 1 },
    { "field_widen_tenths", &Controls::field_widen_tenths, 0, 80 },
    { "field_mid_hundredths", &Controls::field_mid_hundredths, 100, 200 },
    { "field_depth", &Controls::field_depth, 0, 950 },
    { "reverb_enabled", &Controls::reverb_enabled, 0, 1 },
    { "reverb_room_percent", &Controls::reverb_room_percent, 0, 100 },
    { "reverb_width_percent", &Controls::reverb_width_percent, 0, 100 },
    { "reverb_damp_percent", &Controls::reverb_damp_percent, 0, 100 },
    { "reverb_wet_percent", &Controls::reverb_wet_percent, 0, 100 },
    { "reverb_dry_percent", &Controls::reverb_dry_percent, 0, 100 },
    { "limiter_threshold_permille", &Controls::limiter_threshold_permille, 300, 1000 },
    { "limiter_output_percent", &Controls::limiter_output_percent, 1, 200 },
    { "limiter_pan_percent", &Controls::limiter_pan_percent, -100, 100 },
    { "agc_enabled", &Controls::agc_enabled, 0, 1 },
    { "agc_strength_centi", &Controls::agc_strength_centi, 50, 300 },
    { "agc_max_gain_centi", &Controls::agc_max_gain_centi, 100, 1000 },
    { "agc_threshold_permille", &Controls::agc_threshold_permille, 300, 1000 },
    { "lufs_enabled", &Controls::lufs_enabled, 0, 1 },
    { "lufs_target_tenths", &Controls::lufs_target_tenths, -240, -80 },
    { "lufs_max_gain_tenths", &Controls::lufs_max_gain_tenths, 0, 120 },
    { "lufs_speed", &Controls::lufs_speed, 0, 2 },
    { "eq_enabled", &Controls::eq_enabled, 0, 1 },
    { "eq_band_count", &Controls::eq_band_count, 10, 31 },
    { "deq_enabled", &Controls::deq_enabled, 0, 1 },
    { "deq_band_count", &Controls::deq_band_count, 1, 10 },
    { "convolver_enabled", &Controls::convolver_enabled, 0, 1 },
    { "convolver_cross_percent", &Controls::convolver_cross_percent, 0, 100 },
};
static const Field<Controls::MultibandControls> mbFields[] = {
    { "enabled", &Controls::MultibandControls::enabled, 0, 1 },
    { "threshold_db", &Controls::MultibandControls::threshold_db, -48, 0 },
    { "ratio_tenths", &Controls::MultibandControls::ratio_tenths, 10, 100 },
    { "knee_db", &Controls::MultibandControls::knee_db, 0, 12 },
    { "makeup_db", &Controls::MultibandControls::makeup_db, 0, 24 },
    { "attack_ms", &Controls::MultibandControls::attack_ms, 1, 100 },
    { "release_ms", &Controls::MultibandControls::release_ms, 5, 500 },
};
static const Field<Controls::DynamicEqBandControl> deqFields[] = {
    { "frequency_hz", &Controls::DynamicEqBandControl::frequency_hz, 20, 20000 },
    { "q_hundredths", &Controls::DynamicEqBandControl::q_hundredths, 50, 800 },
    { "gain_hundredths", &Controls::DynamicEqBandControl::gain_hundredths, -1200, 1200 },
    { "threshold_tenths", &Controls::DynamicEqBandControl::threshold_tenths, -800, 0 },
    { "attack_ms", &Controls::DynamicEqBandControl::attack_ms, 1, 100 },
    { "release_ms", &Controls::DynamicEqBandControl::release_ms, 10, 500 },
    { "filter_type", &Controls::DynamicEqBandControl::filter_type, 0, 2 },
};

static bool integer(const QJsonValue& json, int low, int high, int& out) {
    if (!json.isDouble()) return false;
    const double v = json.toDouble();
    if (v < low || v > high) return false;
    const int parsed = static_cast<int>(v);
    if (v != static_cast<double>(parsed)) return false;
    out = parsed;
    return true;
}
template <typename T, std::size_t N>
static QJsonObject captureFields(const T& source, const Field<T> (&members)[N]) {
    QJsonObject values;
    for (const auto& f : members)
        values.insert(f.key, (source.*(f.member)).load(std::memory_order_relaxed));
    return values;
}
template <typename T, std::size_t N>
static bool validateFields(const QJsonObject& obj, const Field<T> (&members)[N]) {
    if (obj.size() != static_cast<int>(N)) return false;
    for (const auto& f : members) {
        int v = 0;
        if (!integer(obj.value(QLatin1String(f.key)), f.low, f.high, v)) return false;
    }
    return true;
}
template <typename T, std::size_t N>
static bool restoreFields(T& target, const QJsonObject& json,
                          const Field<T> (&members)[N]) {
    bool changed = false;
    for (const auto& f : members) {
        const int v = json.value(QLatin1String(f.key)).toInt();
        if ((target.*(f.member)).exchange(v, std::memory_order_relaxed) != v)
            changed = true;
    }
    return changed;
}
static bool validBands(const int n) { return n == 10 || n == 15 || n == 25 || n == 31; }

static QString joinEq(const QJsonArray& values, int count) {
    QStringList parts;
    for (int i = 0; i < count; ++i) parts << QString::number(values[i].toInt());
    return parts.join(',');
}
static QString settingName(QString key) {
    if (key == "strength_centi") return "strength_tenths";
    if (key == "frequency_hz") return "frequency";
    if (key == "clarity_gain_milli") return "clarity_gain_tenths";
    if (key == "spectrum_frequency_hz") return "spectrum_frequency_hundreds";
    if (key == "spectrum_exciter_milli") return "spectrum_exciter_tenths";
    if (key == "diff_lp_hz") return "diff_lp_hundreds";
    if (key == "field_depth") return "field_depth_step";
    return key;
}
static int settingValue(const QString& key, int value) {
    if (key == "strength_centi") return value / 10;
    if (key == "clarity_gain_milli" || key == "spectrum_exciter_milli") return value / 100;
    if (key == "spectrum_frequency_hz" || key == "diff_lp_hz") return value / 100;
    if (key == "field_depth") return value == 0 ? 0 : std::clamp((value - 150 + 25) / 50, 1, 16);
    return value;
}
static bool flagName(const QString& key) {
    return key == "enabled" || key == "master_enabled" || key == "diff_reverse" ||
           key.endsWith("_enabled");
}
static void persistState(QSettings& settings, const QJsonObject& state) {
    const auto values = state.value("values").toObject();
    for (const auto& field : fields) {
        const QString key = QLatin1String(field.key);
        const int value = values.value(key).toInt();
        settings.setValue(settingName(key),
                          flagName(key) ? QVariant(value != 0) : QVariant(settingValue(key, value)));
        // Store full native units as well as legacy GUI slider units.
        // Without these, output restoration could leave a stale precision
        // override from the previously selected physical output.
        if (key == "strength_centi" || key == "bass_mono_strength_centi" ||
            key == "clarity_gain_milli" || key == "spectrum_frequency_hz" ||
            key == "spectrum_exciter_milli" || key == "diff_lp_hz" ||
            key == "field_depth")
            settings.setValue("precision/" + key, value);
    }
    const auto eq = state.value("eq_levels").toArray();
    const int bands = values.value("eq_band_count").toInt();
    settings.setValue("eq_levels_" + QString::number(bands), joinEq(eq, bands));
    const auto ui = state.value("ui").toObject();
    for (const int n : {10, 15, 25, 31}) {
        const QString key = "eq_levels_" + QString::number(n);
        if (n != bands && ui.contains(key)) settings.setValue(key, ui.value(key).toString());
    }
    // Preserve the EQ preset identity alongside its actual DSP curve.
    // Legacy output snapshots have no name; they remain Custom as before.
    settings.setValue("eq/selected_preset",
        ui.value("eq_selected_preset").toString("custom"));
    const auto mb = state.value("mb_bands").toArray();
    const auto crossovers = state.value("mb_crossovers").toArray();
    for (int i = 0; i < 4; ++i)
        settings.setValue(QString("mb_x%1_hz").arg(i), crossovers[i].toInt());
    for (int i = 0; i < 5; ++i) {
        const auto band = mb[i].toObject();
        for (const auto& f : mbFields)
            settings.setValue(QString("mb_b%1_%2").arg(i).arg(f.key), band.value(f.key).toInt());
    }
    const auto deq = state.value("deq_bands").toArray();
    for (int i = 0; i < 10; ++i) {
        const auto band = deq[i].toObject();
        const QString p = QString("deq_b%1_").arg(i);
        settings.setValue(p + "freq", band.value("frequency_hz").toInt());
        settings.setValue(p + "q", band.value("q_hundredths").toInt());
        settings.setValue(p + "gain", band.value("gain_hundredths").toInt());
        settings.setValue(p + "threshold", band.value("threshold_tenths").toInt());
        settings.setValue(p + "attack", band.value("attack_ms").toInt());
        settings.setValue(p + "release", band.value("release_ms").toInt());
        settings.setValue(p + "type", band.value("filter_type").toInt());
        settings.setValue(p + "user_modified", ui.value(p + "user_modified").toBool(true));
    }
    const auto files = state.value("files").toObject();
    settings.setValue("ddc_profile", files.value("ddc_profile").toString());
    settings.setValue("convolver_kernel", files.value("convolver_kernel").toString());
}
static QJsonObject captureState(const Controls& controls, QSettings& settings) {
    QJsonObject result;
    result.insert("format", 1);
    result.insert("control_version", static_cast<int>(viperbass::kVersion));
    result.insert("backend_pid", controls.owner_pid.load(std::memory_order_relaxed));
    const auto values = captureFields(controls, fields);
    result.insert("values", values);
    const int count = values.value("eq_band_count").toInt();
    QJsonArray eq;
    for (int i = 0; i < 31; ++i)
        eq.append(controls.eq_levels_hundredths[i].load(std::memory_order_relaxed));
    result.insert("eq_levels", eq);
    QJsonArray crossovers;
    for (int i = 0; i < 4; ++i)
        crossovers.append(controls.mb_crossovers_hz[i].load(std::memory_order_relaxed));
    result.insert("mb_crossovers", crossovers);
    QJsonArray mb;
    for (const auto& band : controls.mb_bands) mb.append(captureFields(band, mbFields));
    result.insert("mb_bands", mb);
    QJsonArray deq;
    for (const auto& band : controls.deq_bands) deq.append(captureFields(band, deqFields));
    result.insert("deq_bands", deq);
    QJsonObject paths;
    paths.insert("ddc_profile", settings.value("ddc_profile").toString());
    paths.insert("convolver_kernel", settings.value("convolver_kernel").toString());
    result.insert("files", paths);
    QJsonObject ui;
    ui.insert("eq_selected_preset", settings.value("eq/selected_preset", "custom").toString());
    for (const int n : {10, 15, 25, 31}) {
        const QString key = "eq_levels_" + QString::number(n);
        if (n != count) ui.insert(key, settings.value(key).toString());
    }
    for (int i = 0; i < 10; ++i) {
        const QString key = QString("deq_b%1_user_modified").arg(i);
        ui.insert(key, settings.value(key, true).toBool());
    }
    result.insert("ui", ui);
    return result;
}
// The 26 -> 27 IPC change added only the transient input-signal counter
// (PR #13), without changing serialized effect fields. This exception is
// intentionally version-specific: re-audit snapshots on the next IPC bump.
static_assert(viperbass::kVersion == 27,
              "Review native Audio Output snapshot compatibility after IPC version changes");
static bool compatibleSnapshotControlVersion(const QJsonValue& value) {
    if (!value.isDouble()) return false;
    const int version = value.toInt(-1);
    return version == 26 || version == static_cast<int>(viperbass::kVersion);
}

static bool validateState(const QJsonObject& state) {
    if (state.value("format").toInt() != 1 ||
        !compatibleSnapshotControlVersion(state.value("control_version"))) return false;
    if (!state.value("values").isObject() ||
        !validateFields(state.value("values").toObject(), fields)) return false;
    const int n = state.value("values").toObject().value("eq_band_count").toInt();
    if (!validBands(n) || !state.value("eq_levels").isArray() ||
        state.value("eq_levels").toArray().size() != 31) return false;
    for (const auto& item : state.value("eq_levels").toArray()) {
        int parsed = 0;
        if (!integer(item, -1200, 1200, parsed)) return false;
    }
    if (!state.value("mb_crossovers").isArray() ||
        state.value("mb_crossovers").toArray().size() != 4) return false;
    for (const auto& item : state.value("mb_crossovers").toArray()) {
        int parsed = 0;
        if (!integer(item, 80, 15000, parsed)) return false;
    }
    if (!state.value("mb_bands").isArray() || state.value("mb_bands").toArray().size() != 5 ||
        !state.value("deq_bands").isArray() || state.value("deq_bands").toArray().size() != 10)
        return false;
    for (const auto& item : state.value("mb_bands").toArray())
        if (!item.isObject() || !validateFields(item.toObject(), mbFields)) return false;
    for (const auto& item : state.value("deq_bands").toArray())
        if (!item.isObject() || !validateFields(item.toObject(), deqFields)) return false;
    const auto files = state.value("files");
    if (!files.isObject() || files.toObject().size() != 2) return false;
    for (const QString k : {"ddc_profile", "convolver_kernel"}) {
        const auto value = files.toObject().value(k);
        if (!value.isString() || value.toString().size() > 4096) return false;
    }
    const auto ui = state.value("ui");
    if (!ui.isObject()) return false;
    const QJsonObject uiValues = ui.toObject();
    // The fourteenth field is optional to support already-saved output
    // profiles. Reject unknown metadata rather than silently accepting it.
    const bool hasEqPresetName = uiValues.contains("eq_selected_preset");
    if (uiValues.size() != (hasEqPresetName ? 14 : 13)) return false;
    if (hasEqPresetName) {
        const QJsonValue name = uiValues.value("eq_selected_preset");
        if (!name.isString()) return false;
        const QString text = name.toString();
        const bool validName = text == "custom" ||
            ((text.startsWith("builtin:") || text.startsWith("user:")) &&
             text.size() > (text.startsWith("builtin:") ? 8 : 5) &&
             text.size() <= 80);
        if (!validName) return false;
    }
    for (const int n : {10, 15, 25, 31}) {
        const QString key = "eq_levels_" + QString::number(n);
        if (n == state.value("values").toObject().value("eq_band_count").toInt()) continue;
        const auto v = ui.toObject().value(key);
        if (!v.isString() || v.toString().size() > 512) return false;
    }
    for (int i = 0; i < 10; ++i) {
        const auto v = ui.toObject().value(QString("deq_b%1_user_modified").arg(i));
        if (!v.isBool()) return false;
    }
    const auto values = state.value("values").toObject();
    // A removed IR/VDC file must not silently leave another output's old
    // loaded filter active. Reject this restore *before* writing settings.
    for (const QString path : {"ddc_profile", "convolver_kernel"}) {
        const QString value = files.toObject().value(path).toString();
        if (!value.isEmpty() && !QFileInfo(value).isFile()) return false;
    }
    const int depth = values.value("field_depth").toInt();
    if (depth != 0 && (depth < 200 || depth > 950 || depth % 50 != 0)) return false;
    if (values.value("mb_band_count").toInt() != 3 && values.value("mb_band_count").toInt() != 5) return false;
    return true;
}
static bool restoreState(Controls& c, const QJsonObject& snapshot, QSettings& settings) {
    if (!validateState(snapshot)) return false;
    const auto previousDdc = settings.value("ddc_profile").toString();
    const auto previousIr = settings.value("convolver_kernel").toString();
    persistState(settings, snapshot);
    settings.sync();
    if (settings.status() != QSettings::NoError) return false;
    const auto values = snapshot.value("values").toObject();
    bool changed = restoreFields(c, values, fields);
    const auto eq = snapshot.value("eq_levels").toArray();
    bool eqChanged = false;
    for (int i = 0; i < 31; ++i)
        eqChanged |= c.eq_levels_hundredths[i].exchange(eq[i].toInt()) != eq[i].toInt();
    const auto cross = snapshot.value("mb_crossovers").toArray();
    bool mbChanged = false;
    for (int i = 0; i < 4; ++i)
        mbChanged |= c.mb_crossovers_hz[i].exchange(cross[i].toInt()) != cross[i].toInt();
    for (int i = 0; i < 5; ++i)
        mbChanged |= restoreFields(c.mb_bands[i], snapshot.value("mb_bands").toArray()[i].toObject(), mbFields);
    bool deqChanged = false;
    for (int i = 0; i < 10; ++i)
        deqChanged |= restoreFields(c.deq_bands[i], snapshot.value("deq_bands").toArray()[i].toObject(), deqFields);
    // Native filter rebuilds are always scheduled off the real-time thread.
    // Use one request per affected group, after complete parameter publication.
    auto bumped = [&](const char* /*unused*/, std::atomic<std::uint32_t>& request) {
        request.fetch_add(1, std::memory_order_release);
    };
    if (changed || eqChanged) bumped("eq", c.eq_request_serial);
    if (changed || mbChanged) bumped("multiband", c.mb_request_serial);
    if (changed || deqChanged) bumped("deq", c.deq_request_serial);
    if (changed) {
        bumped("tube", c.tube_request_serial);
        bumped("analogx", c.analogx_request_serial);
        bumped("crossfeed", c.crossfeed_request_serial);
        bumped("fet", c.fet_request_serial);
        bumped("speaker", c.speaker_request_serial);
        bumped("imager", c.imager_request_serial);
    }
    if (changed || previousDdc != settings.value("ddc_profile").toString())
        bumped("ddc", c.ddc_request_serial);
    if (changed || previousIr != settings.value("convolver_kernel").toString())
        bumped("convolver", c.convolver_request_serial);
    return true;
}

static QJsonObject snapshotFromNamedPreset(const QString& path, Controls& controls,
                                             QSettings& settings, QString* error) {
    QSettings preset(path, QSettings::IniFormat);
    if (preset.status() != QSettings::NoError ||
        preset.value("format/version").toInt() != 1 ||
        !preset.contains("effects/master_enabled") ||
        !preset.contains("effects/limiter_output_percent")) {
        *error = "Invalid named effect preset";
        return {};
    }
    QJsonObject state = captureState(controls, settings);
    QJsonObject values = state.value("values").toObject();
    auto readInt = [&](const QString& key, int& result) -> bool {
        const QString setting = "effects/" + key;
        if (!preset.contains(setting)) { *error = "Missing preset field: " + key; return false; }
        const QVariant v = preset.value(setting);
        if (v.toString().compare("true", Qt::CaseInsensitive) == 0) { result = 1; return true; }
        if (v.toString().compare("false", Qt::CaseInsensitive) == 0) { result = 0; return true; }
        bool ok = false;
        result = v.toString().toInt(&ok);
        if (!ok) *error = "Invalid preset field: " + key;
        return ok;
    };
    for (const auto& field : fields) {
        const QString name = QLatin1String(field.key);
        QString key = settingName(name);
        if (name == "imager_high_crossover_hz") key = "imager_high_crossover_hundreds";
        int value = 0;
        if (!readInt(key, value)) return {};
        if (name == "strength_centi") value *= 10;
        else if (name == "clarity_gain_milli" || name == "spectrum_exciter_milli") value *= 100;
        else if (name == "spectrum_frequency_hz" || name == "diff_lp_hz" ||
                 name == "imager_high_crossover_hz") value *= 100;
        else if (name == "field_depth") value = value == 0 ? 0 : value * 50 + 150;
        values.insert(name, value);
    }
    state.insert("values", values);
    QJsonArray mb;
    for (int i = 0; i < 5; ++i) {
        QJsonObject band;
        for (const auto& field : mbFields) {
            int value = 0;
            if (!readInt(QString("mb_b%1_%2").arg(i).arg(field.key), value)) return {};
            band.insert(field.key, value);
        }
        mb.append(band);
    }
    state.insert("mb_bands", mb);
    QJsonArray xovers;
    for (int i = 0; i < 4; ++i) {
        int value = 0;
        if (!readInt(QString("mb_x%1_hz").arg(i), value)) return {};
        xovers.append(value);
    }
    state.insert("mb_crossovers", xovers);
    QJsonArray deq;
    QJsonObject ui;
    const QString selectedEq = preset.value("effects/eq_selected_preset", "custom").toString();
    const bool validEq = selectedEq == "custom" ||
        ((selectedEq.startsWith("builtin:") || selectedEq.startsWith("user:")) &&
         selectedEq.size() > (selectedEq.startsWith("builtin:") ? 8 : 5) &&
         selectedEq.size() <= 80);
    if (!validEq) { *error = "Invalid EQ preset identity"; return {}; }
    ui.insert("eq_selected_preset", selectedEq);
    const std::pair<const char*,const char*> deqNames[] = {
        {"frequency_hz","freq"}, {"q_hundredths","q"}, {"gain_hundredths","gain"},
        {"threshold_tenths","threshold"}, {"attack_ms","attack"},
        {"release_ms","release"}, {"filter_type","type"}
    };
    for (int i = 0; i < 10; ++i) {
        QJsonObject band;
        for (const auto& pair : deqNames) {
            int value = 0;
            if (!readInt(QString("deq_b%1_%2").arg(i).arg(pair.second), value)) return {};
            band.insert(pair.first, value);
        }
        deq.append(band);
        int modified = 0;
        if (!readInt(QString("deq_b%1_user_modified").arg(i), modified)) return {};
        ui.insert(QString("deq_b%1_user_modified").arg(i), modified != 0);
    }
    state.insert("deq_bands", deq);
    const int count = values.value("eq_band_count").toInt();
    QJsonArray eq;
    for (int i = 0; i < 31; ++i) eq.append(0);
    for (int bands : {10,15,25,31}) {
        const QString key = QString("eq_levels_%1").arg(bands);
        const QStringList list = preset.value("effects/" + key).toString().split(',');
        if (list.size() != bands) { *error = "Invalid preset EQ levels: " + key; return {}; }
        QStringList normalized;
        for (const auto& entry : list) {
            bool ok = false;
            const int level = entry.trimmed().toInt(&ok);
            if (!ok || level < -1200 || level > 1200) {
                *error = "Invalid preset EQ level: " + key; return {};
            }
            normalized.append(QString::number(level));
        }
        if (bands == count) {
            for (int i = 0; i < bands; ++i) eq[i] = normalized[i].toInt();
        } else {
            ui.insert(key, normalized.join(','));
        }
    }
    state.insert("eq_levels", eq);
    state.insert("ui", ui);
    QJsonObject files;
    for (const QString key : {"ddc_profile", "convolver_kernel"})
        files.insert(key, preset.value("effects/" + key, "").toString());
    state.insert("files", files);
    if (!validateState(state)) {
        *error = "Preset values or referenced IRS/VDC files are invalid";
        return {};
    }
    return state;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2 || ((std::string(argv[1]) == "preset" && argc != 3) ||
        (std::string(argv[1]) != "preset" && (argc != 2 ||
         (std::string(argv[1]) != "capture" && std::string(argv[1]) != "restore"))))) {
        std::cerr << "Usage: viper4linux-output-state capture|restore|preset FILE\n";
        return 2;
    }
    const std::string mode = argv[1];
    viperbass::Mapping mapping;
    if (!mapping.attach()) {
        std::cerr << "ViPER DSP shared-memory channel unavailable\n";
        return 1;
    }
    QSettings settings("ViPERforLinux", "ViPERClean");
    settings.sync();
    if (settings.status() != QSettings::NoError) return 1;
    if (mode == "preset") {
        QString error;
        const QJsonObject snapshot = snapshotFromNamedPreset(QString::fromLocal8Bit(argv[2]), *mapping.get(), settings, &error);
        if (snapshot.isEmpty() || !mapping.current()) {
            std::cerr << error.toStdString() << "\n";
            return 2;
        }
        std::cout << QJsonDocument(snapshot).toJson(QJsonDocument::Compact).toStdString() << "\n";
        return 0;
    }
    if (mode == "capture") {
        auto snapshot = captureState(*mapping.get(), settings);
        if (!mapping.current()) return 1;
        std::cout << QJsonDocument(snapshot).toJson(QJsonDocument::Compact).toStdString() << '\n';
        return 0;
    }
    const std::string input((std::istreambuf_iterator<char>(std::cin)),
                            std::istreambuf_iterator<char>());
    if (input.size() > 128 * 1024) return 2;
    QJsonParseError error{};
    const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(input), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject() ||
        !validateState(doc.object())) {
        std::cerr << "Invalid or incompatible Audio Output snapshot (no changes applied)\n";
        return 2;
    }
    if (!mapping.current() || !restoreState(*mapping.get(), doc.object(), settings)) {
        std::cerr << "Audio Output restoration failed\n";
        return 1;
    }
    if (!mapping.current()) return 1;
    return 0;
}
