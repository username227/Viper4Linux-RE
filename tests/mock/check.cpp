#include "viper-control.h"
#include <ladspa.h>
#include <dlfcn.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

#ifndef VIPER_MOCK_TEST_IPC
#error "Mock checks must compile with isolated shared-memory test namespace"
#endif

static void near(float got, float want) {
    if (std::fabs(got - want) > 1.0e-5f) {
        std::fprintf(stderr, "Unexpected output: got %.8f, expected %.8f\n", got, want);
        std::abort();
    }
}
int main(int argc, char** argv) {
    assert(argc == 2 || (argc == 3 && std::string(argv[2]) == "--precision"));
    // Catch accidental regressions to the production IPC namespace.
    assert(viperbass::name().find("/viperlinux-unified-v5-mock-") == 0);
    void *lib = dlopen(argv[1], RTLD_NOW);
    if (!lib) { std::fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    using DescFunc = const LADSPA_Descriptor* (*)(unsigned long);
    auto f = reinterpret_cast<DescFunc>(dlsym(lib, "ladspa_descriptor"));
    assert(f);
    const auto* d = f(0);
    assert(d && d->PortCount == 7 && !f(1));
    auto h = d->instantiate(d, 48000);
    assert(h);
    viperbass::Mapping page;
    assert(page.attach());
    auto* p = page.get();
    if (argc == 3) {
        // A second independent instantiation verifies that exact native values
        // override the legacy [General] GUI sliders during backend startup.
        assert(p->strength_centi.load() == 238);
        assert(p->bass_mono_strength_centi.load() == 247);
        assert(p->clarity_gain_milli.load() == 2675);
        assert(p->spectrum_frequency_hz.load() == 8024);
        assert(p->spectrum_exciter_milli.load() == 2345);
        assert(p->diff_lp_hz.load() == 4267);
        assert(p->field_depth.load() == 537);
        d->cleanup(h);
        dlclose(lib);
        std::printf("PASS: exact CLI values survive DSP initialization without the GUI\n");
        return 0;
    }
    assert(p->strength_centi.load() == 500);
    assert(p->frequency_hz.load() == 90);
    assert(p->bass_mode.load() == 2); // restored from config without GUI launch
    // A legacy input_percent=30 value in the settings must have NO effect.
    assert(p->enabled.load() == 1);
    assert(p->master_enabled.load() == 0);
    assert(p->clarity_enabled.load() == 1);
    assert(p->clarity_mode.load() == 2);
    assert(p->clarity_gain_milli.load() == 2500);
    assert(p->spectrum_enabled.load() == 1);
    assert(p->spectrum_frequency_hz.load() == 6800);
    assert(p->spectrum_exciter_milli.load() == 2000);
    assert(p->headphone_enabled.load() == 1);
    assert(p->headphone_quality.load() == 3);
    assert(p->diff_enabled.load() == 1);
    assert(p->diff_delay_ms.load() == 8);
    assert(p->diff_reverse.load() == 1);
    assert(p->diff_mix_percent.load() == 60);
    assert(p->diff_lp_hz.load() == 4000);
    assert(p->field_enabled.load() == 1);
    assert(p->field_widen_tenths.load() == 20);
    assert(p->field_mid_hundredths.load() == 125);
    assert(p->field_depth.load() == 400);
    assert(p->reverb_enabled.load() == 1);
    assert(p->reverb_room_percent.load() == 60);
    assert(p->reverb_width_percent.load() == 70);
    assert(p->reverb_damp_percent.load() == 40);
    assert(p->reverb_wet_percent.load() == 20);
    assert(p->reverb_dry_percent.load() == 90);
    assert(p->sample_rate_hz.load() == 48000);
    assert(p->convolver_ready.load() == 1); // native loader accepted our test WAV
    assert(p->convolver_status.load() == 1);
    assert(p->convolver_enabled.load() == 0);
    assert(p->eq_enabled.load() == 0);
    assert(p->eq_band_count.load() == 15);
    assert(p->eq_levels_hundredths[0].load() == 500);
    assert(p->bass_mono_enabled.load() == 0);
    assert(p->bass_mono_mode.load() == 1);
    assert(p->bass_mono_strength_centi.load() == 200);
    assert(p->bass_mono_frequency_hz.load() == 75);
    assert(p->psycho_enabled.load() == 0);
    assert(p->psycho_cutoff_hz.load() == 90);
    assert(p->psycho_order.load() == 4);
    assert(p->dynamic_enabled.load() == 0);
    assert(p->dynamic_x_high.load() == 6000);
    assert(p->limiter_threshold_permille.load() == 500);
    assert(p->limiter_output_percent.load() == 100);
    assert(p->limiter_pan_percent.load() == 0);
    assert(p->agc_enabled.load() == 0);
    assert(p->agc_strength_centi.load() == 100);
    assert(p->agc_max_gain_centi.load() == 300);
    assert(p->agc_threshold_permille.load() == 900);
    assert(p->lufs_enabled.load() == 0);
    assert(p->lufs_target_tenths.load() == -140);
    assert(p->lufs_max_gain_tenths.load() == 60);
    assert(p->lufs_speed.load() == 1);
    std::vector<float> l(2048, 0.03f), r(2048, 0.03f), ol(2048), orr(2048);
    float enabled=1, strength=10, freq=65;
    float* ports[] = { l.data(), r.data(), ol.data(), orr.data(), &enabled, &strength, &freq };
    for (unsigned long i=0; i<7; ++i) d->connect_port(h, i, ports[i]);
    auto run = [&] (float expected) {
        d->run(h, 2048); // 2048 frames allow the 20 ms global-bypass crossfade to settle.
        near(ol[2047], expected);
        near(orr[2047], expected);
    };
    // Global bypass receives full, unattenuated input (0.03).
    p->bass_mode.store(0); // prior tests use Natural as baseline
    p->diff_enabled.store(0); // old four-effect regression tests must remain exact
    p->field_enabled.store(0); // Field must not change earlier output
    p->reverb_enabled.store(0); // Reverb must not change previous effect tests
    run(.03f);
    // Existing Bass + Clarity path must not regress.
    p->spectrum_enabled.store(0);
    p->headphone_enabled.store(0);
    p->master_enabled.store(1);
    run(.04935f);
    p->enabled.store(0);
    run(.03525f); // Clarity only
    p->clarity_mode.store(1);
    p->clarity_gain_milli.store(4000);
    run(.03675f); // Clarity mode + gain changed live
    p->clarity_enabled.store(0);
    run(.03f); // No active effects; unity input preserved
    p->spectrum_enabled.store(1);
    run(.03f * 1.083f); // Spectrum alone, reference 6800 Hz, exciter 2.0
    p->spectrum_frequency_hz.store(8200);
    p->spectrum_exciter_milli.store(3000);
    run(.03f * 1.135f); // Live reference and harmonic strength adjustment
    p->enabled.store(1);
    p->clarity_enabled.store(1);
    run(.03f * 1.135f * 1.4f * 1.225f); // All three enabled
    p->spectrum_enabled.store(0);
    run(.03f * 1.4f * 1.225f); // Turning off Spectrum leaves Bass and Clarity intact
    p->headphone_enabled.store(1);
    p->headphone_quality.store(0);
    run(.03f * 1.4f * 1.225f * 1.08f); // Headphone Surround with Bass+Clarity
    p->headphone_quality.store(4);
    run(.03f * 1.4f * 1.225f * 1.4f); // Live VHE quality change
    p->enabled.store(0);
    p->clarity_enabled.store(0);
    run(.03f * 1.4f); // Headphone Surround alone
    p->spectrum_enabled.store(1);
    run(.03f * 1.4f * 1.135f); // Headphone + Spectrum
    p->enabled.store(1);
    p->clarity_enabled.store(1);
    run(.03f * 1.4f * 1.225f * 1.4f * 1.135f); // All four
    p->master_enabled.store(0);
    run(.03f); // Global bypass affects all four
    p->headphone_enabled.store(0);
    p->master_enabled.store(1);
    run(.03f * 1.4f * 1.225f * 1.135f); // Disabling Surround leaves other 3 intact
    // Select Bass processing modes live without disturbing Clarity/Spectrum.
    // Current conditions: Bass+Clarity+Spectrum ON, Surround OFF.
    p->bass_mode.store(1);
    run(.03f * 1.5f * 1.225f * 1.135f); // Pure Bass+
    p->bass_mode.store(2);
    run(.03f * 1.6f * 1.225f * 1.135f); // Subwoofer
    p->bass_mode.store(0);
    run(.03f * 1.4f * 1.225f * 1.135f); // Return to Natural
    p->enabled.store(0);
    p->bass_mode.store(2);
    run(.03f * 1.225f * 1.135f); // Bass disabled: mode never changes other effects
    p->master_enabled.store(0);
    p->enabled.store(1);
    run(.03f); // Master bypass with Subwoofer selected
    p->master_enabled.store(1);
    run(.03f * 1.6f * 1.225f * 1.135f); // Re-enable effects: selected Subwoofer retained
    // Differential Surround is independently adjustable and composes with Bass/Clarity.
    p->enabled.store(0);
    p->clarity_enabled.store(0);
    p->spectrum_enabled.store(0);
    p->headphone_enabled.store(0);
    p->diff_enabled.store(1);
    p->diff_delay_ms.store(8);
    p->diff_reverse.store(0);
    p->diff_mix_percent.store(50);
    p->diff_lp_hz.store(0);
    run(.03f * 1.18f); // Delay 8 ms + blend 50%, alone
    p->diff_delay_ms.store(18);
    run(.03f * 1.28f); // Delay changes take effect live
    p->diff_reverse.store(1);
    run(.03f * 1.33f); // Reverse channel changes take effect live
    p->diff_mix_percent.store(75);
    run(.03f * 1.38f); // Delayed channel mix changes take effect live
    p->diff_lp_hz.store(8000);
    run(.03f * 1.4f); // Delayed channel low-pass changes live
    p->enabled.store(1);
    p->clarity_enabled.store(1);
    run(.03f * 1.6f * 1.225f * 1.4f); // Bass Subwoofer + Clarity + Differential
    p->master_enabled.store(0);
    run(.03f); // Global bypass must disable Differential too
    p->master_enabled.store(1);
    p->diff_enabled.store(0);
    run(.03f * 1.6f * 1.225f); // Disabling Differential leaves existing effects unchanged
    // Field Surround toggles, params and composition remain independent.
    p->enabled.store(0);
    p->clarity_enabled.store(0);
    p->spectrum_enabled.store(0);
    p->headphone_enabled.store(0);
    p->diff_enabled.store(0);
    p->field_enabled.store(1);
    p->field_widen_tenths.store(20);
    p->field_mid_hundredths.store(125);
    p->field_depth.store(400);
    run(.03f * (1.f + .02f + .01f + .02f * 400.f / 950.f));
    p->field_widen_tenths.store(40);
    run(.03f * (1.f + .04f + .01f + .02f * 400.f / 950.f));
    p->field_mid_hundredths.store(175);
    run(.03f * (1.f + .04f + .03f + .02f * 400.f / 950.f));
    p->field_depth.store(800);
    run(.03f * (1.f + .04f + .03f + .02f * 800.f / 950.f));
    p->enabled.store(1);
    run(.03f * 1.6f * (1.f + .04f + .03f + .02f * 800.f / 950.f));
    p->master_enabled.store(0);
    run(.03f);
    p->master_enabled.store(1);
    p->field_enabled.store(0);
    run(.03f * 1.6f);
    // New reverb independently adjustable at runtime. Test five controls,
    // toggling off, global bypass, and composition with existing Bass.
    p->reverb_enabled.store(1);
    p->reverb_room_percent.store(60);
    p->reverb_width_percent.store(70);
    p->reverb_damp_percent.store(40);
    p->reverb_wet_percent.store(20);
    p->reverb_dry_percent.store(90);
    auto reverb_factor = [&]{
        return 1.f + .01f * p->reverb_room_percent.load()/100.f
            + .02f * p->reverb_width_percent.load()/100.f
            + .03f * p->reverb_damp_percent.load()/100.f
            + .04f * p->reverb_wet_percent.load()/100.f
            + .05f * p->reverb_dry_percent.load()/100.f;
    };
    run(.03f * 1.6f * reverb_factor());
    p->reverb_room_percent.store(80);
    run(.03f * 1.6f * reverb_factor());
    p->reverb_width_percent.store(10);
    run(.03f * 1.6f * reverb_factor());
    p->reverb_damp_percent.store(90);
    run(.03f * 1.6f * reverb_factor());
    p->reverb_wet_percent.store(75);
    run(.03f * 1.6f * reverb_factor());
    p->reverb_dry_percent.store(50);
    run(.03f * 1.6f * reverb_factor());
    p->master_enabled.store(0);
    run(.03f);
    p->master_enabled.store(1);
    p->reverb_enabled.store(0);
    run(.03f * 1.6f);
    // Convolver: native kernel loading occurs on instantiate, enable/cross live.
    p->enabled.store(0);
    p->convolver_enabled.store(1);
    run(.03f * 1.2f);
    p->convolver_cross_percent.store(50);
    run(.03f * 1.25f);
    p->master_enabled.store(0);
    run(.03f);
    p->master_enabled.store(1);
    run(.03f * 1.25f);
    p->convolver_enabled.store(0);
    run(.03f);
    assert(p->audio_callbacks.load() == 50);
    // Master OFF must be true Android-style bypass, including limiter,
    // output gain and pan. The legacy mock bypass tests used only 0.03
    // with unity output settings and could not catch this regression.
    std::fill(l.begin(), l.end(), 0.8f);
    std::fill(r.begin(), r.end(), -0.8f);
    p->master_enabled.store(0);
    p->limiter_threshold_permille.store(300);
    p->limiter_output_percent.store(200);
    p->limiter_pan_percent.store(100);
    d->run(h, 2048);
    for (int i = 1024; i < 2048; ++i) {
        assert(ol[i] == l[i] && orr[i] == r[i]); // exact, no 256-frame latency
    }
    p->limiter_output_percent.store(30);
    p->limiter_pan_percent.store(-100);
    d->run(h, 2048);
    for (int i = 0; i < 2048; ++i) {
        assert(ol[i] == l[i] && orr[i] == r[i]); // off-state edits must not leak
    }
    // With Master ON the limiter is still mandatory even if no optional
    // effect is selected. A 0.3 ceiling clips this deliberately loud input.
    p->master_enabled.store(1);
    p->limiter_output_percent.store(200);
    p->limiter_pan_percent.store(0);
    d->run(h, 2048);
    near(ol[2047], 0.3f);
    near(orr[2047], -0.3f);
    p->limiter_pan_percent.store(100);
    d->run(h, 2048);
    near(ol[2047], 0.f);
    near(orr[2047], -0.3f);
    p->limiter_pan_percent.store(-100);
    d->run(h, 2048);
    near(ol[2047], 0.3f);
    near(orr[2047], 0.f);
    // Restore all source samples and limiter controls for subsequent IR checks.
    std::fill(l.begin(), l.end(), 0.03f);
    std::fill(r.begin(), r.end(), 0.03f);
    p->limiter_threshold_permille.store(500);
    p->limiter_output_percent.store(100);
    p->limiter_pan_percent.store(0);
    d->run(h, 2048);
    near(ol[2047], 0.03f);
    near(orr[2047], 0.03f);
    std::printf("PASS: Android-style Master bypass and mandatory in-chain limiter\n");
    // Live IPC kernel replacement must not require LADSPA destroy/instantiate.
    const std::string confpath = std::string(std::getenv("XDG_CONFIG_HOME")) + "/ViPERforLinux/ViPERClean.conf";
    const std::string original = std::string(std::getenv("XDG_CONFIG_HOME")) + "/impulse.irs";
    const auto queue_ir = [&](const std::string& path) {
        std::ifstream f(confpath); assert(f);
        std::string contents((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const auto at = contents.find("convolver_kernel=");
        assert(at != std::string::npos);
        const auto end = contents.find('\n', at);
        contents.replace(at, end == std::string::npos ? std::string::npos : end-at, "convolver_kernel=" + path);
        std::ofstream o(confpath, std::ios::trunc); o << contents; o.close();
        return p->convolver_request_serial.fetch_add(1) + 1;
    };
    const auto await_ir = [&](std::uint32_t expected) {
        bool done = false;
        for (int n=0; n<300; ++n) {
            if (p->convolver_applied_serial.load() == expected) { done=true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        assert(done && "Live IR load request timed out");
    };
    const auto save_ir = [&](const std::string& path) { await_ir(queue_ir(path)); };
    save_ir(std::string(std::getenv("XDG_CONFIG_HOME")) + "/missing.irs");
    assert(p->convolver_status.load() == -1);
    assert(p->convolver_ready.load() == 1); // Invalid request retains previous valid kernel
    p->convolver_enabled.store(1);
    p->convolver_cross_percent.store(0);
    run(.03f * 1.2f); // Prior kernel still active.
    const auto nextIr = queue_ir(std::string(std::getenv("XDG_CONFIG_HOME")) + "/impulse-alt.irs");
    // The mock loader deliberately spends 95ms constructing the replacement kernel.
    // The audio callback must keep producing samples with the same LADSPA handle.
    bool loading = false;
    for (int n=0; n<100; ++n) {
        if (p->convolver_status.load() == 2) { loading = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(loading);
    std::this_thread::sleep_for(std::chrono::milliseconds(65));
    d->run(h, 2048);
    near(ol[2047], .03f); // Nonblocking dry path during kernel construction.
    await_ir(nextIr);
    assert(p->convolver_ready.load() == 1);
    assert(p->convolver_status.load() == 1); // 44.1k IR resampled live to 48k
    run(.03f * 1.2f); // Same instance processed audio with the replacement kernel
    save_ir("");
    assert(p->convolver_ready.load() == 0);
    assert(p->convolver_status.load() == 0);
    run(.03f); // Clearing the kernel also works without re-instantiation
    save_ir(original); // Restore fixture for startup resampling checks below
    assert(p->convolver_status.load() == 1);
    p->convolver_enabled.store(0);
    d->cleanup(h);
    assert(!viperbass::running(p));
    page.close();
    // The upstream VHE kernel provides 44.1/48k processing only.
    // At 96k the requested toggle remains saved but the effect must stay bypassed.
    h = d->instantiate(d, 96000);
    assert(h);
    assert(page.attach());
    p = page.get();
    assert(p->sample_rate_hz.load() == 96000);
    assert(p->convolver_ready.load() == 1);
    assert(p->convolver_status.load() == 1); // 48k IR resampled during startup
    assert(p->bass_mode.load() == 2);
    for (unsigned long i=0; i<7; ++i) d->connect_port(h, i, ports[i]);
    p->enabled.store(0);
    p->clarity_enabled.store(0);
    p->spectrum_enabled.store(0);
    p->headphone_enabled.store(1);
    p->headphone_quality.store(4);
    p->diff_enabled.store(0);
    p->field_enabled.store(0);
    p->reverb_enabled.store(0);
    p->master_enabled.store(1);
    d->run(h, 2048);
    near(ol[2047], .03f);
    assert(p->headphone_enabled.load() == 1);
    d->cleanup(h);
    page.close();
    // At 44.1k the same requested Headphone Surround setting must take effect.
    h = d->instantiate(d, 44100);
    assert(h);
    assert(page.attach());
    p = page.get();
    assert(p->sample_rate_hz.load() == 44100);
    assert(p->convolver_ready.load() == 1);
    assert(p->convolver_status.load() == 1); // 48k IR resampled during startup
    assert(p->bass_mode.load() == 2);
    for (unsigned long i=0; i<7; ++i) d->connect_port(h, i, ports[i]);
    p->enabled.store(0);
    p->clarity_enabled.store(0);
    p->spectrum_enabled.store(0);
    p->headphone_enabled.store(1);
    p->headphone_quality.store(4);
    p->diff_enabled.store(0);
    p->field_enabled.store(0);
    p->reverb_enabled.store(0);
    p->master_enabled.store(1);
    d->run(h, 2048);
    near(ol[2047], .03f * 1.4f);
    // Three independent new Bass processors; verify parameter forwarding,
    // composition, zero-effect on disable and global bypass.
    p->headphone_enabled.store(0);
    p->bass_mono_enabled.store(1);
    p->bass_mono_mode.store(2);
    p->bass_mono_strength_centi.store(300);
    p->bass_mono_frequency_hz.store(80);
    auto monoFactor = [&]{ return 1.f + .02f*p->bass_mono_mode.load()
                                  + .005f*p->bass_mono_strength_centi.load()/100.f
                                  + .001f*p->bass_mono_frequency_hz.load(); };
    d->run(h, 2048);
    near(ol[2047], .03f * monoFactor());
    p->bass_mono_mode.store(0);
    d->run(h, 2048);
    near(ol[2047], .03f * monoFactor());
    p->psycho_enabled.store(1);
    p->psycho_intensity_percent.store(50);
    p->psycho_order.store(5);
    p->psycho_original_percent.store(80);
    auto psychoFactor = [&]{ return 1.f + .01f*p->psycho_intensity_percent.load()/100.f
        + .002f*p->psycho_order.load() + .001f*p->psycho_cutoff_hz.load()/100.f
        + .003f*p->psycho_original_percent.load()/100.f; };
    d->run(h, 2048);
    near(ol[2047], .03f * monoFactor() * psychoFactor());
    p->dynamic_enabled.store(1);
    p->dynamic_strength_percent.store(40);
    auto dynFactor = [&]{return 1.f + .002f*p->dynamic_x_low.load()/100.f
        + .001f*p->dynamic_x_high.load()/1000.f + .001f*p->dynamic_y_low.load()/100.f
        + .001f*p->dynamic_y_high.load()/100.f + .002f*p->dynamic_side_low_percent.load()/100.f
        + .002f*p->dynamic_side_high_percent.load()/100.f
        + .08f*p->dynamic_strength_percent.load()/100.f;};
    d->run(h, 2048);
    near(ol[2047], .03f * monoFactor() * psychoFactor() * dynFactor());
    p->master_enabled.store(0);
    d->run(h, 2048);
    near(ol[2047], .03f);
    p->master_enabled.store(1);
    p->bass_mono_enabled.store(0);
    p->psycho_enabled.store(0);
    p->dynamic_enabled.store(0);
    d->run(h, 2048);
    near(ol[2047], .03f);
    // Additional Output tab processors, initially disabled, must preserve
    // the seven-effect behavior and bypass processing independently.
    p->headphone_enabled.store(0);
    p->limiter_output_percent.store(200);
    d->run(h, 2048);
    near(ol[2047], .06f);
    p->limiter_pan_percent.store(50);
    d->run(h, 2048);
    near(ol[2047], .03f);
    near(orr[2047], .06f);
    p->limiter_pan_percent.store(0);
    p->limiter_output_percent.store(100);
    p->agc_enabled.store(1);
    d->run(h, 2048);
    near(ol[2047], .03f * 1.1f);
    p->lufs_enabled.store(1);
    d->run(h, 2048);
    near(ol[2047], .03f * 1.1f * 1.05f);
    p->agc_enabled.store(0);
    d->run(h, 2048);
    near(ol[2047], .03f * 1.05f);
    p->lufs_enabled.store(0);
    d->run(h, 2048);
    near(ol[2047], .03f);
    p->agc_enabled.store(1);
    p->lufs_enabled.store(1);
    p->master_enabled.store(0);
    d->run(h, 2048);  // Master bypass still retains the output limiter.
    near(ol[2047], .03f);
    p->master_enabled.store(1);
    p->agc_enabled.store(0);
    p->lufs_enabled.store(0);
    p->limiter_threshold_permille.store(300);
    p->limiter_output_percent.store(200);
    std::fill(l.begin(), l.end(), 0.75f);
    std::fill(r.begin(), r.end(), 0.75f);
    d->run(h, 2048);
    near(ol[2047], .3f); // Native limiter threshold is propagated; no hard clamp.
    // Legacy input_percent=30 in the on-disk config must not reduce audio.
    // With all DSP bypassed and limiter at unity, output must equal input.
    p->master_enabled.store(0);
    p->limiter_threshold_permille.store(1000);
    p->limiter_output_percent.store(100);
    std::fill(l.begin(), l.end(), 0.2f);
    std::fill(r.begin(), r.end(), 0.2f);
    d->run(h, 2048);
    near(ol[2047], .2f);
    near(orr[2047], .2f);
    // Native EQ control path: no new LADSPA instance and no blocking audio
    // callback when switching the filter-bank size or updating band gains.
    p->master_enabled.store(1);
    p->eq_enabled.store(1);
    p->eq_band_count.store(31);
    p->eq_levels_hundredths[0].store(500); // 5 dB, mock factor +0.05
    auto serial = p->eq_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->eq_applied_serial.load() != serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->eq_applied_serial.load() == serial);
    d->run(h, 2048); // 20-ms crossfade has settled
    near(ol[2047], .2f * 1.05f);
    // Changing 31 -> 10 must discard inactive-band levels.
    p->eq_levels_hundredths[20].store(1200);
    p->eq_band_count.store(10);
    serial = p->eq_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->eq_applied_serial.load() != serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->eq_applied_serial.load() == serial);
    d->run(h, 2048);
    near(ol[2047], .2f * 1.05f);
    p->eq_levels_hundredths[0].store(-500);
    serial = p->eq_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->eq_applied_serial.load() != serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->eq_applied_serial.load() == serial);
    d->run(h, 2048);
    near(ol[2047], .2f * 0.95f);
    p->eq_enabled.store(0);
    serial = p->eq_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->eq_applied_serial.load() != serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->eq_applied_serial.load() == serial);
    d->run(h, 2048);
    near(ol[2047], .2f);
    // Native TubeSimulator control path.  The mock's gain factor checks the
    // control delivery and isolation, not the actual tube transfer curve.
    assert(p->tube_enabled.load() == 0); // safe default
    assert(p->tube_model.load() == 0);
    assert(p->tube_drive_percent.load() == 50);
    assert(p->tube_mix_percent.load() == 30);
    p->tube_enabled.store(1);
    p->tube_model.store(1); // 12AU7
    auto tube_serial = p->tube_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->tube_applied_serial.load() != tube_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->tube_applied_serial.load() == tube_serial);
    d->run(h, 2048);
    near(ol[2047], .2f * (1.f + .12f * .5f * .3f * 1.2f));
    p->tube_model.store(2); // 6N1P
    p->tube_drive_percent.store(80);
    p->tube_mix_percent.store(50);
    tube_serial = p->tube_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->tube_applied_serial.load() != tube_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->tube_applied_serial.load() == tube_serial);
    d->run(h, 2048);
    near(ol[2047], .2f * (1.f + .12f * .8f * .5f * 1.4f));
    // Zero wet mix is neutral; global bypass ignores any active tube setting.
    p->tube_mix_percent.store(0);
    tube_serial = p->tube_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->tube_applied_serial.load() != tube_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->tube_applied_serial.load() == tube_serial);
    d->run(h, 2048);
    near(ol[2047], .2f);
    p->tube_mix_percent.store(50);
    tube_serial = p->tube_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->tube_applied_serial.load() != tube_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->tube_applied_serial.load() == tube_serial);
    p->master_enabled.store(0);
    d->run(h, 2048);
    near(ol[2047], .2f);
    p->master_enabled.store(1);
    p->tube_enabled.store(0);
    tube_serial = p->tube_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->tube_applied_serial.load() != tube_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->tube_applied_serial.load() == tube_serial);
    d->run(h, 2048);
    near(ol[2047], .2f);
    // AnalogX: same native dispatch as production, synthetic mock multiplier.
    // Test each upstream mode and independent enable/master bypass behavior.
    assert(p->analogx_enabled.load() == 0);
    assert(p->analogx_mode.load() == 0);
    p->analogx_enabled.store(1);
    for (int mode = 0; mode < 3; ++mode) {
        p->analogx_mode.store(mode);
        const auto analog_serial = p->analogx_request_serial.fetch_add(1, std::memory_order_release) + 1;
        for (int i = 0; i < 200 && p->analogx_applied_serial.load() != analog_serial; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(p->analogx_applied_serial.load() == analog_serial);
        d->run(h, 2048);
        near(ol[2047], .2f * (1.f + .03f * (mode + 1)));
    }
    p->master_enabled.store(0);
    d->run(h, 2048);
    near(ol[2047], .2f);
    p->master_enabled.store(1);
    p->analogx_enabled.store(0);
    auto analog_serial = p->analogx_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->analogx_applied_serial.load() != analog_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->analogx_applied_serial.load() == analog_serial);
    d->run(h, 2048);
    near(ol[2047], .2f);
    // Native Dynamic EQ: startup disabled; all bands and types forwarded
    // through worker IPC. Switching while playing must never block callback.
    assert(p->deq_enabled.load() == 0);
    assert(p->deq_band_count.load() == 3);
    p->deq_enabled.store(1);
    p->deq_band_count.store(2);
    p->deq_bands[0].frequency_hz.store(125);
    p->deq_bands[0].q_hundredths.store(75);
    p->deq_bands[0].gain_hundredths.store(400);
    p->deq_bands[0].threshold_tenths.store(-240);
    p->deq_bands[0].attack_ms.store(5);
    p->deq_bands[0].release_ms.store(150);
    p->deq_bands[0].filter_type.store(1);
    p->deq_bands[1].frequency_hz.store(8000);
    p->deq_bands[1].q_hundredths.store(150);
    p->deq_bands[1].gain_hundredths.store(-300);
    p->deq_bands[1].threshold_tenths.store(-300);
    p->deq_bands[1].attack_ms.store(20);
    p->deq_bands[1].release_ms.store(200);
    p->deq_bands[1].filter_type.store(2);
    for (int n = 1; n <= 10; n += 9) {
        p->deq_band_count.store(n);
        const auto serial = p->deq_request_serial.fetch_add(1, std::memory_order_release) + 1;
        for (int i = 0; i < 200 && p->deq_applied_serial.load() != serial; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(p->deq_applied_serial.load() == serial);
        // Ensure mock effects are dispatched when the engine processes audio.
        d->run(h, 2048);
    }
    p->deq_enabled.store(0);
    const auto deq_serial = p->deq_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->deq_applied_serial.load() != deq_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->deq_applied_serial.load() == deq_serial);
    // Once disabled, changing an inactive band's saved parameters must be
    // a strict audio no-op even while other native effects are active.
    d->run(h, 2048);
    const float disabled_baseline = ol[2047];
    p->deq_bands[0].gain_hundredths.store(-750);
    const auto disabled_edit = p->deq_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 && p->deq_applied_serial.load() != disabled_edit; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->deq_applied_serial.load() == disabled_edit);
    d->run(h, 2048);
    near(ol[2047], disabled_baseline);
    // Native Cure/Crossfeed: changing a disabled preset must be a strict audio no-op.
    assert(p->crossfeed_enabled.load() == 0);
    assert(p->crossfeed_preset.load() == 0);
    d->run(h, 2048);
    const float cure_baseline = ol[2047];
    p->crossfeed_preset.store(2);
    auto cure_serial = p->crossfeed_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i=0; i<200 && p->crossfeed_applied_serial.load()!=cure_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->crossfeed_applied_serial.load() == cure_serial);
    d->run(h, 2048);
    near(ol[2047], cure_baseline);
    p->crossfeed_enabled.store(1);
    for(int preset=0; preset<3; ++preset) {
        p->crossfeed_preset.store(preset);
        cure_serial = p->crossfeed_request_serial.fetch_add(1, std::memory_order_release) + 1;
        for (int i=0; i<200 && p->crossfeed_applied_serial.load()!=cure_serial; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(p->crossfeed_applied_serial.load() == cure_serial);
        d->run(h, 2048);
        near(ol[2047], cure_baseline * (1.03f + preset * .01f));
    }
    p->master_enabled.store(0);
    d->run(h, 2048);
    near(ol[2047], .2f);
    p->master_enabled.store(1);
    p->crossfeed_enabled.store(0);
    cure_serial = p->crossfeed_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i=0; i<200 && p->crossfeed_applied_serial.load()!=cure_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->crossfeed_applied_serial.load() == cure_serial);
    d->run(h, 2048);
    near(ol[2047], cure_baseline);
    // Native Stereo Imager: default bypass, neutral widths/crossovers, and
    // serialized worker updates while processing. Mock factor is NOT real DSP.
    assert(p->imager_enabled.load() == 0);
    assert(p->imager_low_width_percent.load() == 100);
    assert(p->imager_mid_width_percent.load() == 100);
    assert(p->imager_high_width_percent.load() == 100);
    assert(p->imager_low_crossover_hz.load() == 200);
    assert(p->imager_high_crossover_hz.load() == 4000);
    d->run(h, 2048);
    const float imager_baseline = ol[2047];
    // Editing disabled settings cannot affect the wet path.
    p->imager_mid_width_percent.store(180);
    auto image_serial = p->imager_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for(int i=0; i<200 && p->imager_applied_serial.load()!=image_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->imager_applied_serial.load() == image_serial);
    d->run(h, 2048);
    near(ol[2047], imager_baseline);
    p->imager_enabled.store(1);
    image_serial = p->imager_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for(int i=0; i<200 && p->imager_applied_serial.load()!=image_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->imager_applied_serial.load() == image_serial);
    d->run(h, 2048);
    near(ol[2047], imager_baseline * (1.f + .02f * .8f));
    p->imager_low_width_percent.store(120);
    p->imager_high_width_percent.store(150);
    p->imager_low_crossover_hz.store(350);
    p->imager_high_crossover_hz.store(6000);
    image_serial = p->imager_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for(int i=0; i<200 && p->imager_applied_serial.load()!=image_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->imager_applied_serial.load() == image_serial);
    d->run(h, 2048);
    near(ol[2047], imager_baseline * (1.f + .01f*.2f + .02f*.8f + .03f*.5f + .00001f*150.f + .000001f*2000.f));
    p->master_enabled.store(0);
    d->run(h, 2048);
    near(ol[2047], .2f);
    p->master_enabled.store(1);
    p->imager_enabled.store(0);
    image_serial = p->imager_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for(int i=0; i<200 && p->imager_applied_serial.load()!=image_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->imager_applied_serial.load() == image_serial);
    d->run(h, 2048);
    near(ol[2047], imager_baseline);
    // FET compressor: verify initial bypass, non-RT serial acknowledgment,
    // dB/ratio/time conversion and full master bypass independently.
    assert(p->fet_enabled.load() == 0);
    assert(p->fet_threshold_db.load() == -18);
    assert(p->fet_ratio_tenths.load() == 20);
    assert(p->mb_enabled.load() == 0);
    assert(p->mb_band_count.load() == 3);
    d->run(h, 2048);
    const float compressor_baseline = ol[2047];
    p->fet_enabled.store(1);
    auto fet_serial = p->fet_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i=0; i<200 && p->fet_applied_serial.load()!=fet_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->fet_applied_serial.load() == fet_serial);
    constexpr float db_to_ln = 0.11512925464970229f;
    auto fetFactor = [&]() {
        return 1.f + .03f * -(1.f - 10.f/p->fet_ratio_tenths.load())
            + .005f * (p->fet_threshold_db.load()*db_to_ln)
            + .002f * (p->fet_knee_db.load()*db_to_ln)
            + .004f * (p->fet_makeup_db.load()*db_to_ln)
            + .01f * (p->fet_attack_ms.load()/1000.f)
            + .01f * (p->fet_release_ms.load()/1000.f);
    };
    d->run(h, 2048);
    near(ol[2047], compressor_baseline * fetFactor());
    p->fet_threshold_db.store(-30);
    p->fet_ratio_tenths.store(40);
    p->fet_makeup_db.store(3);
    fet_serial = p->fet_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i=0; i<200 && p->fet_applied_serial.load()!=fet_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->fet_applied_serial.load() == fet_serial);
    d->run(h, 2048);
    near(ol[2047], compressor_baseline * fetFactor());
    p->fet_enabled.store(0);
    fet_serial = p->fet_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i=0; i<200 && p->fet_applied_serial.load()!=fet_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->fet_applied_serial.load() == fet_serial);
    d->run(h, 2048);
    near(ol[2047], compressor_baseline);
    // Native multiband: test 3->5 layout, each band, new crossover,
    // disabled per-band DSP, independent FET and global bypass.
    p->mb_enabled.store(1);
    p->mb_band_count.store(5);
    const int five_xover[]{120,500,4000,8000};
    for (int i=0;i<4;++i) p->mb_crossovers_hz[i].store(five_xover[i]);
    auto mb_serial = p->mb_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i=0; i<200 && p->mb_applied_serial.load()!=mb_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->mb_applied_serial.load() == mb_serial);
    auto mbFactor = [&]() {
        float factor = 1.f + .000001f * p->mb_crossovers_hz[0].load();
        for (int i=0;i<p->mb_band_count.load();++i) {
            auto& b = p->mb_bands[i];
            if (b.enabled.load()) factor += .01f*-(1.f-10.f/b.ratio_tenths.load())
                + .001f*b.threshold_db.load()*db_to_ln
                + .001f*b.attack_ms.load()/1000.f;
        }
        return factor;
    };
    d->run(h, 2048);
    near(ol[2047], compressor_baseline * mbFactor());
    p->mb_bands[2].enabled.store(0);
    p->mb_bands[4].ratio_tenths.store(60);
    p->mb_crossovers_hz[0].store(180);
    mb_serial = p->mb_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i=0; i<200 && p->mb_applied_serial.load()!=mb_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->mb_applied_serial.load() == mb_serial);
    d->run(h, 2048);
    near(ol[2047], compressor_baseline * mbFactor());
    p->master_enabled.store(0);
    d->run(h, 2048);
    near(ol[2047], compressor_baseline);
    p->master_enabled.store(1);
    p->mb_enabled.store(0);
    mb_serial = p->mb_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i=0; i<200 && p->mb_applied_serial.load()!=mb_serial; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->mb_applied_serial.load() == mb_serial);
    d->run(h, 2048);
    near(ol[2047], compressor_baseline);
    // Meter must detect incoming audio, not perpetual always-process callbacks.
    const auto signal_before = p->input_signal_blocks.load();
    for (auto& sample : l) sample = 0.0f;
    for (auto& sample : r) sample = 0.0f;
    d->run(h, 2048);
    assert(p->input_signal_blocks.load() == signal_before);
    l[0] = 0.002f;
    d->run(h, 2048);
    assert(p->input_signal_blocks.load() == signal_before + 1);
    l[0] = 0.0f;
    r[0] = -0.002f;
    d->run(h, 2048);
    assert(p->input_signal_blocks.load() == signal_before + 2);
    d->cleanup(h);
    page.close();

    // Reproduce worker-startup race deterministically. Old code seeded
    // observed_* from the latest request and lost these queued commands.
    // The new code seeds from *applied* serials and processes them correctly.
    assert(setenv("VIPER_MOCK_WORKER_START_DELAY_MS", "400", 1) == 0);
    h = d->instantiate(d, 48000);
    assert(h);
    assert(page.attach());
    p = page.get();
    p->eq_enabled.store(1);
    p->eq_band_count.store(31);
    p->eq_levels_hundredths[0].store(375);
    p->tube_enabled.store(1);
    p->tube_model.store(1);
    const auto pending_eq = p->eq_request_serial.fetch_add(1, std::memory_order_release) + 1;
    const auto pending_tube = p->tube_request_serial.fetch_add(1, std::memory_order_release) + 1;
    const auto pending_ir = p->convolver_request_serial.fetch_add(1, std::memory_order_release) + 1;
    for (int i = 0; i < 200 &&
         (p->eq_applied_serial.load() != pending_eq ||
          p->tube_applied_serial.load() != pending_tube ||
          p->convolver_applied_serial.load() != pending_ir); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(p->eq_applied_serial.load() == pending_eq &&
           "EQ request queued before worker startup was lost");
    assert(p->tube_applied_serial.load() == pending_tube &&
           "Tube request queued before worker startup was lost");
    assert(p->convolver_applied_serial.load() == pending_ir &&
           "Convolver request queued before worker startup was lost");
    d->cleanup(h);
    page.close();
    assert(unsetenv("VIPER_MOCK_WORKER_START_DELAY_MS") == 0);
    dlclose(lib);
    std::puts("PASS: mock LADSPA, native FET/Multiband Compressor/Stereo Imager/Dynamic EQ/AnalogX/Tube Simulator/Graphic EQ/Convolver forwarding, limiter/AGC/LUFS, bypass, IPC isolation, rates, cleanup");
}
