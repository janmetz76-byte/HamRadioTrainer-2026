#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
#
# Baut auf einem Mac die macOS-Version als Universal-Binary (Intel und Apple
# Silicon) und verpackt sie als Disk-Image.
#
#   tools/macos-build.sh [setup|build|test|package|all]     (Vorgabe: all)
#
#   setup    holt CMake, Ninja und aqtinstall in ein eigenes Python-venv und
#            laedt damit Qt fuer macOS von download.qt.io
#   build    konfiguriert und baut nach build-macos/
#   test     laesst die Tests laufen
#   package  erzeugt das .dmg im Quellverzeichnis; macdeployqt legt dabei
#            Qt in das Bundle und signiert es ad hoc
#
# Voraussetzung sind nur die Xcode Command Line Tools
# (xcode-select --install); Homebrew wird nicht gebraucht.
#
# Umgebungsvariablen:
#   HRT_QT_VERSION   Qt-Version (Vorgabe: 6.8.3, wie in der CI)
#   HRT_MACOS_HOME   Ablage fuer venv und Qt
#                    (Vorgabe: ~/.local/share/hamradiotrainer-macos)
#   HRT_JOBS         parallele Bau-Jobs (Vorgabe: Zahl der Kerne)

set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$SRC_DIR/build-macos"

QT_VERSION="${HRT_QT_VERSION:-6.8.3}"
MACOS_HOME="${HRT_MACOS_HOME:-$HOME/.local/share/hamradiotrainer-macos}"
VENV="$MACOS_HOME/venv"
QT_DIR="$MACOS_HOME/qt/$QT_VERSION/macos"
JOBS="${HRT_JOBS:-$(sysctl -n hw.ncpu)}"

die() { echo "Fehler: $*" >&2; exit 1; }
info() { echo "==> $*"; }

setup() {
    xcode-select -p >/dev/null 2>&1 \
        || die "Xcode Command Line Tools fehlen. Installieren mit: xcode-select --install"
    if [[ ! -x "$VENV/bin/cmake" ]]; then
        info "Python-venv mit CMake, Ninja und aqtinstall anlegen"
        mkdir -p "$MACOS_HOME"
        python3 -m venv "$VENV"
        "$VENV/bin/pip" install --quiet --upgrade pip
        "$VENV/bin/pip" install --quiet cmake ninja aqtinstall
    fi
    if [[ ! -x "$QT_DIR/bin/qmake" ]]; then
        # qtbase enthaelt seit Qt 6.8 auch qtsvg; mehr braucht die
        # Desktop-Oberflaeche nicht.
        info "Qt $QT_VERSION fuer macOS laden"
        "$VENV/bin/aqt" install-qt mac desktop "$QT_VERSION" clang_64 \
            --outputdir "$MACOS_HOME/qt"
    fi
}

build() {
    [[ -x "$QT_DIR/bin/qmake" ]] || die "Qt fehlt - zuerst: $0 setup"
    export PATH="$VENV/bin:$PATH"
    info "Konfigurieren nach $BUILD_DIR"
    cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$QT_DIR" \
        -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
    info "Bauen mit $JOBS Jobs"
    cmake --build "$BUILD_DIR" -j "$JOBS"
}

run_tests() {
    export PATH="$VENV/bin:$PATH"
    info "Tests"
    QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD_DIR" --output-on-failure
}

package() {
    export PATH="$VENV/bin:$PATH"
    info "Disk-Image erzeugen"
    (cd "$BUILD_DIR" && cpack -G DragNDrop)
    local dmg
    dmg="$(ls -t "$BUILD_DIR"/*.dmg | head -1)"
    cp "$dmg" "$SRC_DIR/"
    info "Fertig: $SRC_DIR/$(basename "$dmg")"
}

case "${1:-all}" in
    setup)   setup ;;
    build)   build ;;
    test)    run_tests ;;
    package) package ;;
    all)     setup; build; run_tests; package ;;
    *)       die "Unbekannter Schritt: $1 (setup|build|test|package|all)" ;;
esac
