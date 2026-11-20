# Mono GND

Two GND machines of the Elektron Monomachine (GND SIN and GND NOIS) as two
CLAP instruments for Bitwig (macOS, universal binary). Each one includes
the track's AMP envelope.

The DSP is a fixed-point C port of these machines and of the track kernel's
pitch and AMP sections in Monomachine OS 1.32B. It was written from the
DSP56300 code and checked sample for sample against that code running in an
emulator. This repository holds no firmware code. The tables are built from
formulas at start-up. The pink filter's seven coefficients are constants in
the source.

The bundle `Mono GND.clap` holds two plug-ins, **Mono GND SIN** and **Mono
GND NOIS**. Each is a monophonic synth, as a Monomachine track is. The third
GND machine, GND GND, is left out: its routine writes zeros.

Both machines are plain sound sources. SIN is a clean sine: an 8192-point
table with linear interpolation, at a quarter of full scale. NOIS is white,
pink (−3 dB per octave) or red (−6 dB per octave) noise, with a width
control. Any further character they have on the unit comes from the rest
of the track: the filter, distortion, AMP, SRR and delay. The AMP is in
these plug-ins; the filter, SRR and delay are separate Mono effect
plug-ins.

## Controls

Every plug-in has the machine's knobs (page "Machine") and the AMP page's
knobs (page "Amp"). All of them are 0–127 like the unit's knobs, can be
automated and modulated in Bitwig, and default to the unit's values.
Knobs a machine does not read are hidden parameters.

**Mono GND SIN** has one knob:

| Parameter | On the unit | What it does |
|-----------|-------------|--------------|
| Tune      | TUNE | The pitch, ±2 semitones (−200 to +197 cents). |

**Mono GND NOIS** (white, pink and red noise):

| Parameter    | On the unit | What it does |
|--------------|-------------|--------------|
| Stereo Width | ST   | How far apart the sides are. At 0 each side gets half of each channel. At 127 each side gets its own channel (and 1/256 of the other). |
| Red          | RED  | The colour. From 0 to 64 it fades from white (at a quarter of full scale) to pink (at full). From 64 to 127 it fades from pink to red (at full). The display shows the mix. |
| Stereo       | STON | At 64 and up, stereo. Below 64, both sides get the left side's mix, so at Width 0 that is the two channels' average. |

NOIS has no pitch, so its TUNE and the Amp page's Portamento do nothing.
The plug-in hides them. A note only opens the AMP envelope.

The Amp page, in both:

| Parameter | On the unit | What it does |
|-----------|-------------|--------------|
| Attack    | ATK  | The rise from the current level to full: 0.7 ms at 0, 152 ms at 64, 30 s at 127. |
| Hold      | HOLD | How long the level stays full, in beats of the host's tempo: Hold/32 beats (127 is about a bar). |
| Decay     | DEC  | The fall after the hold: −60 dB in 23 ms at 0, 1.7 s at 64, 112 s at 126. At 127 the level holds. |
| Release   | REL  | The fall after the note ends, with the same times. |
| Volume    | VOL  | The level, (VOL/128)². |
| Pan       | PAN  | Equal-power panning (−64 to +63 on the display). At the centre the left side is 0.11 dB lower than the right, as on the unit. |
| Portamento | PORT | SIN: the glide to a new note, in beats of the host's tempo, whatever the interval. 0 is instant, 32 about 1/30 beat, 64 a third of a beat, 96 three beats, 127 about 27 beats (2^(13 Portamento/128)/276 beats). |

The Monomachine's envelope has no sustain. The level falls with Decay even
while the note is held (set Decay to 127 to hold it), and a note's end
starts the Release from wherever the level is.

## How it behaves

This is what the firmware does; the plug-in keeps all of it.

- **SIN** reads the sine table (the one the whole kernel uses) with linear
  interpolation between neighbouring entries. Its step per sample is the
  kernel's frequency word × 0.1858. The step is ramped from the last block's
  step in 16 equal parts, so pitch changes and glides are smooth within a
  block. A note does not reset the phase, which runs on from the machine's
  assignment. A4 is 439.80 Hz (−0.78 cents), as in SWAVE PULS. The interpolation
  error is below the 24-bit output's last bit.
