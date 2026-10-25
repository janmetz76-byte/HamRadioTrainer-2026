#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
#
# Baut unter Linux eine lauffaehige Windows-Version (x64) und stellt sie als
# portablen Ordner zusammen - das, was windeployqt unter Windows erledigt.
#
#   tools/windows-cross.sh [setup|build|deploy|all]     (Vorgabe: all)
#
#   setup   laedt Qt fuer Windows (llvm-mingw) von download.qt.io
#   build   konfiguriert und baut nach build-win/
#   deploy  installiert nach dist-win/ und legt alle benoetigten DLLs dazu
#
# Voraussetzungen: llvm-mingw (pacman -S llvm-mingw), 7z, curl und ein Host-Qt
# derselben Version (qt6-base, qt6-svg). moc, rcc und uic laufen auf dem Host,
# und ihr Ergebnis passt nur zu Qt-Kopfdateien derselben Version.
#
# Umgebungsvariablen:
#   HRT_QT_VERSION   Qt-Version fuer Windows (Vorgabe: die des Host-Qt)
#   HRT_CROSS_HOME   Ablage fuer das Windows-Qt
#                    (Vorgabe: ~/.local/share/hamradiotrainer-cross)
#   HRT_LLVM_MINGW   Wurzel der llvm-mingw-Toolchain, falls nicht im PATH

set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$SRC_DIR/build-win"
DIST_DIR="$SRC_DIR/dist-win"

HOST_QT_VERSION="$(qmake6 -query QT_VERSION 2>/dev/null || true)"
QT_VERSION="${HRT_QT_VERSION:-$HOST_QT_VERSION}"
CROSS_HOME="${HRT_CROSS_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/hamradiotrainer-cross}"
QT_DIR="$CROSS_HOME/qt-$QT_VERSION-llvm-mingw"
TRIPLE=x86_64-w64-mingw32

# Plugins, die das Programm braucht. Fehlt eines, startet es trotzdem - ohne
# qwindows aber gar nicht, ohne die SVG-Plugins mit leeren Bildern und
# Symbolen und ohne Fehlermeldung.
PLUGINS=(
    platforms/qwindows.dll
    imageformats/qsvg.dll
    iconengines/qsvgicon.dll
    styles/qmodernwindowsstyle.dll
)

die() { echo "Fehler: $*" >&2; exit 1; }
info() { echo "==> $*"; }

find_toolchain() {
    local cxx
    cxx="$(command -v "$TRIPLE-clang++" || true)"
    if [[ -z "$cxx" ]]; then
        for root in "${HRT_LLVM_MINGW:-}" /opt/llvm-mingw /usr/lib/llvm-mingw; do
            [[ -n "$root" && -x "$root/bin/$TRIPLE-clang++" ]] && cxx="$root/bin/$TRIPLE-clang++" && break
        done
    fi
    [[ -n "$cxx" ]] || die "llvm-mingw nicht gefunden. Installieren mit: sudo pacman -S llvm-mingw"
    TOOLCHAIN_BIN="$(dirname "$cxx")"
    TOOLCHAIN_ROOT="$(dirname "$TOOLCHAIN_BIN")"
    OBJDUMP="$TOOLCHAIN_BIN/$TRIPLE-objdump"
    [[ -x "$OBJDUMP" ]] || OBJDUMP="$(command -v llvm-objdump)" || die "kein objdump gefunden"
}

check_host_qt() {
    [[ -n "$HOST_QT_VERSION" ]] || die "kein Host-Qt gefunden (qmake6). Installieren mit: sudo pacman -S qt6-base qt6-svg"
    [[ "$HOST_QT_VERSION" == "$QT_VERSION" ]] || die \
        "Host-Qt ist $HOST_QT_VERSION, Windows-Qt soll $QT_VERSION sein. Die Versionen muessen gleich sein."
}

# ---------------------------------------------------------------------------
# setup
# ---------------------------------------------------------------------------
cmd_setup() {
    command -v 7z >/dev/null || die "7z fehlt. Installieren mit: sudo pacman -S 7zip"
    if [[ -f "$QT_DIR/.complete" ]]; then
        info "Qt $QT_VERSION fuer Windows liegt bereits in $QT_DIR"
        return
    fi

    local v="${QT_VERSION//./}"
    local repo="https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_$v/qt6_${v}_llvm_mingw/qt.qt6.$v.win64_llvm_mingw"
    info "Suche Qt $QT_VERSION (llvm-mingw) auf download.qt.io"
    local listing
    listing="$(curl -fsSL "$repo/")" || die "Qt $QT_VERSION fuer llvm-mingw gibt es nicht unter $repo"

    local tmp; tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' RETURN
    rm -rf "$QT_DIR"; mkdir -p "$QT_DIR"

    for module in qtbase qtsvg; do
        local archive
        archive="$(grep -oE "href=\"[^\"]*${module}-Windows-[^\"]*-Clang-[^\"]*\.7z\"" <<<"$listing" \
                   | head -1 | sed -e 's/^href="//' -e 's/"$//')"
        [[ -n "$archive" ]] || die "kein Archiv fuer $module gefunden"
        info "Lade $module"
        curl -fL --progress-bar -o "$tmp/$archive" "$repo/$archive"
        local want have
        want="$(curl -fsSL "$repo/$archive.sha1" | awk '{print $1}')"
        have="$(sha1sum "$tmp/$archive" | awk '{print $1}')"
        [[ -n "$want" && "$want" == "$have" ]] || die "Pruefsumme von $archive stimmt nicht"
        7z x -y -bso0 -bsp0 -o"$tmp/x" "$tmp/$archive"
    done

    # Die Archive tragen den Pfad <version>/<arch>/ in sich; nach oben ziehen.
    local inner
    inner="$(find "$tmp/x" -mindepth 1 -maxdepth 3 -type d -name bin -printf '%h\n' | head -1)"
    [[ -n "$inner" ]] || die "unerwarteter Aufbau der Qt-Archive"
    cp -a "$inner"/. "$QT_DIR"/
    touch "$QT_DIR/.complete"
    info "Qt $QT_VERSION fuer Windows liegt in $QT_DIR"
}

# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------
cmd_build() {
    find_toolchain
    check_host_qt
    [[ -f "$QT_DIR/.complete" ]] || die "Windows-Qt fehlt, zuerst: $0 setup"

    info "Konfiguriere nach $BUILD_DIR"
    HRT_LLVM_MINGW="$TOOLCHAIN_ROOT" cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$QT_DIR/lib/cmake/Qt6/qt.toolchain.cmake" \
        -DQT_CHAINLOAD_TOOLCHAIN_FILE="$SRC_DIR/cmake/llvm-mingw-x86_64.cmake" \
        -DQT_HOST_PATH=/usr \
        -DQT_HOST_PATH_CMAKE_DIR=/usr/lib/cmake \
        -DCMAKE_INSTALL_PREFIX="$DIST_DIR"
    info "Baue"
    cmake --build "$BUILD_DIR"
}

# ---------------------------------------------------------------------------
# deploy
# ---------------------------------------------------------------------------

# Liefert die DLL-Namen, die eine PE-Datei importiert.
imports_of() {
    "$OBJDUMP" -p "$1" | sed -n 's/^[[:space:]]*DLL Name: //p'
}

# Sucht eine DLL in Qt und in der Laufzeit der Toolchain. Windows-DLLs wie
# KERNEL32 oder api-ms-win-* liegen nirgends davon und bleiben aussen vor.
locate_dll() {
    local name="$1" dir hit
    for dir in "$QT_DIR/bin" "$TOOLCHAIN_ROOT/$TRIPLE/bin"; do
        hit="$(find "$dir" -maxdepth 1 -iname "$name" -print -quit 2>/dev/null)"
        [[ -n "$hit" ]] && { echo "$hit"; return; }
    done
    return 0
}

cmd_deploy() {
    find_toolchain
    [[ -d "$BUILD_DIR" ]] || die "kein Build vorhanden, zuerst: $0 build"

    info "Installiere nach $DIST_DIR"
    rm -rf "$DIST_DIR"
    cmake --install "$BUILD_DIR" --prefix "$DIST_DIR" >/dev/null

    local bin="$DIST_DIR/bin"
    [[ -f "$bin/HamRadioTrainer2026.exe" ]] || die "HamRadioTrainer2026.exe fehlt in $bin"

    local plugin
    for plugin in "${PLUGINS[@]}"; do
        [[ -f "$QT_DIR/plugins/$plugin" ]] || die "Qt-Plugin fehlt: $plugin"
        mkdir -p "$bin/$(dirname "$plugin")"
        cp "$QT_DIR/plugins/$plugin" "$bin/$plugin"
    done

    # Abhaengigkeiten rekursiv aufloesen: jede DLL, die neu dazukommt, kann
    # selbst wieder weitere verlangen.
    local -a queue=("$bin"/*.exe)
    for plugin in "${PLUGINS[@]}"; do queue+=("$bin/$plugin"); done
    local -A done_dlls=() system_dlls=()
    local file dll lower path
    while ((${#queue[@]})); do
        file="${queue[0]}"; queue=("${queue[@]:1}")
        while read -r dll; do
            lower="${dll,,}"
            [[ -n "${done_dlls[$lower]:-}" || -n "${system_dlls[$lower]:-}" ]] && continue
            path="$(locate_dll "$dll")"
            if [[ -z "$path" ]]; then
                system_dlls[$lower]=1
                continue
            fi
            done_dlls[$lower]=1
            cp "$path" "$bin/"
            queue+=("$bin/$(basename "$path")")
        done < <(imports_of "$file")
    done

    info "Mitgeliefert (${#done_dlls[@]} DLLs, ${#PLUGINS[@]} Plugins):"
    printf '    %s\n' $(cd "$bin" && ls *.dll) "${PLUGINS[@]}"
    info "Von Windows erwartet:"
    printf '    %s\n' $(printf '%s\n' "${!system_dlls[@]}" | grep -v '^api-ms-win-' | sort)
    if printf '%s\n' "${!system_dlls[@]}" | grep -qiE '^(qt6|lib|zlib)'; then
        die "eine Qt- oder Laufzeit-DLL wurde nicht gefunden (siehe oben)"
    fi

    local zip="$SRC_DIR/HamRadioTrainer2026-windows-x64.zip"
    rm -f "$zip"
    (cd "$DIST_DIR" && 7z a -tzip -bso0 -bsp0 "$zip" .)
    info "Fertig: $DIST_DIR (Start: bin/HamRadioTrainer2026.exe)"
    info "Als ZIP fuer die VM: $zip"
}

case "${1:-all}" in
    setup)  cmd_setup ;;
    build)  cmd_build ;;
    deploy) cmd_deploy ;;
    all)    cmd_setup; cmd_build; cmd_deploy ;;
    *)      sed -n '5,13p' "$0"; exit 1 ;;
esac
