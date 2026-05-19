cmake_minimum_required(VERSION 3.15)
project(UIVision VERSION 0.1)

include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if(UNIX AND NOT APPLE)
  message(STATUS "//atomic//: Linux detected: Fetching SDL3")
 FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG main
  GIT_PROGRESS TRUE            # Enables --progress flag in git
  USES_TERMINAL_DOWNLOAD TRUE  # Pipes the git output directly to your terminal
)

  # SDL3 uses these specific names to control the build type
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  # Standard feature toggles
  set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
  set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
  set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  # Prevents SDL3 from trying to install to /usr/local during your build
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(SDL3)
endif()

FetchContent_Declare(
  freetype
  GIT_REPOSITORY https://github.com/freetype/freetype.git
  GIT_TAG master
  GIT_PROGRESS TRUE            # Enables --progress flag in git
  USES_TERMINAL_DOWNLOAD TRUE 
)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(freetype)




# Add subfolders
add_subdirectory(atomic_lib)

target_link_libraries(atomic_lib PUBLIC freetype)

find_program(GLSLANG_VALIDATOR "glslangValidator")

if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found! Cannot compile shaders.")
endif()

set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/atomic_lib/src/renderer/vulkan/shaders")
set(SHADER_OUT "${CMAKE_BINARY_DIR}/shaders")

file(MAKE_DIRECTORY ${SHADER_OUT})

set(SHADERS "ui.vert" "ui.frag")

foreach(SHADER ${SHADERS})
    add_custom_command(
        OUTPUT "${SHADER_OUT}/${SHADER}.spv"
        COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER_SRC}/${SHADER}" -o "${SHADER_OUT}/${SHADER}.spv"
        DEPENDS "${SHADER_SRC}/${SHADER}"
        COMMENT "Compiling shader: ${SHADER}"
    )
    list(APPEND SHADER_SPV_FILES "${SHADER_OUT}/${SHADER}.spv")
endforeach()

add_custom_target(AtomicShaders ALL DEPENDS ${SHADER_SPV_FILES})

find_package(Vulkan REQUIRED)
target_link_libraries(atomic_lib PUBLIC Vulkan::Vulkan)
add_subdirectory(sandbox)
