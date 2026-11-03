#!/usr/bin/env python3
"""Static contracts complement the DSP mock; GUI rendering needs Qt6/Arch."""
from pathlib import Path
r = Path(__file__).resolve().parents[1]
s = (r / 'src/viper-ladspa.cpp').read_text()
start = s.index('    void apply_dynamic_eq_from_controls() {')
end = s.index('    void run_dry_while_loading(', start)
method = s[start:end]
assert 'if (!enabled && !last_deq_enabled) return;' in method
assert 'std::lock_guard<std::mutex> guard(wet_mutex);' in method
assert 'configure_dynamic_eq(enabled, count, bands);' in method
assert 'ir_transition.store(true' not in method
assert 'sleep_for' not in method
assert 'wet.ApplyDynamicEq(p)' in s
from gui_source_contract import read_gui_sources
ui = read_gui_sources(r)
# About moved from the footer to the compact application menu.
assert 'menuButton->setObjectName("viperApplicationMenuButton")' in ui
assert 'menuButton->setMenu(makeApplicationMenu(menuButton));' in ui
assert 'menu->addAction("About Viper4Linux")' in ui
assert 'connect(about, &QAction::triggered, this, [this] { showAbout(); });' in ui
assert 'viperAboutButton' not in ui
assert 'VIPER4LINUX_DEV_VERSION' in ui
assert 'VIPER4LINUX_DEV_REVISION' in ui
assert 'VIPER4LINUX_VIPERDSP_REVISION' in ui
assert 'about.setWindowIcon(QIcon(":/viper4linux/blue.png"));' in ui
cm = (r / 'CMakeLists.txt').read_text()
assert 'include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.cmake")' in cm
assert 'set(V4L_BASE_VERSION "1.1.0")' not in cm
version_rules = (r / 'cmake/Version.cmake').read_text()
assert 'describe --tags --abbrev=0' in version_rules
assert 'RELEASE-VERSION' in version_rules
assert 'RELEASE-REVISION' in version_rules
assert 'set(V4L_DEV_VERSION "${V4L_BASE_VERSION}-${V4L_REVISION}")' in version_rules
assert 'configure_file(src/viper-build-info.h.in' in cm
assert 'QString("Version: <b>%1</b><br>"' in ui
print('PASS: Dynamic EQ avoids global dry crossfade; About uses tag/archive version and revision')
