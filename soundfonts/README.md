# SoundFonts

This directory should contain a SoundFont (.sf2 or .sf3) file for MIDI synthesis.

Both SF2 (uncompressed) and SF3 (OGG Vorbis compressed) formats are supported.

## Quick Setup

A default soundfont (FluidR3Mono_GM.sf3, 23MB) is included if you cloned the full repository.

## Recommended SoundFonts

### Included - FluidR3Mono GM (~23MB)
- **FluidR3Mono_GM.sf3**: Good quality, compressed format
  ```bash
  wget -O default.sf3 "https://github.com/musescore/MuseScore/raw/master/share/sound/FluidR3Mono_GM.sf3"
  ```

### Medium (~30MB) - Good balance
- **GeneralUser GS**: High quality, reasonable size
  ```bash
  wget "https://www.schristiancollins.com/soundfonts/GeneralUser_GS_1.471.zip"
  unzip GeneralUser_GS_1.471.zip
  mv "GeneralUser GS 1.471/GeneralUser GS v1.471.sf2" default.sf2
  rm -rf "GeneralUser GS 1.471" GeneralUser_GS_1.471.zip
  ```

### Large (~140MB) - Best quality
- **FluidR3_GM**: Professional quality (stereo)
  ```bash
  wget "https://keymusician01.s3.amazonaws.com/FluidR3_GM.zip"
  unzip FluidR3_GM.zip
  mv FluidR3_GM/FluidR3_GM.sf2 default.sf2
  rm -rf FluidR3_GM FluidR3_GM.zip
  ```

## Usage

Place your soundfont file in one of these locations:

1. `./soundfont.sf2` or `./default.sf2` (current directory)
2. `soundfonts/default.sf2` (this directory)
3. `$HOME/soundfonts/default.sf2`
4. Set the `TERMUX_MIDI_SF2` environment variable
5. Use the `--sf2` command line option

## Environment Variable

```bash
export TERMUX_MIDI_SF2="$HOME/soundfonts/FluidR3_GM.sf2"
```

Add this to your `.bashrc` or `.zshrc` for persistence.