- **NOIS' white noise** gives two words per sample, left and right. Each
  word is the sum of the two before it, mod 2^24: a Fibonacci-style
  generator. At each 16-sample block the firmware stores the pair in the
  other order, which twists the sequence. The result has a flat spectrum
  (within 0.1 dB in a 47 s measurement) and no correlation from one sample
  to the next or between the sides. It is the same sequence every time the
  machine is assigned, and it repeats after 2^23 blocks (50 minutes).
- **Pink** is the white noise through a third-order filter (four input
  taps, three feedback taps), ×4. Measured from 31 Hz to 16 kHz its slope is
  −3.0 dB per octave.
- **Red** is the white noise through a leaky integrator: each sample adds
  1/64 of the noise and loses 839/2^23 of the level, with a time constant of
  0.23 s. Its slope is −6 dB per octave from about 1 Hz up. Its peaks reach
  full scale, where the DSP's 24-bit words clip them.
- **The start.** The red integrator starts from 0 when the machine is
  assigned and fills with its 0.23 s time constant. The plug-in runs NOIS's
  first 2048 blocks (0.74 s) at every reset, so its first note is like any
  other.
- **Notes.** Each note restarts the AMP envelope's attack from where its
  level is. In SIN, with Portamento at 0, the pitch reaches a new note in
  two 16-sample blocks. Above 0 it glides in a straight line (in pitch),
  covering the interval from the glide's start in the Portamento time. A
  note that comes mid-glide in the same direction keeps that start. A note
  in the other direction turns from where the pitch is, at the speed for
  the interval from the note it was heading to.
- **Knobs.** Parameter changes reach the machine the way the Monomachine's
  CPU sends a knob: every 384 samples the value moves a quarter of the way to
  where you set it. Modulation is added on top, unsmoothed, every 48
  samples, as the unit's LFOs are. Notes and the tempo are taken every 48
  samples too.
- **The AMP gains** move in a straight line across each 16-sample block.
  The AMP section's product is saturated at full scale (below).

## Differences from the hardware

- **The track filter.** On the unit the machine's output goes through the
  track's filter and distortion before the AMP section. The plug-in leaves
  them out ([Mono Filter Distortion](https://github.com/nerazin/mono-filter-distortion-clap)
  is that filter, as an effect). In their place it uses the filter's plain
  level at its default settings, 0.4455 (−7.0 dB), without its inversion,
  as Mono FM+ and Mono SWAVE do. NOIS's left and right each get that level.
- **The rest of the track.** EQ, SRR, delay, the LFOs and the track's LEVEL
  are not in the plug-in. Bitwig's own modulators can take the LFOs' place.
- **Notes.** One voice: the newest note held sounds. Releasing it returns to
  the previous note still held, with a new trig; releasing the last ends the
  note. Velocity is ignored. This note handling is the plug-in's; the unit's
  own MIDI handling was not studied.
- **Start.** The plug-in starts as a track does when the machine is assigned:
  the machine's init routine runs and the AMP envelope is idle at 0. SIN's
  step then ramps up from 0 in its first block. On the unit it ramps from
  whatever the track's memory held. On a unit just switched on, the
  firmware's envelope starts with an attack of its own.
- **Other sample rates.** The machines run at 44.1 kHz, the unit's rate. At
  other rates the plug-in resamples its output (windowed sinc), so it is no
  longer bit-exact, but the pitch is the same. The resampler reads ahead, so
  the plug-in reports a latency: 0 samples at 44.1 kHz, 40 at 22.05 kHz, 44
  at 48 kHz and 87 at 96 kHz. Notes land on the unit's 48-sample control
  grid, so their timing has that much spread anyway.
- **Levels.** 0 dBFS in the plug-in is full scale after the AMP section. A
  held note peaks at −10.0 dBFS at Volume 64, for SIN and for white NOIS.
  From Volume 114 up the AMP section clips them at full scale, as the
  firmware's does (40% of a sine's samples are clipped at 127). Pink peaks
  at −0.1 dBFS at Volume 64. Red at Volume 64 has 10% of its samples
  clipped; it stays clean at 32 and below. On the unit the filter's level
  sets where this begins; with the filter at its defaults it is the same.

## Accuracy

