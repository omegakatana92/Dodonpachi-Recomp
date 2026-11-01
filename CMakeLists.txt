cmake_minimum_required(VERSION 3.20)

project(DoDonPachiNative VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(DDP_BUILD_HEADLESS "Build the deterministic headless frame runner" ON)
option(DDP_BUILD_SDL2 "Build the SDL2 front end when SDL2 is available" ON)
option(DDP_FIXTURE "Build the ROM-free CI fixture instead of a translated game" OFF)
set(DDP_GENERATED_DIR "${CMAKE_SOURCE_DIR}/build/generated" CACHE PATH
    "Directory holding program.h, program_NN.c and entries.c produced by tools/recompile.py")
if(DDP_FIXTURE)
    set(DDP_GENERATED_DIR "${CMAKE_SOURCE_DIR}/tests/fixture/generated")
endif()
set(DDP_ROM_FILE "${CMAKE_SOURCE_DIR}/rom/ddonpach.zip" CACHE FILEPATH
    "The player's ddonpach ROM set, used by the CTest boot test")

file(GLOB DDP_GENERATED_SOURCES CONFIGURE_DEPENDS
     "${DDP_GENERATED_DIR}/program_*.c"
     "${DDP_GENERATED_DIR}/entries.c")

if(NOT DDP_GENERATED_SOURCES)
    message(FATAL_ERROR
        "No translated program in ${DDP_GENERATED_DIR}.\n"
        "Run: python tools/recompile.py <path-to-ddonpach.zip>")
endif()

add_library(ddp_runtime STATIC
    runtime/src/m68000.c
    runtime/src/machine.c
    runtime/src/program.c
    runtime/src/rom.c
    runtime/src/video.c
    runtime/src/eeprom.c
    runtime/src/ymz280b.c
    runtime/third_party/miniz/miniz.c
    ${DDP_GENERATED_SOURCES}
)

target_include_directories(ddp_runtime
    PUBLIC
        ${CMAKE_SOURCE_DIR}/runtime/include
        ${DDP_GENERATED_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/runtime/third_party/miniz
)

target_compile_definitions(ddp_runtime PUBLIC _CRT_SECURE_NO_WARNINGS)

if(MSVC)
    target_compile_options(ddp_runtime PRIVATE /W3)
else()
    target_compile_options(ddp_runtime PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

set(DDP_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")

function(ddp_set_output target)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${DDP_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DDP_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DDP_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${DDP_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${DDP_OUTPUT_DIR}")
endfunction()

if(DDP_FIXTURE)
    add_executable(ddonpach_fixture tests/fixture/main.c)
    target_link_libraries(ddonpach_fixture PRIVATE ddp_runtime)
    ddp_set_output(ddonpach_fixture)
elseif(DDP_BUILD_HEADLESS)
    add_executable(ddonpach_headless ports/headless/main.c)
    target_link_libraries(ddonpach_headless PRIVATE ddp_runtime)
    ddp_set_output(ddonpach_headless)
endif()

if(DDP_BUILD_SDL2)
    find_package(SDL2 CONFIG QUIET)
    if(SDL2_FOUND)
        add_executable(DoDonPachi ports/sdl2/main.c)
        target_link_libraries(DoDonPachi PRIVATE ddp_runtime SDL2::SDL2)
        target_compile_definitions(DoDonPachi PRIVATE SDL_MAIN_HANDLED)
        if(WIN32)
            set_target_properties(DoDonPachi PROPERTIES OUTPUT_NAME "DoDonPachi-Windows-x64")
            add_custom_command(TARGET DoDonPachi POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:DoDonPachi>")
        endif()
        ddp_set_output(DoDonPachi)
    else()
        message(STATUS "SDL2 not found: the windowed front end is not built")
    endif()
endif()

if(TARGET DoDonPachi)
    include(GNUInstallDirs)
    install(TARGETS DoDonPachi RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(WIN32 AND SDL2_FOUND)
        install(FILES "$<TARGET_FILE:SDL2::SDL2>" DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
    install(DIRECTORY LICENSES/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/LICENSES)
endif()

include(CTest)
if(BUILD_TESTING AND DDP_FIXTURE)
    add_test(NAME ddp_runtime_fixture COMMAND ddonpach_fixture)
elseif(BUILD_TESTING AND DDP_BUILD_HEADLESS AND EXISTS "${DDP_ROM_FILE}")
    add_test(NAME ddonpach_boot_60
             COMMAND ddonpach_headless --rom "${DDP_ROM_FILE}" --frames 60 --quiet)
endif()
