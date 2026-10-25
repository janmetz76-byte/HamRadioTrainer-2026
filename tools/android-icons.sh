#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
#
# Erzeugt App-Symbol und Startbildschirm der Android-App aus
# resources/icons/app.svg nach android/res/. Nur noetig, wenn sich das Symbol
# aendert; das Ergebnis liegt im Repository.
#
# Voraussetzung: rsvg-convert (librsvg).

set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RES="$SRC_DIR/android/res"
SVG="$SRC_DIR/resources/icons/app.svg"
BACKGROUND="#1b3a57"
INK="#7fd4ff"

command -v rsvg-convert >/dev/null || { echo "Fehler: rsvg-convert fehlt (librsvg)" >&2; exit 1; }

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# Der Vordergrund des adaptiven Symbols: nur Mast und Wellen, ohne die
# Kachel. Android schneidet das Symbol je nach Geraet rund oder eckig zu und
# zeigt sicher nur die inneren 66 von 108 dp; das Zeichen bleibt darin.
glyph() {
    local colour="$1"
    cat <<EOF
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 108 108" width="108" height="108">
  <g transform="translate(18 11) scale(1.3)">
    <g fill="none" stroke="$colour" stroke-width="3.4" stroke-linecap="round">
      <path d="M20 50 V24"/>
      <path d="M15 50 h10"/>
      <path d="M26 26 a13 13 0 0 1 0 16"/>
      <path d="M33 20 a21 21 0 0 1 0 28"/>
    </g>
    <circle cx="20" cy="19" r="3.4" fill="$colour"/>
  </g>
</svg>
EOF
}
glyph "$INK" > "$tmp/foreground.svg"
glyph "#ffffff" > "$tmp/monochrome.svg"

# Dichten: Name, Faktor gegenueber mdpi.
for density in mdpi:1 hdpi:1.5 xhdpi:2 xxhdpi:3 xxxhdpi:4; do
    name="${density%%:*}"
    factor="${density##*:}"
    dir="$RES/mipmap-$name"
    mkdir -p "$dir"
    legacy="$(awk "BEGIN{print int(48*$factor)}")"
    adaptive="$(awk "BEGIN{print int(108*$factor)}")"
    # Vor Android 8 gibt es kein adaptives Symbol: dort das ganze Bild.
    rsvg-convert -w "$legacy" -h "$legacy" "$SVG" -o "$dir/ic_launcher.png"
    cp "$dir/ic_launcher.png" "$dir/ic_launcher_round.png"
    rsvg-convert -w "$adaptive" -h "$adaptive" "$tmp/foreground.svg" -o "$dir/ic_launcher_foreground.png"
    rsvg-convert -w "$adaptive" -h "$adaptive" "$tmp/monochrome.svg" -o "$dir/ic_launcher_monochrome.png"
done

mkdir -p "$RES/mipmap-anydpi-v26" "$RES/values" "$RES/values-v31" "$RES/drawable"
for name in ic_launcher ic_launcher_round; do
    cat > "$RES/mipmap-anydpi-v26/$name.xml" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<!-- Erzeugt von tools/android-icons.sh -->
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/ic_launcher_background"/>
    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>
    <monochrome android:drawable="@mipmap/ic_launcher_monochrome"/>
</adaptive-icon>
EOF
done

cat > "$RES/values/colors.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<!-- Erzeugt von tools/android-icons.sh -->
<resources>
    <color name="ic_launcher_background">$BACKGROUND</color>
</resources>
EOF

# Startbildschirm vor Android 12: der Fensterhintergrund selbst, mit dem
# Zeichen in der Mitte. Qt zeigt ihn weiter, bis die Oberflaeche steht.
cat > "$RES/drawable/splash.xml" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<!-- Erzeugt von tools/android-icons.sh -->
<layer-list xmlns:android="http://schemas.android.com/apk/res/android">
    <item android:drawable="@color/ic_launcher_background"/>
    <item android:width="160dp" android:height="160dp" android:gravity="center">
        <bitmap android:src="@mipmap/ic_launcher_foreground" android:gravity="fill"/>
    </item>
</layer-list>
EOF

cat > "$RES/values/themes.xml" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<!-- Erzeugt von tools/android-icons.sh -->
<resources>
    <style name="HrtLaunch" parent="@android:style/Theme.DeviceDefault.NoActionBar">
        <item name="android:windowBackground">@drawable/splash</item>
    </style>
</resources>
EOF

# Ab Android 12 zeigt das System den Startbildschirm selbst: Hintergrund
# und Symbol kommen aus dem Thema.
cat > "$RES/values-v31/themes.xml" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<!-- Erzeugt von tools/android-icons.sh -->
<resources>
    <style name="HrtLaunch" parent="@android:style/Theme.DeviceDefault.NoActionBar">
        <item name="android:windowBackground">@drawable/splash</item>
        <item name="android:windowSplashScreenBackground">@color/ic_launcher_background</item>
        <item name="android:windowSplashScreenAnimatedIcon">@mipmap/ic_launcher_foreground</item>
    </style>
</resources>
EOF

echo "Symbole und Startbildschirm liegen in $RES"
