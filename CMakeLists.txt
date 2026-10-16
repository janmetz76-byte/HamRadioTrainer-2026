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
find_package(Qt6 6.5 QUIET OPTIONAL_COMPONENTS Test Svg)

qt_standard_project_setup()

# Qt is LGPLv3; linking it into a GPLv3 application is compatible.
add_library(hrt_core STATIC
    src/core/Catalog.cpp
    src/core/CatalogXml.cpp
    src/core/CatalogValidator.cpp
    src/core/LatexFormatter.cpp
    src/core/AppIdentity.cpp
    src/core/CatalogLocator.cpp
    src/learning/Scheduler.cpp
    src/learning/ProgressStore.cpp
    src/learning/LearningStatistics.cpp
    src/learning/StudySession.cpp
    src/exam/PaperBuilder.cpp
    src/exam/ExamSession.cpp)

target_include_directories(hrt_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

include(GNUInstallDirs)

# Where the catalogs are installed, relative to the installation prefix.
# Runtime search (CatalogLocator) and the packaging rules read this one
# variable, so they cannot drift apart.
#
# It deliberately stays a relative path. A cache variable of type PATH would
# turn a relative value from the command line into an absolute one anchored at
# the build directory, and the catalogs would silently be installed into the
# build tree - and from there into the package.
set(HRT_CATALOG_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/hamradiotrainer-2026"
    CACHE STRING "Installationsort der Fragenkataloge, relativ zum Praefix")
if(IS_ABSOLUTE "${HRT_CATALOG_INSTALL_DIR}")
    message(FATAL_ERROR
        "HRT_CATALOG_INSTALL_DIR muss relativ zum Installationspraefix sein, "
        "kein absoluter Pfad: ${HRT_CATALOG_INSTALL_DIR}")
endif()
target_link_libraries(hrt_core PUBLIC Qt6::Core Qt6::Xml)
target_compile_definitions(hrt_core PUBLIC
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
    HRT_VERSION_STRING="${PROJECT_VERSION}"
    HRT_INSTALL_DATA_DIR="${CMAKE_INSTALL_PREFIX}"
    HRT_CATALOG_RELATIVE_DIR="${HRT_CATALOG_INSTALL_DIR}"
    HRT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# The user interface is a library of its own so that the widget tests can link
# the very same code the application runs.
add_library(hrt_ui STATIC
    src/ui/AnswerView.cpp
    src/ui/CatalogView.cpp
    src/ui/ExamWidget.cpp
    src/ui/ImageLoader.cpp
    src/ui/LearningWidget.cpp
    src/ui/StartupDialog.cpp
    src/ui/ValidationReport.cpp)

target_include_directories(hrt_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/ui)
target_link_libraries(hrt_ui PUBLIC hrt_core Qt6::Widgets Qt6::Gui)
if(TARGET Qt6::Svg)
    # Question and answer images are SVG in the BNetzA source, so a raster
    # fallback would show nothing at all.
    target_link_libraries(hrt_ui PUBLIC Qt6::Svg)
    target_compile_definitions(hrt_ui PUBLIC HRT_HAVE_SVG)
endif()

# Das Hauptfenster liegt in einer Bibliothek, nicht direkt im Programm: nur so
# laesst sich der Startablauf ueberhaupt pruefen, denn ein Executable kann
# nicht in ein anderes Executable gelinkt werden.
add_library(hrt_app STATIC src/ui/MainWindow.cpp)
target_link_libraries(hrt_app PUBLIC hrt_ui hrt_core Qt6::Widgets Qt6::Gui)

add_executable(HamRadioTrainer2026 WIN32 MACOSX_BUNDLE src/main.cpp)

target_link_libraries(HamRadioTrainer2026 PRIVATE hrt_app hrt_ui hrt_core Qt6::Widgets Qt6::Gui)

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
    add_test(NAME catalog COMMAND tst_catalog)

    add_executable(tst_scheduler tests/tst_scheduler.cpp)
    target_link_libraries(tst_scheduler PRIVATE hrt_core Qt6::Test)
    add_test(NAME scheduler COMMAND tst_scheduler)

    if(TARGET Qt6::Widgets)
        foreach(widget_test tst_examwidget tst_imageloader tst_learningwidget
                          tst_validationreport tst_catalogview
                          tst_startupdialog)
            add_executable(${widget_test} tests/${widget_test}.cpp)
            target_link_libraries(${widget_test} PRIVATE hrt_ui hrt_core Qt6::Test Qt6::Widgets)
            add_test(NAME ${widget_test} COMMAND ${widget_test})
            set_tests_properties(${widget_test} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        endforeach()
    endif()

    # Prueft den Startablauf: der Einrichtungsassistent muss beim ersten
    # Start erscheinen und auch dann, wenn bereits ein Katalogpfad gemerkt ist.
    add_executable(tst_startupflow tests/tst_startupflow.cpp)
    target_link_libraries(tst_startupflow PRIVATE hrt_app hrt_ui hrt_core
                                                 Qt6::Test Qt6::Widgets)
    target_compile_definitions(tst_startupflow PRIVATE
        HRT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME startupflow COMMAND tst_startupflow)
    set_tests_properties(startupflow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(tst_latex tests/tst_latex.cpp)
    # Prueft, dass das Programm seine Kataloge an den Orten findet, an denen
    # das Packaging sie ablegt: Installationspraefix, /usr und /usr/local.
    add_executable(tst_cataloglocator tests/tst_cataloglocator.cpp)
    target_link_libraries(tst_cataloglocator PRIVATE hrt_core Qt6::Test)
    add_test(NAME cataloglocator COMMAND tst_cataloglocator)

    target_link_libraries(tst_latex PRIVATE hrt_core Qt6::Test)
    add_test(NAME latex COMMAND tst_latex)

    add_executable(tst_learning tests/tst_learning.cpp)
    target_link_libraries(tst_learning PRIVATE hrt_core Qt6::Test)
    add_test(NAME learning COMMAND tst_learning)

    add_executable(tst_progress tests/tst_progress.cpp)
    target_link_libraries(tst_progress PRIVATE hrt_core Qt6::Test)
    add_test(NAME progress COMMAND tst_progress)

    add_executable(tst_exam tests/tst_exam.cpp)
    target_link_libraries(tst_exam PRIVATE hrt_core Qt6::Test)
    add_test(NAME exam COMMAND tst_exam)
endif()

# ---------------------------------------------------------------------------
# Installation
#
# The catalogs belong to the application, not to the documentation: without
# them the program starts up with nothing to show.
# ---------------------------------------------------------------------------
install(TARGETS HamRadioTrainer2026
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY data/catalogs
    DESTINATION ${HRT_CATALOG_INSTALL_DIR}
    FILES_MATCHING PATTERN "*.xml")
install(DIRECTORY data/catalogs/images
    DESTINATION ${HRT_CATALOG_INSTALL_DIR})
install(FILES schema/catalog.xsd
    DESTINATION ${HRT_CATALOG_INSTALL_DIR}/schema)
install(FILES LICENSE README.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hamradiotrainer-2026)

# Desktop entry, icon and metadata only where they serve a purpose.
if(UNIX AND NOT APPLE)
    install(FILES packaging/hamradiotrainer-2026.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/hamradiotrainer-2026.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES packaging/icons/hicolor/scalable/apps/hamradiotrainer-2026.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

# Windows: collect Qt runtime next to the executable so the bundle is portable.
if(WIN32)
    qt_generate_deploy_app_script(
        TARGET HamRadioTrainer2026
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        NO_TRANSLATIONS)
    install(SCRIPT ${deploy_script})
endif()
# ---------------------------------------------------------------------------
# CPack
#
# Linux: DEB for Debian, Ubuntu and their relatives, RPM for Fedora and RHEL.
#   An RPM also runs on openSUSE in most cases, but that is not officially
#   supported there; see packaging/README.md.
# Windows: NSIS produces a single .exe installer.
#
# Arch is not built here. Arch packages live in the AUR as a PKGBUILD that
# builds from source; packaging/PKGBUILD is the recipe for it.
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "hamradiotrainer-2026")
set(CPACK_PACKAGE_VENDOR "Jan DC6DX und HamRadioTrainer 2026 contributors")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Lern- und Prüfungstrainer für den Amateurfunk nach Amtsblatt-Verfügung 29/2024")
set(CPACK_PACKAGE_DESCRIPTION
    "HamRadioTrainer 2026 bereitet auf die Amateurfunkprüfungen der Klassen N, E "
    "und A vor. Der Fragenkatalog entspricht der dritten Auflage der "
    "Bundesnetzagentur, der Prüfungsumfang der Amtsblatt-Verfügung 29/2024.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/hamradiotrainer/hamradiotrainer-2026")
# Ein .deb braucht zwingend eine erreichbare Maintainer-Adresse, sonst bricht
# der Build ab.
set(CPACK_PACKAGE_CONTACT "jan@dc6dx.de")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_STRIP_FILES ON)

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "HamRadioTrainer 2026")
    set(CPACK_NSIS_DISPLAY_NAME "HamRadioTrainer 2026")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "HamRadioTrainer 2026")
else()
    set(CPACK_GENERATOR "DEB;RPM")

    set(CPACK_DEBIAN_PACKAGE_NAME "hamradiotrainer-2026")
    set(CPACK_DEBIAN_PACKAGE_SECTION "education")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    # SHLIBDEPS allein reicht nicht: es kann aus einer aqt-Installation kein
    # dpkg-Paket ableiten, weil dort kein Debian-Paket installiert wurde. Ein
    # deb ohne Qt-Abhaengigkeit laesst sich auf einem echten System nicht
    # installieren, also wird sie hier von Hand genannt - mit den Namen, die
    # Ubuntu 24.04 fuehrt (dort heissen sie wegen der t64-Umbenennung
    # ...6t64). Auf aelteren Debian-Systemen stimmen die Namen dann nicht;
    # CPack laesst pro Generator nur eine Liste zu, keine Alternativen.
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6core6t64, libqt6gui6t64, libqt6widgets6t64, libqt6xml6t64, libqt6svg6t64")

    set(CPACK_RPM_PACKAGE_NAME "hamradiotrainer-2026")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Education")
    # AUTOREQPROV liest die Abhaengigkeiten aus den gebauten Programmen und
    # liefert bei einer aqt-Installation nur die Systembibliotheken - Qt
    # fehlt. Deshalb nennt das rpm seine Qt-Pakete selbst; AUTOREQPROV ist
    # damit aus.
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase, qt6-qtsvg")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
endif()

include(CPack)
