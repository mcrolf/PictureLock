#!/usr/bin/env bash
# Builds a minimal, static, LGPL-only FFmpeg containing just the demuxers and
# decoders PictureLock needs. On macOS it produces a universal (arm64 + x86_64)
# build; on Windows (MSYS2 + MSVC toolchain) it produces an x64 build.
#
# Usage: scripts/build-ffmpeg.sh [install-prefix]
# Default prefix: .deps/ffmpeg-install
set -euo pipefail

FFMPEG_VERSION="${FFMPEG_VERSION:-9.1}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$ROOT/.deps"
PREFIX="${1:-$DEPS/ffmpeg-install}"
SRC="$DEPS/ffmpeg-$FFMPEG_VERSION"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

mkdir -p "$DEPS"
if [ ! -d "$SRC" ]; then
  curl -fsSL "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz" -o "$DEPS/ffmpeg.tar.xz"
  tar -xf "$DEPS/ffmpeg.tar.xz" -C "$DEPS"
fi

COMMON_FLAGS=(
  --disable-everything --disable-programs --disable-doc --disable-network
  --disable-autodetect --disable-debug --disable-shared --enable-static
  --enable-pic --enable-pthreads
  --disable-avdevice --disable-avfilter
  --enable-avcodec --enable-avformat --enable-swscale --enable-swresample
  --enable-protocol=file
  --enable-demuxer=mov,matroska,avi,mxf,mpegts,mpegps,m4v,h264,hevc,wav,aiff,mp3,aac
  --enable-decoder=h264,hevc,prores,dnxhd,mjpeg,mpeg2video,mpeg4,mpeg1video,vp8,vp9,dvvideo,png,qtrle,rawvideo,v210,v410,cfhd,hap
  --enable-decoder=aac,ac3,eac3,mp3,alac,flac,opus,vorbis,pcm_s16le,pcm_s16be,pcm_s24le,pcm_s24be,pcm_s32le,pcm_s32be,pcm_f32le,pcm_f32be,pcm_s8,pcm_u8
  --enable-parser=h264,hevc,mpeg4video,mpegvideo,mjpeg,vp8,vp9,aac,ac3,mpegaudio,dnxhd,png,opus,vorbis,flac
  --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb
  --enable-zlib
)

build_arch() {
  local arch="$1" out="$2"; shift 2
  local work="$DEPS/ffmpeg-build-$arch"
  rm -rf "$work"; mkdir -p "$work"
  ( cd "$work" && "$SRC/configure" --prefix="$out" "${COMMON_FLAGS[@]}" "$@" >/dev/null && make -j"$JOBS" >/dev/null && make install >/dev/null )
}

case "$(uname -s)" in
  Darwin)
    MIN_MACOS="11.0"
    for arch in arm64 x86_64; do
      flags="-arch $arch -mmacosx-version-min=$MIN_MACOS"
      extra=(--arch="$arch" --target-os=darwin --extra-cflags="$flags" --extra-ldflags="$flags"
             --enable-videotoolbox --enable-hwaccel=h264_videotoolbox,hevc_videotoolbox,prores_videotoolbox
             --enable-audiotoolbox)
      [ "$arch" != "$(uname -m)" ] && extra+=(--enable-cross-compile)
      echo "Building FFmpeg $FFMPEG_VERSION for $arch"
      build_arch "$arch" "$DEPS/ffmpeg-$arch" "${extra[@]}"
    done
    rm -rf "$PREFIX"; mkdir -p "$PREFIX/lib"
    cp -R "$DEPS/ffmpeg-arm64/include" "$PREFIX/include"
    for lib in "$DEPS"/ffmpeg-arm64/lib/*.a; do
      name="$(basename "$lib")"
      lipo -create "$lib" "$DEPS/ffmpeg-x86_64/lib/$name" -output "$PREFIX/lib/$name"
    done
    ;;
  MINGW*|MSYS*)
    echo "Building FFmpeg $FFMPEG_VERSION for Windows x64 (MSVC)"
    # /MT matches the project's static MSVC runtime. MSVC has no pthreads, so use native
    # Win32 threads (flags given later override COMMON_FLAGS).
    build_arch x86_64 "$PREFIX" --toolchain=msvc --arch=x86_64 --target-os=win64 \
      --extra-cflags=-MT --disable-pthreads --enable-w32threads --disable-pic \
      --enable-d3d11va --enable-hwaccel=h264_d3d11va,hevc_d3d11va --disable-zlib
    # FFmpeg's msvc toolchain may emit libavcodec.a (an MSVC static lib despite the suffix).
    # Provide a libavcodec.lib copy, which CMake's find_library accepts on MSVC.
    for lib in "$PREFIX"/lib/lib*.a; do
      [ -e "$lib" ] && cp "$lib" "${lib%.a}.lib"
    done
    ;;
  *) echo "Unsupported platform: $(uname -s)" >&2; exit 1 ;;
esac

echo "FFmpeg installed to $PREFIX"
