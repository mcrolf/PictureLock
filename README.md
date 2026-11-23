<p align="center">
  <img src="assets/logo/picturelock-icon.svg" alt="PictureLock logo: a padlock whose body is a film frame with a play button" width="160">
</p>

# PictureLock

PictureLock is a video player plug-in for Ableton Live, built for film scoring and sound post-production. It shows a movie in sync with Live's transport, the way the video tracks in Pro Tools and Logic do: frame-accurate, locked to elapsed time, with a timecode burn-in and a separate window you can put on a second display.

It is a VST3 and Audio Unit effect. It passes audio through untouched and only reads the song position from Live.

> Status: early prototype (0.1.0). macOS builds are tested locally; Windows builds come from CI and have not been tested in Live yet.

## How it stays in sync

- Each audio block, the plug-in reads Live's song position in samples and converts it to seconds since song position 0. Between blocks the picture is advanced with the wall clock so it moves smoothly at the display's refresh rate.
- The picture is never placed on Live's arrangement, so Live never warps or stretches it. Tempo changes move your bar lines relative to the picture, exactly as they do in Pro Tools or Logic.
- The picture start is stored as timecode, not as a bar position. Changing tempo before the picture start does not move the picture.
- Frames are decoded with FFmpeg and seeks are frame accurate (a seek decodes forward from the previous keyframe to the exact frame).

## Using it in Live

1. Put PictureLock on any track. The Master track is a good choice because it always processes audio.
2. Click **Open Movie...** or drop a movie onto the plug-in window. QuickTime (`.mov`), MP4, MXF, Matroska and AVI files with ProRes, DNxHD/DNxHR, H.264, HEVC, MPEG-2, MJPEG, VP9 and similar codecs are supported.
3. If the movie carries a start timecode (for example `01:00:00:00`), PictureLock uses it as the picture start and sets the session start to match, so the first frame lands at song position 0.
4. Set **Session start** to the timecode you want at song position 0. For example, `00:59:52:00` gives you eight seconds of pre-roll before a `01:00:00:00` picture start.
5. Click **Show Video Window** for a floating picture window. Double-click it to go full screen on the display it is on. Right-click it for Always on Top, Timecode Burn-in and size options. The window never takes keyboard focus, so the space bar still starts and stops Live.
6. To use the production audio as a guide track, click **Export Guide Audio...**. PictureLock writes a stereo WAV at Live's sample rate that starts at the first picture frame, and tells you the song position to place it at. **Turn Warp off on that clip** or Live will stretch it when the tempo changes.

All settings, including the movie path and the window position, are saved with the Live Set.

### Calibrating the sync offset

Audio leaves your interface a little after Live processes it, so the picture can appear slightly early. To calibrate, play a movie with a 2-pop or a sync flash, compare it against the beep, and adjust **Sync offset**. A positive offset delays the picture. Typical values are 5 to 30 ms, depending on your buffer size.

### Checking tempo behaviour

The plug-in shows the song time it receives from Live (`Song 1:23.456`). After adding tempo changes, compare that with Live's time ruler (the min:sec ruler under the arrangement). They should match. If they don't, please open an issue with your Live version.

## Installing

Download the installer for your platform from the Releases page.

- **macOS** (11 or later, Apple silicon and Intel): `PictureLock-<version>-macOS.pkg` installs the VST3 and AU plug-ins, and optionally a standalone app. Builds are not yet notarized; if macOS blocks the installer, right-click it and choose **Open**.
- **Windows** (64-bit): `PictureLock-<version>-Windows-Setup.exe` installs the VST3 plug-in to `C:\Program Files\Common Files\VST3`.

In Live, make sure **Use VST3 Plug-In System Folders** (and **Use Audio Units** on macOS) is on under Settings > Plug-Ins, then rescan.

## Building from source

Requirements: CMake 3.22+, a C++20 compiler (Xcode or Command Line Tools on macOS, Visual Studio 2022 on Windows), and `nasm` for FFmpeg's x86 assembly.

```sh
git clone --recurse-submodules <repo-url>
cd PictureLock
scripts/build-ffmpeg.sh                     # minimal static LGPL FFmpeg into .deps/ffmpeg-install
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
scripts/package-macos.sh build              # macOS: writes dist/PictureLock-<version>-macOS.pkg
```

On Windows, run `scripts/build-ffmpeg.sh` from an MSYS2 shell started inside a Visual Studio developer prompt, then build with CMake as usual. `.github/workflows/build.yml` shows the exact steps CI uses for both platforms.

To measure decode speed for one of your own movies:

```sh
PICTURELOCK_BENCHMARK_MOVIE=/path/to/movie.mov build/tests/PictureLockTests_artefacts/Release/PictureLockTests
```

## Project layout

| Path | Contents |
| --- | --- |
| `src/timecode` | Frame rates, SMPTE timecode, drop-frame labelling |
| `src/sync` | `TransportClock` (host song position) and `PicturePlacement` (song time to timecode and movie frame) |
| `src/video` | FFmpeg `VideoDecoder`, `FrameCache`, and the `PictureEngine` decode thread |
| `src/audio` | `GuideAudioExporter` |
| `src/plugin` | The audio processor, editor and saved settings |
| `src/ui` | Picture view, video window, settings panel |
| `tests` | Unit and integration tests, with small fixture movies |

## License

PictureLock is licensed under the GNU Affero General Public License v3.0 (see `LICENSE`), as required by the JUCE framework's open-source licence. FFmpeg is built with only LGPL components and linked statically; the full source and build script are in this repository, so you can relink against your own FFmpeg build.
