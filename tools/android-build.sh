#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
#
# Baut unter Linux eine Android-APK.
#
#   tools/android-build.sh [setup|build|apk|install|keygen|all]  (Vorgabe: all)
#
#   setup    laedt Qt fuer Android von download.qt.io
#   build    konfiguriert und baut nach build-android-<abi>/
#   apk      packt die APK und signiert sie: mit dem Release-Schluessel, wenn
#            es ihn gibt, sonst mit dem Debug-Schluessel
#   install  spielt die APK per adb auf das angeschlossene Geraet
#   keygen   legt den Release-Schluessel an (einmalig)
#   all      setup, build und apk
#
# Voraussetzungen: Android SDK mit NDK und Build-Tools, JDK 17, 7z, curl und
# ein Host-Qt derselben Version (qt6-base, qt6-svg, qt6-declarative). moc,
# rcc, uic, qmlcachegen und androiddeployqt laufen auf dem Host.
#
# Umgebungsvariablen:
#   HRT_ANDROID_ABI  arm64-v8a (Handys, Vorgabe) oder x86_64 (Emulator)
#   HRT_QT_VERSION   Qt-Version fuer Android (Vorgabe: die des Host-Qt)
#   HRT_CROSS_HOME   Ablage fuer das Android-Qt
#                    (Vorgabe: ~/.local/share/hamradiotrainer-cross)
#   ANDROID_SDK_ROOT Android SDK (Vorgabe: ~/Android/Sdk)
#   ANDROID_NDK_ROOT Android NDK (Vorgabe: das neueste im SDK)
#   HRT_ANDROID_KEYSTORE  Release-Schluessel (Vorgabe:
#                    ~/.local/share/hamradiotrainer-release/release.p12);
#                    das Passwort steht in derselben Ablage in release.pass
#   HRT_ANDROID_SIGNING   "debug" erzwingt den Debug-Schluessel

set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ABI="${HRT_ANDROID_ABI:-arm64-v8a}"
BUILD_DIR="$SRC_DIR/build-android-$ABI"
OUT_APK="$SRC_DIR/HamRadioTrainer2026-android-$ABI.apk"

HOST_QT_VERSION="$(qmake6 -query QT_VERSION 2>/dev/null || true)"
QT_VERSION="${HRT_QT_VERSION:-$HOST_QT_VERSION}"
CROSS_HOME="${HRT_CROSS_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/hamradiotrainer-cross}"
QT_ABI="${ABI//-/_}"
QT_DIR="$CROSS_HOME/qt-$QT_VERSION-android-$QT_ABI"

SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
# Qt Quick (qtdeclarative) traegt die Handy-Oberflaeche.
QT_MODULES="qtbase qtsvg qtdeclarative"
# Der Release-Schluessel liegt bewusst ausserhalb des Quellbaums: wer ihn
# hat, kann Updates fuer jede installierte App ausliefern.
RELEASE_KEYSTORE="${HRT_ANDROID_KEYSTORE:-${XDG_DATA_HOME:-$HOME/.local/share}/hamradiotrainer-release/release.p12}"
RELEASE_PASS="$(dirname "$RELEASE_KEYSTORE")/release.pass"
RELEASE_ALIAS="hrt2026"

die() { echo "Fehler: $*" >&2; exit 1; }
info() { echo "==> $*"; }

case "$ABI" in
    arm64-v8a|x86_64) ;;
    *) die "HRT_ANDROID_ABI muss arm64-v8a oder x86_64 sein, nicht '$ABI'" ;;
esac

find_sdk() {
    [[ -d "$SDK/platforms" ]] || die "Android SDK nicht gefunden unter $SDK (ANDROID_SDK_ROOT setzen)"
    NDK="${ANDROID_NDK_ROOT:-}"
    if [[ -z "$NDK" ]]; then
        NDK="$(ls -d "$SDK"/ndk/*/ 2>/dev/null | sort -V | tail -1)"
        NDK="${NDK%/}"
    fi
    [[ -n "$NDK" && -d "$NDK" ]] || die "kein Android NDK gefunden (im SDK-Manager unter 'NDK' installieren)"
    BUILD_TOOLS="$(ls -d "$SDK"/build-tools/*/ 2>/dev/null | sort -V | tail -1)"
    BUILD_TOOLS="${BUILD_TOOLS%/}"
    [[ -x "$BUILD_TOOLS/apksigner" ]] || die "keine Android Build-Tools mit apksigner gefunden"
}

