cmake_minimum_required(VERSION 3.20)

project(PinAppBM VERSION 0.1.0 LANGUAGES CXX RC)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(resources/app.manifest.in "${CMAKE_CURRENT_BINARY_DIR}/generated/app.manifest" @ONLY)
configure_file(resources/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc" @ONLY)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "Pin App BM can only be built for Windows.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Pin App BM MVP supports only 64-bit Windows builds.")
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(pin_app_core INTERFACE)
target_include_directories(pin_app_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(pin_app_icon_generator tools/generate_icon.cpp)
set(PIN_APP_ICON "${CMAKE_CURRENT_BINARY_DIR}/generated/app.ico")
add_custom_command(
    OUTPUT "${PIN_APP_ICON}"
    COMMAND pin_app_icon_generator "${PIN_APP_ICON}"
    DEPENDS pin_app_icon_generator
    VERBATIM
)
add_custom_target(pin_app_icon DEPENDS "${PIN_APP_ICON}")

add_executable(PinAppBM WIN32
    src/app.cpp
    src/app.h
    src/config.cpp
    src/config.h
    src/icon_factory.cpp
    src/icon_factory.h
    src/localization.cpp
    src/localization.h
    src/logger.cpp
    src/logger.h
    src/main.cpp
    src/pin_manager.cpp
    src/pin_manager.h
    src/pinned_order.h
    src/selection_overlay.cpp
    src/selection_overlay.h
    src/tray_activation.h
    src/tray_icon.cpp
    src/tray_icon.h
    src/window_utils.cpp
    src/window_utils.h
    "${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc"
)
add_dependencies(PinAppBM pin_app_icon)

target_link_libraries(PinAppBM PRIVATE
    pin_app_core
    advapi32
    dwmapi
    gdi32
    ole32
    shell32
    user32
    wtsapi32
)

target_compile_definitions(PinAppBM PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    PIN_APP_VERSION=L"${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(PinAppBM PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_compile_options(pin_app_icon_generator PRIVATE /W4 /permissive- /utf-8 /EHsc)
    # The application manifest is embedded as resource #1 by app.rc.
    target_link_options(PinAppBM PRIVATE /MANIFEST:NO)
endif()

install(TARGETS PinAppBM RUNTIME DESTINATION .)

include(CTest)
if(BUILD_TESTING)
    add_executable(pin_app_core_tests tests/pinned_order_tests.cpp)
    target_link_libraries(pin_app_core_tests PRIVATE pin_app_core)
    add_executable(pin_app_config_tests tests/config_tests.cpp src/config.cpp)
    target_link_libraries(pin_app_config_tests PRIVATE pin_app_core advapi32 ole32 shell32)
    target_compile_definitions(pin_app_config_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_executable(pin_manager_integration_tests
        tests/pin_manager_integration_tests.cpp
        src/pin_manager.cpp
        src/window_utils.cpp
    )
    target_link_libraries(pin_manager_integration_tests PRIVATE pin_app_core dwmapi user32)
    target_compile_definitions(pin_manager_integration_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(pin_app_core_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
        target_compile_options(pin_app_config_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
        target_compile_options(pin_manager_integration_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    endif()
    add_test(NAME pin_app_core_tests COMMAND pin_app_core_tests)
    add_test(NAME pin_app_config_tests COMMAND pin_app_config_tests)
    add_test(NAME pin_manager_integration_tests COMMAND pin_manager_integration_tests)
endif()