The reference is Monomachine OS 1.32B running in the DSP56300 emulator of
[Monomodule](https://github.com/shnolk/monomodule), with the track's controls
and notes built by Monomodule's model of the Monomachine's CPU. It has not
been compared with a physical Monomachine, and the plug-ins have not been
opened in Bitwig as part of this work.

| Check | Result |
|-------|--------|
| Core vs. firmware, for each machine on a synth track: random notes on and off, knob moves through the CPU model (Portamento included), raw control words anywhere from 0 to $7FFFFF (also for the AMP page and the trig word), tempos from 30 to 300 BPM; 400,000 blocks each over 8 seeds; NOIS's red integrator at full scale after 8,656 of its blocks | every machine output sample (left and right) identical; every machine state word identical after every block; the AMP envelope's state, its gain ramps and its products with the firmware's own filter output identical in every block; the glide's state, the pitch index and the frequency word identical to the kernel's in every block |
| Built plug-in code vs. firmware, with the firmware's filter output replaced by the machine's output × 0.4455 as in the plug-in: knob moves on both pages (Portamento included) through the CPU model, modulation, overlapping notes, tempo changes (240,000 blocks per plug-in: about 23,900 moves, 2,390 of them modulation; about 6,030 notes on and 4,680 off) | every output sample identical |
| Tables vs. firmware memory | identical: the sine, the pitch table, the AMP section's attack, decay and portamento tables, and the pink filter's coefficients |

The tables: the sine is the boot code's recurrence, reproduced exactly; the
exponential pitch table is 2^(22 + k/2048) with a mask for its 416 entries
one LSB lower in the firmware; the AMP section's attack, decay and
portamento tables are Mono FM+'s formulas. The pink filter's seven
coefficients are written in the source as numbers.

`tests/test_core.c` pins this without the firmware. Its expected hashes were
produced by feeding the same stimulus into the firmware's machines and AMP
section, which the core matched from its own start.

## Build and install

```bash
./build_macos.sh
```

```bash
cp -R "build/Mono GND.clap" ~/Library/Audio/Plug-Ins/CLAP/
```

The build script needs Xcode's command-line tools.

## Tests

```bash
cc -std=c99 -O2 -Wno-unused-function tests/test_core.c -lm -o tests/test_core && tests/test_core
```

```bash
xcrun clang -std=c11 -O2 -Wno-unused-function -Wno-missing-field-initializers -I vendor/clap/include tests/clap_test.c -o tests/clap_test && tests/clap_test "build/Mono GND.clap"
```

`test_core` checks the tables and 12,000 blocks of each machine's output and
AMP output against the firmware-derived hashes. `clap_test` loads the built
bundle and checks:

- the two plug-ins, each with its fifteen parameters (the ones a machine
  does not read hidden, also on the remote-control pages);
- at 44.1 kHz, each plug-in's output equal to the core's, for notes on and
  off at and between control frames, with the machines' knobs away from
  their defaults and Portamento at 0 and at 50 (and that 50 glides in SIN);
- SIN at the core's frequency for A4 at 44.1 and 48 kHz, and 197 cents
  higher with Tune at 127;
- releasing the newer of two held notes returning to the other, and the
  release ending in silence;
- NOIS: no sample-to-sample or left/right correlation at Red 0 and full
  width, near-total correlation at Red 127, and the same signal on both
  sides (but for the centre pan) with Stereo off or Width 0;
- a note's onset no earlier than the reported latency and at most one
  control frame and one block later, at 22.05, 44.1, 48 and 96 kHz;
- state for both plug-ins (also another machine's state, a short and a bad
  state rejected) and the parameter text.

Both tests also pass with AddressSanitizer and UndefinedBehaviorSanitizer,
and as Intel builds under Rosetta. The bundle passes clap-trap's `validate`
(both plug-ins, at 44.1, 48 and 96 kHz) and `state --roundtrip` (which tests
the first plug-in, SIN).

## Source

- `source/mono_gnd.h`: the two machines and the kernel's pitch glide and
  AMP section (C99 and libm).
- `source/mono_gnd_clap.c`: the two CLAP plug-ins around it.
- `vendor/clap`: CLAP 1.2 headers (MIT).

## License

GPL-3.0-only for this project's code. The DSP is an independent
reimplementation of the Monomachine's GND machines and track kernel, derived
from analysis of Elektron's firmware; no firmware code is included.
Monomachine and Elektron are trademarks of Elektron Music Machines; this
project is not affiliated with Elektron.
