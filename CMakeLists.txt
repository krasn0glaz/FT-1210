cmake_minimum_required(VERSION 3.16)
project(ft1210 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(OPENMPT REQUIRED libopenmpt)

add_executable(ft1210
    src/audio.c
    src/deck.c
    src/main.c
    src/ui.c
)

target_include_directories(ft1210 PRIVATE
    include
    ${SDL2_INCLUDE_DIRS}
    ${OPENMPT_INCLUDE_DIRS}
)

target_compile_options(ft1210 PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    ${SDL2_CFLAGS_OTHER}
    ${OPENMPT_CFLAGS_OTHER}
)

target_link_libraries(ft1210 PRIVATE
    m
    ${SDL2_LIBRARIES}
    ${OPENMPT_LIBRARIES}
)

target_link_directories(ft1210 PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${OPENMPT_LIBRARY_DIRS}
)

install(TARGETS ft1210 RUNTIME DESTINATION bin)
install(FILES src/ft1210.desktop DESTINATION share/applications)
install(FILES src/ft1210.svg DESTINATION share/icons/hicolor/scalable/apps)
