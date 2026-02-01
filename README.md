# termux-midi

A command-line MIDI synthesizer for Termux that uses TinySoundFont for synthesis and OpenSL ES for audio output.

## Features

- **MIDI File Playback**: Play standard MIDI files
- **Real-time Mode**: Send MIDI commands via stdin or Unix socket
- **SoundFont Support**: Use any SF2 or SF3 (compressed) soundfont
- **Low Latency**: OpenSL ES buffer queue for minimal audio latency

## Building

### Native build (in Termux)

```bash
pkg install clang make

make
make install  # optional: install to $PREFIX/bin
```

### Cross-compilation (from Linux host)

For building outside Termux, set these environment variables:

| Variable | Description |
|----------|-------------|
| `NDK_PATH` | Path to Android NDK installation |
| `TERMUX_SYSROOT` | Path to Termux sysroot with headers/libs |
| `ARCH` | Target: `aarch64` (default), `armv7a`, or `x86_64` |
| `API_LEVEL` | Android API level (default: 24) |
| `USE_ALSA` | ALSA support: `1` or `0` (auto-detected) |

```bash
# Example: cross-compile for aarch64 with ALSA
export NDK_PATH=/opt/android-ndk-r26b
export TERMUX_SYSROOT=/path/to/termux-packages/termux-sysroot
make

# Build for 32-bit ARM
make arm

# Build without ALSA
make no-alsa

# Show build configuration
make info
```

## Quick Start

1. **Get a SoundFont** (see `soundfonts/README.md`):
   ```bash
   cd soundfonts
   wget -O default.sf3 "https://github.com/musescore/MuseScore/raw/master/share/sound/FluidR3Mono_GM.sf3"
   cd ..
   ```

2. **Play a MIDI file**:
   ```bash
   ./termux-midi play song.mid
   ```

3. **Real-time mode**:
   ```bash
   ./termux-midi listen
   # Then type commands:
   noteon 0 60 100
   noteoff 0 60
   quit
   ```

## Usage

```
termux-midi <command> [options]

Commands:
  play <file.mid>        Play a MIDI file
  listen                 Real-time mode (read commands from stdin)
  list-instruments       List instruments in soundfont

Options:
  --sf2 <path>           Path to SoundFont file
  --socket <path>        Listen on Unix socket instead of stdin
```

## Real-time Commands

| Command | Description |
|---------|-------------|
| `noteon <ch> <note> <vel>` | Play a note (velocity 0-127) |
| `noteoff <ch> <note>` | Stop a note |
| `cc <ch> <ctrl> <val>` | Control change |
| `pc <ch> <prog>` | Program change (select instrument) |
| `pitch <ch> <val>` | Pitch bend (0-16383, 8192=center) |
| `panic` | All notes off |
| `sleep <seconds>` | Wait (for scripting) |
| `quit` | Exit |

## Examples

### Play a chord
```bash
echo -e "noteon 0 60 100\nnoteon 0 64 100\nnoteon 0 67 100\nsleep 1\npanic" | ./termux-midi listen
```

### Change instrument and play
```bash
cat << 'EOF' | ./termux-midi listen
pc 0 25
noteon 0 60 80
sleep 0.5
noteoff 0 60
quit
EOF
```

### Socket mode for external control
```bash
# Terminal 1: Start the synth
./termux-midi listen --socket /tmp/midi.sock

# Terminal 2: Send commands
echo "noteon 0 60 100" | nc -U /tmp/midi.sock
```

## Environment Variables

- `TERMUX_MIDI_SF2`: Default soundfont path

## Architecture

```
┌──────────────┐     ┌───────────────┐     ┌────────────┐
│ MIDI Input   │────►│ TinySoundFont │────►│ OpenSL ES  │──►🔊
│ stdin/socket │     │ (synthesis)   │     │ BufferQueue│
└──────────────┘     └───────────────┘     └────────────┘
```

## Dependencies

All dependencies are vendored as single-header libraries:

- **TinySoundFont**: SF2 synthesis
- **TinyMidiLoader**: MIDI file parsing

## License

MIT
