// Experimental, one-way ViPER4Linux -> ViPER4Android schema 2.1 exporter.
// Intentionally read-only: native QSettings presets and the DSP are untouched.
#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace viper_android_export {

inline bool convert(const QString& path, QJsonObject* output, QString* warning, QString* error) {
    QSettings ini(path, QSettings::IniFormat);
    if (ini.status() != QSettings::NoError ||
        ini.value("format/version").toInt() != 1 ||
        !ini.contains("effects/master_enabled") ||
        !ini.contains("effects/limiter_output_percent")) {
        *error = "Not a valid ViPER4Linux named preset.";
        return false;
    }
    const auto get = [&](const char* key, const QVariant& fallback) {
        return ini.value(QStringLiteral("effects/") + QString::fromLatin1(key), fallback);
    };
    const auto n = [&](const char* key, int fallback) { return get(key, fallback).toInt(); };
    const auto flag = [&](const char* key, bool fallback = false) { return get(key, fallback).toBool(); };
    const auto scaled = [&](const char* key, int fallback, double divisor) {
        return double(n(key, fallback)) / divisor;
    };
    const auto clamp = [](double value, double lower, double upper) {
        return std::clamp(value, lower, upper);
    };

    QJsonObject json;
    json["schemaVersion"] = 2.1;
    json["name"] = ini.value("format/name", "ViPER4Linux Export").toString();
    // Master bypass is not represented in Android preset JSON: Android stores
    // the master switch separately from per-effect settings.
    json["masterLimiter"] = QJsonObject{
        {"threshold", clamp(scaled("limiter_threshold_permille", 500, 1000.0), 0.3, 1.0)},
        {"outputVolume", clamp(scaled("limiter_output_percent", 100, 100.0), 0.01, 2.0)},
        {"channelPan", clamp(scaled("limiter_pan_percent", 0, 100.0), -1.0, 1.0)}};
    json["playbackGainControl"] = QJsonObject{
        {"enable", flag("agc_enabled")},
        {"strength", scaled("agc_strength_centi", 100, 100.0)},
        {"maxGain", scaled("agc_max_gain_centi", 300, 100.0)},
        {"outputThreshold", scaled("agc_threshold_permille", 900, 1000.0)}};
    json["lufs"] = QJsonObject{
        {"enable", flag("lufs_enabled")},
        {"target", scaled("lufs_target_tenths", -140, 10.0)},
        {"maxGain", scaled("lufs_max_gain_tenths", 60, 10.0)},
        {"speed", n("lufs_speed", 1)}};
    json["bass"] = QJsonObject{
        {"enable", flag("enabled")},
        {"mode", n("bass_mode", 0)},
        {"frequency", n("frequency", 65)},
        {"gain", scaled("strength_tenths", 100, 10.0)},
        {"antiPop", true}};
    json["bassMono"] = QJsonObject{
        {"enable", flag("bass_mono_enabled")},
        {"mode", n("bass_mono_mode", 0)},
        {"frequency", n("bass_mono_frequency_hz", 65)},
        {"gain", scaled("bass_mono_strength_centi", 200, 100.0)},
        {"antiPop", true}};
    json["clarity"] = QJsonObject{
        {"enable", flag("clarity_enabled")},
        {"mode", n("clarity_mode", 0)},
        {"gain", scaled("clarity_gain_tenths", 15, 10.0)}};
    json["spectrumExtension"] = QJsonObject{
        {"enable", flag("spectrum_enabled")},
        {"strength", n("spectrum_frequency_hundreds", 76) * 100},
        {"exciter", scaled("spectrum_exciter_tenths", 10, 10.0)}};
    json["headphoneSurround"] = QJsonObject{
        {"enable", flag("headphone_enabled")},
        {"quality", n("headphone_quality", 0) + 1}};
    json["diffSurround"] = QJsonObject{
        {"enable", flag("diff_enabled")},
        {"delay", n("diff_delay_ms", 8)},
        {"reverse", flag("diff_reverse")},
        {"wetDryMix", scaled("diff_mix_percent", 50, 100.0)},
        {"lpCutoff", n("diff_lp_hundreds", 0) * 100}};
    json["stereoImager"] = QJsonObject{
        {"enable", flag("imager_enabled")},
        {"lowWidth", scaled("imager_low_width_percent", 100, 100.0)},
        {"midWidth", scaled("imager_mid_width_percent", 100, 100.0)},
        {"highWidth", scaled("imager_high_width_percent", 100, 100.0)},
        {"lowCrossover", n("imager_low_crossover_hz", 200)},
        {"highCrossover", n("imager_high_crossover_hundreds", 40) * 100}};
    json["reverb"] = QJsonObject{
        {"enable", flag("reverb_enabled")},
        {"roomSize", scaled("reverb_room_percent", 50, 100.0)},
        {"width", scaled("reverb_width_percent", 50, 100.0)},
        {"damp", scaled("reverb_damp_percent", 50, 100.0)},
        {"wet", scaled("reverb_wet_percent", 20, 100.0)},
        {"dry", scaled("reverb_dry_percent", 50, 100.0)}};
    json["analogX"] = QJsonObject{
        {"enable", flag("analogx_enabled")},
        {"mode", n("analogx_mode", 0)}};
    json["tubeSimulator"] = QJsonObject{{"enable", flag("tube_enabled")}};
    json["speakerCorrection"] = QJsonObject{{"enable", flag("speaker_enabled")}};

    // Android accepts exactly these graphic-EQ band layouts. V4L stores
    // hundredths of a dB as comma-separated signed integers.
    const int count = n("eq_band_count", 10);
    if (count != 10 && count != 15 && count != 25 && count != 31) {
        *error = "Unsupported graphic EQ band count.";
        return false;
    }
    const QStringList raw = get(("eq_levels_" + QString::number(count)).toLatin1().constData(),
                                QString()).toString().split(',');
    if (raw.size() != count) {
        *error = "Incomplete graphic EQ levels in saved preset.";
        return false;
    }
    QJsonArray levels;
    for (const QString& item : raw) {
        bool ok = false;
        const int hundredths = item.trimmed().toInt(&ok);
        if (!ok || hundredths < -1200 || hundredths > 1200) {
            *error = "Invalid graphic EQ band gain in saved preset.";
            return false;
        }
        levels.append(double(hundredths) / 100.0);
    }
    json["equalizer"] = QJsonObject{
        {"enable", flag("eq_enabled")}, {"bandCount", count},
        {"bands", levels}, {"presetId", QJsonValue(QJsonValue::Null)}};

    const int deqCount = n("deq_band_count", 3);
    if (deqCount < 1 || deqCount > 10) {
        *error = "Invalid Dynamic EQ band count.";
        return false;
    }
    QJsonArray freqs, qs, gains, thresholds, attacks, releases, types;
    for (int i = 0; i < deqCount; ++i) {
        const QString k = QString("deq_b%1_").arg(i);
        const auto field = [&](const char* suffix, int def) {
            return ini.value("effects/" + k + QString::fromLatin1(suffix), def).toInt();
        };
        freqs.append(field("freq", 60));
        qs.append(double(field("q", 100)) / 100.0);
        gains.append(double(field("gain", 0)) / 100.0);
        thresholds.append(double(field("threshold", -240)) / 10.0);
        attacks.append(field("attack", 10));
        releases.append(field("release", 100));
        types.append(field("type", 0));
    }
    json["dynamicEq"] = QJsonObject{
        {"enable", flag("deq_enabled")}, {"bandCount", deqCount},
        {"freqs", freqs}, {"qs", qs}, {"gains", gains},
        {"thresholds", thresholds}, {"attacks", attacks},
        {"releases", releases}, {"filterTypes", types}};
    // Android deserializes on top of its current live state. Explicitly disable
    // unsupported effects so they cannot remain active from an earlier preset.
    // A later converter revision can add their full, verified parameter maps.
    json["fetCompressor"] = QJsonObject{{"enable", false}};
    json["multibandCompressor"] = QJsonObject{{"enable", false}};
    json["fieldSurround"] = QJsonObject{{"enable", false}};
    json["dynamicSystem"] = QJsonObject{{"enable", false}};
    json["psychoacousticBass"] = QJsonObject{{"enable", false}};
    json["cure"] = QJsonObject{{"enable", false}};
    // Our convolver/DDC reference external assets; a file path from Linux
    // cannot be used as an Android device-local kernel/profile path.
    json["convolver"] = QJsonObject{
        {"enable", false}, {"kernelFile", ""}, {"crossChannel",
            scaled("convolver_cross_percent", 0, 100.0)}};
    json["ddc"] = QJsonObject{{"enable", false}, {"device", ""}};

    // Clearly disclose nonportable controls rather than inventing equivalents.
    QStringList skipped;
    if (flag("convolver_enabled")) skipped << "Convolver (import the IRS separately)";
    if (flag("ddc_enabled")) skipped << "DDC (import the VDC separately)";
    if (flag("tube_enabled")) skipped << "Tube model/drive/mix (Android supports on/off only)";
    if (flag("crossfeed_enabled")) skipped << "Crossfeed (different implementations)";
    if (flag("field_enabled")) skipped << "Field Surround (scale requires validation)";
    if (flag("fet_enabled") || flag("mb_enabled")) skipped << "compressor advanced settings (pending unit validation)";
    if (flag("psycho_enabled") || flag("dynamic_enabled")) skipped << "advanced Bass/Dynamic System (pending mapping)";
    if (!flag("master_enabled", true)) skipped << "master bypass (Android manages its master switch separately)";
    if (!skipped.isEmpty())
        *warning = "Some controls are not transferable in this experimental export:\n• " + skipped.join("\n• ");
    *output = json;
    return true;
}
} // namespace viper_android_export
