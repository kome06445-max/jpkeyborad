cmake_minimum_required(VERSION 3.16)
project(bsdjp VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Find Dependencies ---
find_package(PkgConfig REQUIRED)

pkg_check_modules(XCB REQUIRED xcb xcb-util xcb-keysyms)
pkg_check_modules(XCB_IMDKIT REQUIRED xcb-imdkit)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(PANGO REQUIRED pangocairo)

# --- Data directory ---
set(BSDJP_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/bsdjp" CACHE PATH "Data directory")
add_definitions(-DBSDJP_DATA_DIR="${BSDJP_DATA_DIR}")
add_definitions(-DBSDJP_SOURCE_DATA_DIR="${CMAKE_SOURCE_DIR}/data")

# --- Main executable ---
add_executable(bsdjp
    src/main.c
    src/xim_server.c
    src/engine.c
    src/romaji.c
    src/kana_kanji.c
    src/predict.c
    src/dict.c
    src/user_dict.c
    src/candidate_window.c
    src/util.c
)

find_path(X11_KEYSYM_DIR X11/keysym.h
    PATHS /usr/include /usr/local/include
)

target_include_directories(bsdjp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${XCB_INCLUDE_DIRS}
    ${XCB_IMDKIT_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${PANGO_INCLUDE_DIRS}
    ${X11_KEYSYM_DIR}
)

target_link_libraries(bsdjp PRIVATE
    ${XCB_LIBRARIES}
    ${XCB_IMDKIT_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${PANGO_LIBRARIES}
)

target_compile_options(bsdjp PRIVATE
    ${XCB_CFLAGS_OTHER}
    ${XCB_IMDKIT_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
    ${PANGO_CFLAGS_OTHER}
    -Wall -Wextra -Wpedantic
)

# --- Install ---
install(TARGETS bsdjp DESTINATION bin)
install(DIRECTORY data/ DESTINATION "${BSDJP_DATA_DIR}"
    FILES_MATCHING
    PATTERN "*.txt"
    PATTERN "SKK-JISYO.*"
)

# --- Tests ---
enable_testing()

add_executable(test_romaji test/test_romaji.c src/romaji.c src/util.c)
target_include_directories(test_romaji PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(test_romaji PRIVATE -Wall -Wextra)
target_compile_definitions(test_romaji PRIVATE BSDJP_SOURCE_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
add_test(NAME test_romaji COMMAND test_romaji)

add_executable(test_dict test/test_dict.c src/dict.c src/util.c)
target_include_directories(test_dict PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(test_dict PRIVATE -Wall -Wextra)
target_compile_definitions(test_dict PRIVATE BSDJP_SOURCE_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
add_test(NAME test_dict COMMAND test_dict)
