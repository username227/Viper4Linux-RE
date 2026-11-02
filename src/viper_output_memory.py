"""Audio Output transition rules, independent of PipeWire and DSP transport.

An adapter must provide capture() -> complete versioned snapshot, and
restore(snapshot) -> None (raise on failure). The live backend adapter will be
introduced only after complete DSP round-trip tests are available.
"""
from __future__ import annotations

from typing import Protocol, Any

from viper_output_store import OutputStore
from viper_output_graph import AudioOutput


class EffectAdapter(Protocol):
    def capture(self) -> dict[str, Any]: ...
    def restore(self, snapshot: dict[str, Any]) -> None: ...


class OutputMemory:
    def __init__(self, store: OutputStore, effects: EffectAdapter):
        self.store = store
        self.effects = effects
        self.current: AudioOutput | None = None

    def observe(self, output: AudioOutput | None) -> str:
        """Call on a settled physical-output transition, never during a graph flap.

        Returns 'same', 'unknown', 'disabled', 'initialized' or 'restored'.
        Does not change audio when a target is not positively identified.
        """
        if output is None:
            return 'unknown'
        if self.current and self.current.key == output.key:
            if (self.current.description != output.description or
                    self.current.node_name != output.node_name):
                self.store.remember(output)
            self.current = output  # refresh connection label/ephemeral graph id
            return 'same'
        self.store.remember(output)
        if not self.store.enabled():
            self.current = output
            return 'disabled'
        # Capture before the state switch, while the old output's settings are
        # still present in shared memory. The snapshot must be complete.
        original = self.effects.capture()
        if self.current and self.store.settings(self.current.key) != original:
            self.store.save_settings(self.current.key, original)
        destination = self.store.settings(output.key)
        if destination is None:
            # First encounter: keep the active sound instead of surprising the
            # listener with a sudden reset. Memory becomes independent now.
            self.store.save_settings(output.key, original)
            self.current = output
            return 'initialized'
        try:
            self.effects.restore(destination)
        except Exception:
            # Fail closed: do not mark the target as current or overwrite its
            # saved settings. The supervisor must report and retry safely.
            raise
        self.current = output
        return 'restored'

    def checkpoint(self) -> None:
        """Remember live edits, but only with a stable identified active output."""
        if self.store.enabled() and self.current is not None:
            live = self.effects.capture()
            if self.store.settings(self.current.key) != live:
                self.store.save_settings(self.current.key, live)

    def enable(self) -> None:
        """Opt in without modifying the current audio or saved named presets."""
        if not self.store.enabled():
            if self.current is not None:
                self.store.save_settings(self.current.key, self.effects.capture())
            self.store.set_enabled(True)

    def disable(self) -> None:
        """Keep existing per-device memories, but stop following outputs."""
        if self.store.enabled():
            self.checkpoint()
            self.store.set_enabled(False)
