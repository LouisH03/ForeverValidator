cmake_minimum_required(VERSION 3.21)

project(ForeverValidator LANGUAGES CXX)

option(FOREVERVALIDATOR_BUILD_CLI "Build the ForeverValidator CLI"
       ${PROJECT_IS_TOP_LEVEL})
option(FOREVERVALIDATOR_BUILD_TESTS "Build ForeverValidator public tests" OFF)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32 AND (NOT MINGW OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
    message(FATAL_ERROR "Windows builds require MinGW-w64 GCC")
endif()

find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(ZLIB REQUIRED)

file(GLOB_RECURSE FOREVERVALIDATOR_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/format/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/simulation/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/validation/api/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/validation/evaluation/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/validation/planning/*.cpp")
file(GLOB_RECURSE FOREVERVALIDATOR_JSON_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/validation/serialization/*.cpp")
file(GLOB_RECURSE FOREVERVALIDATOR_NATIVE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/native/*.cpp")
file(GLOB_RECURSE FOREVERVALIDATOR_CLI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/cli/*.cpp")

function(forevervalidator_configure_target target)
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_compile_definitions(${target} PRIVATE
        NOMINMAX
        OPENSSL_SUPPRESS_DEPRECATED)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wno-deprecated-declarations
        -ffp-contract=off
        -frandom-seed=forevervalidator
        "-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
endfunction()

add_library(forevervalidator_core STATIC ${FOREVERVALIDATOR_CORE_SOURCES})
add_library(ForeverValidator::core ALIAS forevervalidator_core)
target_include_directories(forevervalidator_core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(forevervalidator_core
    PUBLIC OpenSSL::Crypto ZLIB::ZLIB)
forevervalidator_configure_target(forevervalidator_core)

add_library(forevervalidator_json STATIC ${FOREVERVALIDATOR_JSON_SOURCES})
add_library(ForeverValidator::json ALIAS forevervalidator_json)
target_include_directories(forevervalidator_json
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(forevervalidator_json PUBLIC forevervalidator_core)
forevervalidator_configure_target(forevervalidator_json)

add_library(forevervalidator_native STATIC ${FOREVERVALIDATOR_NATIVE_SOURCES})
add_library(ForeverValidator::native ALIAS forevervalidator_native)
target_include_directories(forevervalidator_native
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(forevervalidator_native PUBLIC forevervalidator_core)
forevervalidator_configure_target(forevervalidator_native)

if(FOREVERVALIDATOR_BUILD_CLI)
    add_executable(forevervalidator ${FOREVERVALIDATOR_CLI_SOURCES})
    target_include_directories(forevervalidator PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(forevervalidator PRIVATE
        forevervalidator_native
        forevervalidator_json
        forevervalidator_core)
    if(WIN32)
        target_link_libraries(forevervalidator PRIVATE
            ws2_32
            gdi32
            crypt32)
    endif()
    if(MINGW)
        target_link_options(forevervalidator PRIVATE
            -static
            -Wl,--no-insert-timestamp)
    endif()
    forevervalidator_configure_target(forevervalidator)
endif()

if(FOREVERVALIDATOR_BUILD_TESTS)
    enable_testing()
    add_executable(forevervalidator-render-scene-tests
        tests/render_scene_tests.cpp)
    target_include_directories(forevervalidator-render-scene-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(forevervalidator-render-scene-tests PRIVATE
        forevervalidator_core)
    forevervalidator_configure_target(forevervalidator-render-scene-tests)
    add_test(NAME forevervalidator-render-scene
             COMMAND forevervalidator-render-scene-tests)
endif()
