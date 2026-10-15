# SPDX-License-Identifier: GPL-3.0-or-later
#
# HamRadioTrainer 2026
# Neu aufgebaut von Jan DC6DX
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.

cmake_minimum_required(VERSION 3.21)

project(HamRadioTrainer2026
    VERSION 0.1.0
    DESCRIPTION "HamRadioTrainer 2026 - Lern- und Pruefungstrainer fuer den Amateurfunk"
    HOMEPAGE_URL "https://hamradiotrainer.org/"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Everything is relocatable: users may run the binary from the build tree.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Xml)
find_package(Qt6 6.5 QUIET OPTIONAL_COMPONENTS Test)

qt_standard_project_setup()

# Qt is LGPLv3; linking it into a GPLv3 application is compatible.
add_library(hrt_core STATIC
    src/core/Catalog.cpp
    src/core/CatalogXml.cpp
    src/core/CatalogValidator.cpp
    src/learning/Scheduler.cpp
    src/exam/PaperBuilder.cpp)

target_include_directories(hrt_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(hrt_core PUBLIC Qt6::Core Qt6::Xml)
target_compile_definitions(hrt_core PUBLIC
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
    HRT_VERSION_STRING="${PROJECT_VERSION}")

add_executable(HamRadioTrainer2026 WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/ui/MainWindow.cpp)

target_link_libraries(HamRadioTrainer2026 PRIVATE hrt_core Qt6::Widgets Qt6::Gui)

if(MSVC)
    target_compile_options(HamRadioTrainer2026 PRIVATE /W4 /permissive-)
    target_compile_definitions(HamRadioTrainer2026 PRIVATE
        UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(HamRadioTrainer2026 PRIVATE -Wall -Wextra)
endif()

# No console window on Windows in the GUI build.
if(WIN32)
    set_target_properties(HamRadioTrainer2026 PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

set_target_properties(HamRadioTrainer2026 PROPERTIES
    OUTPUT_NAME "HamRadioTrainer2026"
    MACOSX_BUNDLE_BUNDLE_NAME "HamRadioTrainer 2026"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.hamradiotrainer.HamRadioTrainer2026"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

# ---------------------------------------------------------------------------
# Command line companion for catalog maintenance and CI
# ---------------------------------------------------------------------------

add_executable(hrt-catalog-tool tools/catalog-tool.cpp)
target_link_libraries(hrt-catalog-tool PRIVATE hrt_core)
set_target_properties(hrt-catalog-tool PROPERTIES OUTPUT_NAME "hrt-catalog-tool")
install(TARGETS hrt-catalog-tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Imports the Bundesnetzagentur question catalog (DL-DE-BY-2.0). It is only
# needed to regenerate the catalogs, never at runtime.
add_executable(hrt-catalog-import tools/catalog-import.cpp)
target_link_libraries(hrt-catalog-import PRIVATE hrt_core)

if(Qt6Test_FOUND)
    enable_testing()
    add_executable(tst_catalog tests/tst_catalog.cpp)
    target_link_libraries(tst_catalog PRIVATE hrt_core Qt6::Test)
    target_compile_definitions(tst_catalog PRIVATE
        HRT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME catalog COMMAND tst_catalog)

    add_executable(tst_scheduler tests/tst_scheduler.cpp)
    target_link_libraries(tst_scheduler PRIVATE hrt_core Qt6::Test)
    target_compile_definitions(tst_scheduler PRIVATE
        HRT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME scheduler COMMAND tst_scheduler)
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

include(GNUInstallDirs)

install(TARGETS HamRadioTrainer2026
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY data/catalogs
    DESTINATION ${CMAKE_INSTALL_DATADIR}/hamradiotrainer2026
    FILES_MATCHING PATTERN "*.xml")

install(FILES schema/catalog.xsd
    DESTINATION ${CMAKE_INSTALL_DATADIR}/hamradiotrainer2026/schema)

install(FILES LICENSE README.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hamradiotrainer2026)

# Windows: collect Qt runtime next to the executable so the bundle is portable.
if(WIN32)
    qt_generate_deploy_app_script(
        TARGET HamRadioTrainer2026
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        NO_TRANSLATIONS)
    install(SCRIPT ${deploy_script})
endif()