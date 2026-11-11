// ViPERDSP stereo LADSPA adapter for PipeWire's filter-chain.
// Prototype only. The underlying ViPERDSP rights must be reviewed before distribution.
#include <ladspa.h>
#include "viperdsp_compat.h" // Include missing upstream standard headers first.
#include "viper/ViPER.h"
#include "viper-control.h"
#include "viper-input-signal.h"
#include "viper-ir-wave.h"
#include "viper-ddc.h"
#include "viper-multiband-init.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <string>
#include <new>
#include <mutex>
#include <cstdio>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

namespace {
constexpr uint32_t kChunk = 2048;
constexpr unsigned long kPluginId = 2780030;

enum Port : unsigned long {
    InputLeft, InputRight, OutputLeft, OutputRight,
    BassEnabled, BassStrength, BassFrequency,
    PortCount
};

#include "backend/startup-settings.inc"

// One shared control page per filter-chain process, even if LADSPA is
// instantiated more than once during audio reconfiguration.
std::mutex control_mutex;
viperbass::Mapping control_mapping;
unsigned control_users = 0;
viperbass::Controls* acquire_controls() {
    std::lock_guard<std::mutex> lock(control_mutex);
    if (control_users == 0) {
        if (!control_mapping.create_server()) {
            std::fprintf(stderr, "ViPER: Cannot create clean control channel; DSP will use fallback defaults.\n");
            // Audio still functions using LADSPA defaults if another instance owns IPC.
            // Do not increment the reference count after a failed setup.
            return nullptr;
        }
        load_saved_settings(*control_mapping.get());
    }
    ++control_users;
    return control_mapping.get();
}
void release_controls() {
    std::lock_guard<std::mutex> lock(control_mutex);
    if (control_users && --control_users == 0) control_mapping.stop_server();
}

#include "backend/processor.inc"

LADSPA_Handle create(const LADSPA_Descriptor*, unsigned long rate) {
    try { return new Processor(rate); } catch (...) { return nullptr; }
}
void attach(LADSPA_Handle handle, unsigned long index, LADSPA_Data* data) {
    if (handle && index < PortCount) static_cast<Processor*>(handle)->ports[index] = data;
}
void run(LADSPA_Handle handle, unsigned long frames) {
    if (handle) static_cast<Processor*>(handle)->run(frames);
}
void destroy(LADSPA_Handle handle) { delete static_cast<Processor*>(handle); }

const LADSPA_PortDescriptor descriptors[] = {
    LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO, LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO,
    LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO, LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,
    LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL, LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL
};
const char* names[] = {
    "Input L", "Input R", "Output L", "Output R",
    "Bass Enabled", "Bass Strength", "Bass Frequency"
};
const LADSPA_PortRangeHint hints[] = {
    {}, {}, {}, {},
    { LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_1, 0.0f, 1.0f },
    { LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_MAXIMUM, 0.5f, 10.0f },
    { LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_MIDDLE, 15.0f, 150.0f },
};

const LADSPA_Descriptor descriptor = {
    kPluginId, "viperbass_stereo", 0,
    "ViPER for Linux Clean — ViPER Bass", "ViPER for Linux prototype",
    "ViPERDSP upstream rights reserved; prototype is not for redistribution",
    PortCount, descriptors, names, hints, nullptr,
    create, attach, nullptr, run, nullptr, nullptr, nullptr, destroy
};
} // namespace

extern "C" const LADSPA_Descriptor* ladspa_descriptor(unsigned long index) {
    return index == 0 ? &descriptor : nullptr;
}
