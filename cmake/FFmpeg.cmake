# Imports the static FFmpeg built by scripts/build-ffmpeg.sh as PictureLock::FFmpeg.
set(_ffmpeg_libs avformat avcodec swscale swresample avutil)
set(_ffmpeg_paths "")
foreach(_lib IN LISTS _ffmpeg_libs)
    find_library(FFMPEG_${_lib}_LIBRARY
        NAMES ${_lib} lib${_lib}
        PATHS "${PICTURELOCK_FFMPEG_DIR}/lib"
        NO_DEFAULT_PATH REQUIRED)
    list(APPEND _ffmpeg_paths "${FFMPEG_${_lib}_LIBRARY}")
endforeach()

add_library(PictureLock::FFmpeg INTERFACE IMPORTED)
target_include_directories(PictureLock::FFmpeg INTERFACE "${PICTURELOCK_FFMPEG_DIR}/include")
target_link_libraries(PictureLock::FFmpeg INTERFACE ${_ffmpeg_paths})

if(APPLE)
    target_link_libraries(PictureLock::FFmpeg INTERFACE
        z iconv "-framework VideoToolbox" "-framework CoreMedia" "-framework CoreVideo"
        "-framework CoreFoundation" "-framework CoreServices" "-framework AudioToolbox"
        "-framework Security")
elseif(WIN32)
    target_link_libraries(PictureLock::FFmpeg INTERFACE bcrypt ws2_32 secur32 strmiids mfuuid ole32 user32 d3d11 dxgi)
endif()
