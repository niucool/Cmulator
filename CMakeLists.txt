cmake_minimum_required(VERSION 3.20)
project(Cmulator VERSION 0.4.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/FS)
endif()
option(USE_VCPKG "Use vcpkg" ON)
include(FetchContent)

# ── Unicorn ──
if(NOT USE_VCPKG OR NOT unicorn_FOUND)
    FetchContent_Declare(unicorn GIT_REPOSITORY https://github.com/unicorn-engine/unicorn.git GIT_TAG 2.1.4)
    set(UNICORN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(UNICORN_ARCH "x86" CACHE STRING "" FORCE)
    FetchContent_MakeAvailable(unicorn)
    set(unicorn_target unicorn)
else()
    set(unicorn_target unicorn::unicorn)
endif()

# ── Zydis ──
if(NOT USE_VCPKG OR NOT Zydis_FOUND)
    FetchContent_Declare(zydis GIT_REPOSITORY https://github.com/zyantific/zydis.git GIT_TAG v4.1.1)
    set(ZYDIS_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
    set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ZYDIS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(zydis)
    set(Zydis_target Zydis)
else()
    set(Zydis_target Zydis::Zydis)
endif()

# ── QuickJS ──
if(NOT USE_VCPKG OR NOT qjs_FOUND)
    FetchContent_Declare(quickjs GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git GIT_TAG v0.14.0)
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(quickjs)
    if(TARGET qjs)
        set(qjs_target qjs)
    else()
        message(FATAL_ERROR "Cannot find qjs target")
    endif()
else()
    set(qjs_target qjs)
endif()

# ── xxHash ──
if(NOT USE_VCPKG OR NOT xxhash_FOUND)
    FetchContent_Declare(xxhash GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git GIT_TAG v0.8.3 SOURCE_SUBDIR cmake_unofficial)
    set(XXHASH_BUILD_XXHSUM OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(xxhash)
    set(xxhash_target xxhash)
else()
    set(xxhash_target xxHash::xxhash)
endif()

# ── pe-parse ──
if(NOT USE_VCPKG OR NOT pe-parse_FOUND)
    FetchContent_Declare(peparse GIT_REPOSITORY https://github.com/trailofbits/pe-parse.git GIT_TAG master GIT_SHALLOW TRUE)
    set(PEPARSE_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(PEPARSE_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_COMMAND_LINE_TOOLS OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(peparse)
    set(peparse_target pe-parse)
else()
    set(peparse_target pe-parse::pe-parse)
endif()

# ── nlohmann_json ──
if(NOT USE_VCPKG OR NOT nlohmann_json_FOUND)
    FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
    FetchContent_MakeAvailable(nlohmann_json)
endif()
set(nlohmann_target nlohmann_json::nlohmann_json)

# ── plog ──
if(NOT USE_VCPKG OR NOT plog_FOUND)
    FetchContent_Declare(plog GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git GIT_TAG 1.1.11)
    set(PLOG_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(PLOG_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(plog)
endif()
set(plog_target plog::plog)

# ── cmulator library ──
set(LIB_SRC
    Core/Segments.cpp Core/Utils.cpp Core/Globals.cpp Core/Emu.cpp
    Core/PE_Loader.cpp Core/NativeHooks.cpp Core/js_engine.cpp
    Core/process/ethreads.cpp Core/emu_init.cpp Core/PE/pe_image.cpp)
set(LIB_HDR Core/types.h Core/Globals.h Core/Struct.h Core/TEP_PEB.h
    Core/FnHook.h Core/Segments.h Core/Emu.h Core/Utils.h
    Core/PE_Loader.h Core/NativeHooks.h Core/MemManager.h
    Core/js_engine.h Core/process/ethreads.h Core/PE/pe_image.h)

add_library(cmulator STATIC ${LIB_SRC} ${LIB_HDR})
target_include_directories(cmulator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(WIN32)
    target_compile_definitions(cmulator PUBLIC _PEPARSE_WINDOWS_CONFLICTS)
endif()
target_link_libraries(cmulator PUBLIC
    ${unicorn_target} ${Zydis_target} ${qjs_target} ${xxhash_target}
    ${peparse_target} ${nlohmann_target} ${plog_target})
if(WIN32)
    target_link_libraries(cmulator PRIVATE ws2_32)
endif()

# ── cmulator executable ──
add_executable(cmulator_exe Cmulator.cpp)
target_link_libraries(cmulator_exe PRIVATE cmulator qjs-libc)
set_target_properties(cmulator_exe PROPERTIES OUTPUT_NAME cmulator)
if(WIN32)
    target_link_libraries(cmulator_exe PRIVATE ws2_32)
endif()

# ── test ──
add_executable(test_deps test_deps.cpp)
target_link_libraries(test_deps PRIVATE cmulator)
if(WIN32)
    target_link_libraries(test_deps PRIVATE ws2_32)
endif()
