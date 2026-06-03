# 1. Source Collection
file(GLOB_RECURSE LIB_SOURCES
    "src/*.cpp"
    "src/*.hpp"
)

# 2. Define the library
add_library(atomic_lib STATIC ${LIB_SOURCES})

# 3. Ensure Vulkan is visible
find_package(Vulkan REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FONTCONFIG REQUIRED fontconfig)

find_package(Freetype REQUIRED)

target_include_directories(atomic_lib PRIVATE
    ${FONTCONFIG_INCLUDE_DIRS}
)

target_link_libraries(atomic_lib PRIVATE
    ${FONTCONFIG_LIBRARIES}
    Freetype::Freetype
)

# 4. Link everything
target_link_libraries(atomic_lib
    PUBLIC
        SDL3::SDL3-static
        Vulkan::Vulkan
)

# 5. Direct Path Mapping
# FetchContent defines ${targetname_SOURCE_DIR} automatically.
# This points directly to where the headers live, bypassing the NOTFOUND issue.
target_include_directories(atomic_lib
    BEFORE PUBLIC
    "${sdl3_SOURCE_DIR}/include"
    "src"
)
