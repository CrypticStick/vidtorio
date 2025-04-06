cmake_minimum_required(VERSION 3.10)
project(display_gen_wasm C)

include(ExternalProject)

# Using C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

# Enable common warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Set optimization flags
add_compile_options("$<IF:$<CONFIG:Debug>,-O0,-O3>")

# FFmpeg configuration
# (see https://jeromewu.github.io/build-ffmpeg-webassembly-version-part-2-compile-with-emscripten/ for details)
set(FFMPEG_C_FLAGS "-pthread")
set(FFMPEG_LD_FLAGS "${CFLAGS} -sINITIAL_MEMORY=33554432") # 32 MB
set(FFMPEG_CONFIGURE_ARGS
    --prefix=${CMAKE_BINARY_DIR}/FFmpeg
    --target-os=none        # use none to prevent any os specific configurations
    --arch=x86_32           # use x86_32 to achieve minimal architectural optimization
    --enable-cross-compile  # enable cross compile
    --disable-x86asm        # disable x86 asm
    --disable-inline-asm    # disable inline asm
    --disable-stripping     # disable stripping
    --disable-programs      # disable programs build (incl. ffplay, ffprobe & ffmpeg)
    --disable-doc           # disable doc

    --disable-shared
    --enable-static

    --extra-cflags=${FFMPEG_C_FLAGS}
    --extra-cxxflags=${FFMPEG_C_FLAGS}
    --extra-ldflags=${FFMPEG_LD_FLAGS}

    --host-cc=$ENV{EMSDK}/upstream/bin/clang
    --nm="$ENV{EMSDK}/upstream/bin/llvm-nm -g"
    --as=$ENV{EMSDK}/upstream/bin/llvm-as
    --ar=emar
    --ranlib=emranlib
    --cc=emcc
    --cxx=em++
    --objcc=emcc
    --dep-cc=emcc
)

# Libraries we are building from FFmpeg
set(FFMPEG_LIBRARIES
    ${CMAKE_BINARY_DIR}/FFmpeg/lib/libavcodec.a
    ${CMAKE_BINARY_DIR}/FFmpeg/lib/libavformat.a
    ${CMAKE_BINARY_DIR}/FFmpeg/lib/libavutil.a
)

# Add and build FFmpeg libraries
ExternalProject_Add(
    FFmpeg
    PREFIX ${CMAKE_BINARY_DIR}/FFmpeg
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/FFmpeg
    CONFIGURE_COMMAND emconfigure ${CMAKE_SOURCE_DIR}/FFmpeg/configure ${FFMPEG_CONFIGURE_ARGS}
    BUILD_COMMAND emmake make -j
    INSTALL_COMMAND emmake make install
    BUILD_BYPRODUCTS ${FFMPEG_LIBRARIES}
)

# Add all source files and headers to the program
file(GLOB SOURCES "wasm/src/*.c")
add_executable(display_gen ${SOURCES})
target_include_directories(display_gen PRIVATE "wasm/include")

# FFmpeg include directories
target_include_directories(display_gen PRIVATE "${CMAKE_BINARY_DIR}/FFmpeg/include")

# Add Emscripten ports
string(APPEND EMSDK_PORTS
    " --use-port=sdl2"
    " --use-port=sdl2_image:formats=png"
    " --use-port=zlib"
)

# Add Emscripten link flags
string(APPEND EMSDK_LD_FLAGS
    " -o display_generator.js"
    " -sINITIAL_MEMORY=100663296"
    " -sEXPORTED_FUNCTIONS=['_main','_Process_Image','_Process_Init']"
    " -sEXPORTED_RUNTIME_METHODS=['addFunction','UTF8ToString']"
    " -sENVIRONMENT=web,worker"
    " -sMODULARIZE=1"
    " -sEXPORT_ES6=1"
    " -sWASM=1"
    " -pthread"
    " --embed-file ${CMAKE_SOURCE_DIR}/assets@/assets"
)

# Set compile/link options
set_target_properties(display_gen PROPERTIES COMPILE_FLAGS "-pthread ${EMSDK_PORTS}")
set_target_properties(display_gen PROPERTIES LINK_FLAGS "${EMSDK_PORTS} ${EMSDK_LD_FLAGS}")

# Optional debug settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_property(TARGET display_gen APPEND_STRING PROPERTY LINK_FLAGS " -gsource-map")
endif()

# Add FFmpeg dependency
add_dependencies(display_gen FFmpeg)
target_link_libraries(display_gen PRIVATE ${FFMPEG_LIBRARIES})
