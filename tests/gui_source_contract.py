"""Read actual GUI source files for modularization source contracts."""
from pathlib import Path
GUI_FILES = (
    "src/viper-gui.cpp",
    "src/widgets/effect-widgets.h",
    "src/widgets/irs-combo-box.h",
    "src/widgets/eq-overviews.h",
    "src/widgets/graphic-eq-editor.h",
    "src/widgets/dynamic-eq-editor.h",
    "src/viper-theme.h",
    "src/panels/output-panels.h",
    "src/panels/bass-panels.h",
    "src/panels/eq-tone-panels.h",
    "src/panels/spatial-panels.h",
    "src/panels/dynamics-panels.h",
    "src/panels/correction-panels.h",
    "src/sync/gui-control-channel.h",
    "src/sync/gui-settings-bridge.inc",
    "src/sync/gui-simple-settings.inc",
    "src/sync/gui-backend-bridge.inc",
    "src/window/window-presets.inc",
    "src/window/window-layout.inc",
    "src/window/window-correction.inc",
    "src/window/window-shell.inc",
    "src/window/window-graphic-eq.inc",
    "src/window/window-initialization.inc",
    "src/window/window-dynamics.inc",
    "src/window/window-effect-actions.inc",
)
def read_gui_sources(root: Path) -> str:
    return "\n\n".join((root / filename).read_text() for filename in GUI_FILES)