check_host_qt() {
    [[ -n "$HOST_QT_VERSION" ]] || die "kein Host-Qt gefunden (qmake6). Installieren mit: sudo pacman -S qt6-base qt6-svg"
    [[ "$HOST_QT_VERSION" == "$QT_VERSION" ]] || die \
        "Host-Qt ist $HOST_QT_VERSION, Android-Qt soll $QT_VERSION sein. Die Versionen muessen gleich sein."
}

# ---------------------------------------------------------------------------
# setup
# ---------------------------------------------------------------------------
cmd_setup() {
    command -v 7z >/dev/null || die "7z fehlt. Installieren mit: sudo pacman -S 7zip"
    if [[ -f "$QT_DIR/.complete" && "$(cat "$QT_DIR/.complete")" == "$QT_MODULES" ]]; then
        info "Qt $QT_VERSION fuer Android ($ABI) liegt bereits in $QT_DIR"
        return
    fi

    local v="${QT_VERSION//./}"
    local repo="https://download.qt.io/online/qtsdkrepository/all_os/android/qt6_$v/qt6_${v}_$QT_ABI/qt.qt6.$v.android_$QT_ABI"
    info "Suche Qt $QT_VERSION fuer Android ($ABI) auf download.qt.io"
    local listing
    listing="$(curl -fsSL "$repo/")" || die "Qt $QT_VERSION fuer Android ($ABI) gibt es nicht unter $repo"

    local tmp; tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' RETURN
    rm -rf "$QT_DIR"; mkdir -p "$QT_DIR"

    for module in $QT_MODULES; do
        local archive
        archive="$(grep -oE "href=\"[^\"]*${module}-[^\"]*-Android-[^\"]*\.7z\"" <<<"$listing" \
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
    inner="$(find "$tmp/x" -mindepth 1 -maxdepth 3 -type d -name lib -printf '%h\n' | head -1)"
    [[ -n "$inner" ]] || die "unerwarteter Aufbau der Qt-Archive"
    cp -a "$inner"/. "$QT_DIR"/
    # Die Liste der Module markiert den Stand: kommt eines dazu, wird neu geladen.
    echo "$QT_MODULES" > "$QT_DIR/.complete"
    info "Qt $QT_VERSION fuer Android liegt in $QT_DIR"
}

# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------
cmd_build() {
    find_sdk
    check_host_qt
    [[ "$(cat "$QT_DIR/.complete" 2>/dev/null)" == "$QT_MODULES" ]] \
        || die "Android-Qt fehlt oder ist unvollstaendig, zuerst: $0 setup"

    info "Konfiguriere nach $BUILD_DIR (NDK $(basename "$NDK"))"
    cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$QT_DIR/lib/cmake/Qt6/qt.toolchain.cmake" \
        -DQT_HOST_PATH=/usr \
        -DQT_HOST_PATH_CMAKE_DIR=/usr/lib/cmake \
        -DANDROID_SDK_ROOT="$SDK" \
        -DANDROID_NDK_ROOT="$NDK"
    info "Baue"
    cmake --build "$BUILD_DIR" --target HamRadioTrainer2026
}

