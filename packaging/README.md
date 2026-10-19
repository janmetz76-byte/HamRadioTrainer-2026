# Verpacken und Verteilen

Dieses Verzeichnis enthält alles, was zwischen dem Quelltext und einem
Paket liegt, das eine Person installieren kann.

## Womit wird gebaut

Alles entsteht mit **CPack** aus derselben CMake-Konfiguration:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && cpack
```

## Windows

CPack-Generator `NSIS` erzeugt **eine einzelne `.exe`**, die das Programm
installiert und wieder deinstalliert. Die Qt-Laufzeitbibliotheken legt
`windeployqt` über `qt_generate_deploy_app_script` daneben, deshalb braucht der
Zielrechner kein Qt.

Werkzeug auf dem Bau-Rechner: `makensis`. Auf den GitHub-Actions-Läufern für
Windows ist es vorinstalliert, sonst `choco install nsis`.

## Linux

| Distribution | Paket | Werkzeug |
|---|---|---|
| Debian, Ubuntu und Abkömmlinge | `.deb` | `dpkg-dev` |
| Fedora, RHEL, CentOS Stream | `.rpm` | `rpm` |
| Arch Linux | **kein Binärpaket, siehe unten** | – |
| openSUSE / SuSE | `.rpm` funktioniert meist, AppImage ist der sichere Weg | `rpm` |
| Void, Gentoo, Solus, Slackware | `.rpm` oder `.deb` meist nicht passend | – |

Die Abhängigkeiten werden nicht von Hand eingetragen. CPack ermittelt sie beim
Bauen aus den Bibliotheken, die das Programm wirklich benutzt
(`CPACK_DEBIAN_PACKAGE_SHLIBDEPS`, `CPACK_RPM_PACKAGE_AUTOREQPROV`). Damit
stimmen sie automatisch zur Qt-Version, gegen die gebaut wurde.

### Arch Linux: hier endet die Zuständigkeit des Projekts

**Ein Arch-Binärpaket kann dieses Projekt nicht selbst erzeugen.** Arch-Linux
installiert ausschließlich aus dem AUR, und ein AUR-Eintrag ist eine
`PKGBUILD`, die **aus dem Quelltext baut** – kein fertiges Binary. Das ist
Absicht: Arch verwaltet Pakete selbst, damit sie zur Version der
Distribution passen.

Was dieses Projekt liefern kann und liefert:

* `packaging/PKGBUILD` – das Rezept, fertig und kommentiert
* das Quellarchiv als Release, mit fester Prüfsumme

Was es nicht liefern kann: die Aufnahme in den AUR. Die gehört zu einem
AUR-Konto, und wer sie anmeldet, pflegt sie fortlaufend. Anleitung in
`README.md` im Abschnitt *Verteilen auf Arch*.

Wer Arch ohne AUR bedienen möchte, baut mit `makepkg -si` aus diesem Verzeichnis.

### openSUSE

Ein RPM aus diesem Projekt läuft auf openSUSE in der Regel, ist dort aber nicht
offiziell unterstützt – openSUSE-Pakete entstehen im Open Build Service und
haben eigene Abhängigkeitsangaben. Ein RPM zu veröffentlichen kostet also wenig
und deckt den Fall ab; wer es sauber will, legt zusätzlich ein OBS-Rezept an.

### Der Rest

Für alle übrigen Distributionen gibt es zwei Wege: die `.deb` oder `.rpm` als
Ausgangspunkt für eine eigene Regel, oder ein AppImage. Ein AppImage bündelt
das Programm samt Qt und läuft ohne Installation auf jedem x86-64-Linux – es
eignet sich damit auch als Notlösung für Systeme, für die es keine Paketregel
gibt.

Ein AppImage ist hier noch nicht eingerichtet. Es braucht `appimagetool` oder
`linuxdeployqt`, die beide nicht Teil von CPack sind und beim Bauen
zusätzlich beschafft werden müssten.

## Paketkontakt

Der Paketkontakt steht auf `jan@dc6dx.de` und ist in `CMakeLists.txt` über
`CPACK_PACKAGE_CONTACT` gesetzt. Ein `.deb` lässt sich ohne Maintainer-Adresse
nicht bauen; für RPM und NSIS ist das Feld freiwillig, wird aber mitgeführt.

## Prüfen, was in einem Paket landet

```sh
cd build
cpack -G DEB         # erzeugt das .deb im Buildverzeichnis
dpkg-deb -c ../build/hamradiotrainer-2026-*.deb   # Inhalt ansehen
dpkg-deb -I ../build/hamradiotrainer-2026-*.deb   # Metadaten ansehen
```

Erwartet werden genau drei Katalogdateien und 687 Bilder unter
`share/hamradiotrainer-2026/`. Fehlt eines davon, findet das installierte
Programm keine Fragen.

## Dateien

| Datei | Zweck |
|---|---|
| `hamradiotrainer-2026.desktop` | Menüeintrag für die Desktop-Umgebung |
| `hamradiotrainer-2026.metainfo.xml` | Angaben für die Softwarecenter |
| `../resources/icons/app.svg` | Programmsymbol, wird nach `share/icons/hicolor/scalable/apps` installiert |
| `PKGBUILD` | Rezept für das AUR |