# Replicant Edge factory presets

The factory bank, exported as `.edgy` files by the same `PresetManager` the
plugin uses:

```
presets/
├── Bass/    Acid Protocol, Sub Level, Chrome Sub, Dark Current
├── Drone/   Reactor Hum, Obsidian Drone, Voltage Field
├── FX/      Cross Patch, Velocity Filter, Envelope Sweep,
│            Feedback Grid, Clock Divide, Patch Lab, Rate Chaos
├── Lead/    Neon Lead, Ghost Wire, Laser Grid, Chrome Solo
├── Noise/   Static Rain, Pink Corridor, White Noise Hit
└── Perc/    Neon Pulse, Chrome Rain, Razor Wire, Voltage Drop, Toxic Bloom
```

Each `.edgy` file is human-readable XML containing:

* every panel parameter (oscillators, filter, envelopes, sequencer, tempo),
* the eight pitch and velocity steps,
* the patch cables (source jack, destination jack, cable colour).

Import a file from the plugin's **PRESET ARCHIVE** drawer, or drop it into the
user preset folder:

* Linux: `~/.config/ReplicantEdgy/Presets`
* macOS: `~/Library/Application Support/ReplicantEdgy/Presets`
* Windows: `%APPDATA%\ReplicantEdgy\Presets`

Regenerate this folder with:

```bash
cmake --build build --target generate_presets
./build/generate_presets
```

The generator is deterministic, so re-running it produces identical files. CI
fails if the checked-in presets drift from the code.