# ---------------------------------------------------------------------------
# apk
# ---------------------------------------------------------------------------
cmd_apk() {
    find_sdk
    [[ -d "$BUILD_DIR" ]] || die "kein Build vorhanden, zuerst: $0 build"

    info "Packe die APK"
    # androiddeployqt raeumt nicht auf: Bibliotheken frueherer Laeufe blieben
    # liegen und landeten in jeder neuen APK (zeitweise Qt Widgets und ein
    # Dutzend QML-Debugging-Plugins, die niemand mehr braucht).
    # Die Marke android-build/<Ziel>.apk muss mit weg, sonst haelt Ninja das
    # Packen fuer erledigt und laesst es aus.
    rm -rf "$BUILD_DIR/android-build/libs" "$BUILD_DIR/android-build/build" \
           "$BUILD_DIR/android-build/HamRadioTrainer2026.apk"
    cmake --build "$BUILD_DIR" --target apk

    local unsigned
    unsigned="$(find "$BUILD_DIR" -path '*outputs/apk/*' -name '*.apk' -newer "$BUILD_DIR/CMakeCache.txt" \
                | head -1)"
    [[ -n "$unsigned" ]] || die "keine APK gefunden unter $BUILD_DIR"

    # Eine Release-APK ist unsigniert und laesst sich so nicht installieren.
    # Veroeffentlicht wird nur mit dem Release-Schluessel: Android nimmt ein
    # Update nur an, wenn es mit demselben Schluessel signiert ist wie die
    # installierte App. Der Debug-Schluessel bleibt fuer Versuche ohne ihn.
    local sign_args
    if [[ "${HRT_ANDROID_SIGNING:-}" != "debug" && -f "$RELEASE_KEYSTORE" ]]; then
        [[ -f "$RELEASE_PASS" ]] || die "Passwortdatei fehlt: $RELEASE_PASS"
        info "Signiere mit dem Release-Schluessel"
        # Kein --key-pass: bei PKCS12 ist es dasselbe Passwort, und apksigner
        # laese es sonst ein zweites Mal aus derselben Datei - hinter dem Ende.
        sign_args=(--ks "$RELEASE_KEYSTORE" --ks-key-alias "$RELEASE_ALIAS"
                   --ks-pass "file:$RELEASE_PASS")
    else
        local keystore="$HOME/.android/debug.keystore"
        if [[ ! -f "$keystore" ]]; then
            info "Lege den Android-Debug-Schluessel an"
            mkdir -p "$(dirname "$keystore")"
            keytool -genkeypair -keystore "$keystore" -storepass android -keypass android \
                -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
                -dname "CN=Android Debug,O=Android,C=US" >/dev/null
        fi
        info "Signiere mit dem Debug-Schluessel - nicht zur Veroeffentlichung"
        sign_args=(--ks "$keystore" --ks-key-alias androiddebugkey
                   --ks-pass pass:android --key-pass pass:android)
    fi
    rm -f "$OUT_APK"
    "$BUILD_TOOLS/zipalign" -f -p 4 "$unsigned" "$OUT_APK.aligned"
    "$BUILD_TOOLS/apksigner" sign "${sign_args[@]}" --out "$OUT_APK" "$OUT_APK.aligned"
    rm -f "$OUT_APK.aligned" "$OUT_APK.idsig"
    "$BUILD_TOOLS/apksigner" verify --print-certs "$OUT_APK" | grep -m2 -E "DN:|SHA-256"
    info "Fertig: $OUT_APK ($(du -h "$OUT_APK" | cut -f1))"
}

# ---------------------------------------------------------------------------
# keygen
# ---------------------------------------------------------------------------
cmd_keygen() {
    command -v keytool >/dev/null || die "keytool fehlt (gehoert zum JDK)"
    command -v openssl >/dev/null || die "openssl fehlt"
    [[ -e "$RELEASE_KEYSTORE" ]] && die "Release-Schluessel gibt es schon: $RELEASE_KEYSTORE"

    local dir
    dir="$(dirname "$RELEASE_KEYSTORE")"
    mkdir -p "$dir"
    chmod 700 "$dir"
    # Ein zufaelliges Passwort in einer Datei, die nur der Besitzer lesen
    # kann. So taucht es weder in der Prozessliste noch im Verlauf auf.
    ( umask 077; openssl rand -base64 33 | tr -d '\n' > "$RELEASE_PASS" )

    info "Lege den Release-Schluessel an"
    # PKCS12, RSA 4096, gueltig rund 30 Jahre: Android verlangt, dass jedes
    # Update mit demselben Schluessel signiert ist, solange es die App gibt.
    keytool -genkeypair -storetype PKCS12 -keystore "$RELEASE_KEYSTORE" \
        -storepass:file "$RELEASE_PASS" -keypass:file "$RELEASE_PASS" \
        -alias "$RELEASE_ALIAS" -keyalg RSA -keysize 4096 -validity 11000 \
        -dname "CN=HamRadioTrainer 2026,OU=Jan Metz - DC6DX,O=hrt2026.de,C=DE" >/dev/null
    chmod 600 "$RELEASE_KEYSTORE"

    keytool -list -v -storetype PKCS12 -keystore "$RELEASE_KEYSTORE" \
        -storepass:file "$RELEASE_PASS" -alias "$RELEASE_ALIAS" \
        | grep -E "Eigent|Owner|SHA256|Gültig|Valid"
    info "Schluessel: $RELEASE_KEYSTORE"
    info "Passwort:   $RELEASE_PASS"
    info "Beide Dateien sicher aufbewahren. Ohne sie lassen sich keine Updates mehr ausliefern."
}

# ---------------------------------------------------------------------------
# install
# ---------------------------------------------------------------------------
cmd_install() {
    find_sdk
    [[ -f "$OUT_APK" ]] || die "keine APK vorhanden, zuerst: $0"
    "$SDK/platform-tools/adb" install -r "$OUT_APK"
}

case "${1:-all}" in
    setup)   cmd_setup ;;
    build)   cmd_build ;;
    apk)     cmd_apk ;;
    install) cmd_install ;;
    keygen)  cmd_keygen ;;
    all)     cmd_setup; cmd_build; cmd_apk ;;
    *)       sed -n '5,15p' "$0"; exit 1 ;;
esac
