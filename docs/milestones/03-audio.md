# Milestone 3 — audio (in progress / first working pass)

Date: 2026-10-31

## Implemented

- `runtime/src/ymz280b.c` implements the YMZ280B's eight voices:
  - 4-bit ADPCM (the format DoDonPachi's sample ROM uses), 8-bit PCM and 16-bit PCM
  - start/stop/loop start/loop end addresses, key-on/off, loop enable
  - pitch (`fnum` + 1), total level, 16-step pan
  - stereo mixing at the chip's 88.2 kHz internal rate, resampled to the host rate
- Register interface at `0x300000`/`0x300002` (the high byte of the 16-bit write, matching
  MAME's `umask16(0x00ff)`).
- SDL2 front end opens a 44.1 kHz stereo `AUDIO_S16SYS` device and queues one video frame's
  worth of samples per frame; the queue is capped near real time.
- Headless `--wav PATH` renders the chip's output from frame 3001 onward for verification.

## Verified

Command:

```powershell
build\cmake\bin\ddonpach_headless.exe --rom ...ddonpach.zip --frames 8000 --wav build\ddp_audio.wav --quiet
```

WAV analysis (referencing the file, not shipped):

```text
frames 3,830,000 (44.1 kHz stereo)
peak 16167 (of 32767)
non-zero samples 2,421,596
```

The SDL front end logs:

```text
ddonpach: audio device 44100 Hz, 2 channels
ddonpach: audio queue started (766 frames)
```

A later runtime pass added `--audio-wav` to the SDL front end, recording the exact bytes
passed to `SDL_QueueAudio` rather than only the headless chip output. A 6,000-frame auto-start
run produced:

```text
frames 4,597,741 (104.26 s at 44.1 kHz stereo)
peak 10,047 of 32,767
non-zero samples 3,382,412
first non-zero sample at ~65.9 s, when coin/start is injected
queue 1,684..5,816 bytes and drained every frame
```

This confirms generation, queueing and device consumption. The earliest boot/attract screens
are silent by design; music/SFX begin when the game starts.

## Not yet done

- The YMZ280B's end-of-voice IRQ is not wired into the board's cause register yet; playback
  works without it, but exact IRQ-driven restart behavior is a later accuracy pass.
- No listening comparison against MAME or physical speaker test has been recorded on Steam
  Deck hardware.
- Music/SFX balance and clipping need an audible pass once the Deck is available.
- The current source has no reproducible audio-generation defect; a silent run should include
  the `--audio-wav` capture and the `--stats` queue lines so the failure can be localized to
  generation, queueing or the host device.
