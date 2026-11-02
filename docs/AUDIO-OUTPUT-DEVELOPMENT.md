# Audio Output memory — staged implementation

The feature deliberately keeps **Audio Outputs** separate from named effect
presets. Output identification is based on structured PipeWire properties; a
substring of a display name is never a device identifier.

## Stage 1: safe, read-only discovery

The first implementation provides `src/viper_output_graph.py` and
`src/viper_output_info.py`. It distinguishes ViPER's virtual smart-filter node
from an actual device-backed `Audio/Sink`; when the smart-filter playback node
is linked, its physical destination takes precedence over default metadata.
Unresolved/ambiguous or device-less outputs are never guessed.

The exact device association key is computed from the stable `device.name` and
`node.name` properties. PipeWire's numeric object ID is used *only* within an
active graph snapshot, never as a persistent key. Future work must verify
profile, port, and multi-output behavior against actual hardware traces.

Run locally (no audio changes):

```sh
python3 src/viper_output_info.py --json
python3 -m unittest discover -s tests -p 'test_output_*.py' -v
```

`src/viper_output_store.py` supplies atomic, independently versioned storage
under `$XDG_DATA_HOME/viper4linux/outputs/` for remembered outputs and their
settings snapshots, isolated from the named preset library.

## Not implemented in this stage

Automatic capture and restoration of **all** DSP effect values, an output
monitor process managed by the existing backend launcher, UI/CLI integration,
and applying saved full-effect presets to a remembered output require subsequent
commits. Merely enabling the store option does not switch DSP configurations.
Do not wire the store into live audio before a complete, tested serialization
and restoration path exists.

No systemd units are required or installed by this stage.
