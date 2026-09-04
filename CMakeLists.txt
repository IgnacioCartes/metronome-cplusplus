cmake_minimum_required(VERSION 3.20)
project(WindowsMetronome LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/window.cpp
    src/window.h
    src/audio_engine.cpp
    src/audio_engine.h
    src/wav_loader.cpp
    src/wav_loader.h
    src/custom_dialogs.cpp
    src/custom_dialogs.h
    src/resource.h
    src/resources.rc
)

# Win32 GUI executable (no console window)
add_executable(metronome WIN32 ${SOURCES})

target_include_directories(metronome PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
)

# System libraries
target_link_libraries(metronome PRIVATE
    user32
    gdi32
    ole32
    comdlg32
    winmm
)

# Binary size & runtime optimizations
if(MSVC)
    # Size optimizations, disable RTTI and exceptions for minimal footprint
    target_compile_options(metronome PRIVATE
        $<$<CONFIG:Release>:
            /O1              # Favor small code
            /Os              # Favor size over speed
            /GL              # Whole program optimization
            /GR-             # Disable RTTI
            /EHs-c-          # Disable C++ exceptions
            /GS-             # Disable buffer security checks for size (optional, can omit if standard)
            /W4
        >
    )
    target_link_options(metronome PRIVATE
        $<$<CONFIG:Release>:
            /LTCG            # Link-time code generation
            /OPT:REF         # Eliminate unused functions
            /OPT:ICF         # Identical COMDAT folding
        >
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # MinGW GCC / Clang size optimizations
    target_compile_options(metronome PRIVATE
        $<$<CONFIG:Release>:
            -Os              # Optimize for size
            -flto            # Link-time optimization
            -fno-rtti        # Disable RTTI
            -fno-exceptions  # Disable C++ exceptions
            -Wall
            -Wextra
        >
    )
    target_link_options(metronome PRIVATE
        $<$<CONFIG:Release>:
            -s                           # Strip all symbol information
            -flto                        # Link-time optimization
            -static                      # Static link runtime libraries to remove external DLL dependencies
            -static-libgcc
            -static-libstdc++
            -Wl,--gc-sections
        >
        -mwindows                        # Win32 GUI subsystem
    )

    # Resource compiler flags for windres
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I \"${CMAKE_CURRENT_SOURCE_DIR}\" -I \"${CMAKE_CURRENT_SOURCE_DIR}/src\"")
endif()
