# Fire Progress (VST3 + Standalone)

Pick a main chord (root + major/minor) and a vibe. Fire Progress gives you chord progressions and a melody over them.
Chords go out on MIDI channel 1, melody on channel 2, and a built-in synth plays both.

## What it does
- **Popular**: well-known progressions for the chosen vibe (I-V-vi-IV, vi-IV-I-V, ii-V-I and more), shown as numerals + real chords.
- **Generate**: builds new progressions that start on your main chord and move through related chords. The last 3 stay as chips.
- **New melody**: every press picks a melody that was never used before (remembered inside the project and between sessions).
  The badge shows NEW MELODY or USED BEFORE, plus how many melodies were heard.
- **DAW sync**: tempo and transport follow the DAW automatically (pill in the top right says "DAW sync"). If it says
  "Standalone clock", the host is not sending position info; use Play + BPM.
- **Drag MIDI**: drag "Drag chords MIDI" / "Drag melody MIDI" into a DAW track or piano roll. The file uses the DAW tempo.
  If your DAW does not accept the drag, just click the pad: the .mid is saved to Documents/FireProgress.

## Build (Windows / macOS / Linux)
Needs: CMake 3.22+, git, a C++17 compiler (Visual Studio 2022 on Windows, Xcode tools on macOS), internet (JUCE 7.0.12 is downloaded automatically).

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Output: build/FireProgress_artefacts/Release/VST3/Fire Progress.vst3 (also copied to the system VST3 folder) and a Standalone app.

## Files
- Source/Engine.h: popular library, progression generator, melody generator, MIDI events (plain C++)
- Source/PluginProcessor.*: parameters, host sync, MIDI output, built-in synth, melody memory, MIDI export
- Source/PluginEditor.*: UI (look and feel, chord cards, piano roll, drag pads)
