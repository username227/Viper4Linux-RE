#!/usr/bin/env python3
"""Guard Dynamic EQ handoff against unwanted audio-worker side effects."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "src/backend/processor.inc").read_text()
start = source.index("    void apply_dynamic_eq_from_controls() {")
end = source.index("    void run_dry_while_loading(", start)
method = source[start:end]
assert "if (!enabled && !last_deq_enabled) return;" in method
assert "std::lock_guard<std::mutex> guard(wet_mutex);" in method
assert "configure_dynamic_eq(enabled, count, bands);" in method
assert "ir_transition.store(true" not in method
assert "sleep_for" not in method
print("PASS: Dynamic EQ handoff does not trigger a global dry transition")
