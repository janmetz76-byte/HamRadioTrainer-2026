# HamRadioTrainer 2026

Neu aufgebaut von **Jan DC6DX**.

Lern- und Prüfungstrainer für den Amateurfunk, mit ausdrücklich bearbeitbaren
Fragenkatalogen für die Prüfungsklassen **N**, **E** und **A**.

Die Software steht unter der **GNU General Public License v3 oder später**
(siehe [LICENSE](LICENSE)). Die Veröffentlichung des Quellcodes steht unter der
Bedingung, dass die Lizenz der Fragentexte selbst geklärt ist — dazu siehe
[Lizenzfragen zu den Musterfragen](#lizenzfragen-zu-den-musterfragen).

## Warum ein Neubau?

Das Vorgängerprogramm *HamRadioTrainer* von DM1OLI wird nicht mehr
weiterentwickelt. Der Originalquellcode liegt nicht vor, die Fragenkataloge des
Vorgängers lagen in einem verschlüsselten Binärformat (`.etq`) vor und sind
inhaltlich veraltet.

Deshalb wird hier bewusst neu aufgebaut:

* **Klartext-Datenformat.** Die Fragenkataloge liegen als unverschlüsseltes XML
  vor. Jeder Texteditor genügt; Änderungen sind im Git nachvollziehbar.
* **Aktueller Fragenstand.** Die Kataloge werden am aktuellen Stand der
  BNetzA ausgerichtet und um die zusätzliche Prüfungsklasse **N** ergänzt.
* **Aktuelle Technik.** Qt 6 / C++20, unterstützt Linux sowie Windows 10/11.

## Zielplattformen

| Plattform | Status |
|---|---|
| Linux (X11/Wayland) | gebaut und getestet |
| Windows 10/11 x64 | CI-Build (GitHub Actions), Paketierung über `windeployqt` |

## Bauen

Voraussetzungen: CMake ≥ 3.21, ein C++20-fähiger Compiler, Qt 6.5 oder neuer
mit den Modulen `Core`, `Gui`, `Widgets`, `Xml` und optional `Test`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/HamRadioTrainer2026
```

### Linux

Distributionen mit Qt 6:

```sh
# Arch / CachyOS
sudo pacman -S --needed cmake ninja qt6-base
# Debian / Ubuntu
sudo apt install cmake ninja-build qt6-base-dev
```

### Windows 10/11

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --prefix dist
```

`cmake --install` ruft `windeployqt` auf und legt die Qt-Laufzeitbibliotheken
neben der EXE ab. Ergebnis ist ein Verzeichnis `dist`, das ohne Installation auf
einem anderen Windows-Rechner läuft.

## Fragenkataloge

### Format

Das Format ist in [`schema/catalog.xsd`](schema/catalog.xsd) definiert und
absichtlich streng gehalten:

```xml
<catalog schemaVersion="1">
  <metadata>
    <title>Prüfungskatalog Klasse E</title>
    <examClass>E</examClass>       <!-- N | E | A -->
    <examId>DE-E</examId>
    <catalogVersion>2026-01</catalogVersion>
    <validFrom>2026-01-01</validFrom>
  </metadata>
  <parts>
    <part id="DE-E-1" title="Teil 1" order="1">
      <section id="DE-E-1-1" title="Recht" order="1">
        <selection questions="40" errorPoints="360"/>
        <chapters>
          <chapter id="DE-E-1-1-1" title="Allgemeines" order="1">
            <questions>
              <question id="DE-E-1-1-1-001" order="1" type="single" points="2">
                <text>Frage?</text>
                <answers>
                  <answer id="DE-E-1-1-1-001-a" order="1" correct="true"  text="Richtig"/>
                  <answer id="DE-E-1-1-1-001-b" order="2" correct="false" text="Falsch"/>
                </answers>
                <explanation>Begründung</explanation>
                <history>
                  <addedOn>2026-01-01</addedOn>
                  <changedOn>2026-07-01</changedOn>
                  <retiredOn>2027-01-01</retiredOn>
                </history>
              </question>
            </questions>
          </chapter>
        </chapters>
      </section>
    </part>
  </parts>
</catalog>
```

Die Element- und Attributnamen sind englisch, damit Werkzeuge sprachneutral
bleiben. Oberfläche und Dokumentation sind deutsch.

Kennungen (`id`) sind dokumentweit eindeutig und beginnen mit der Kennung des
übergeordneten Kapitels, damit die Zuordnung beim Nachtragen von Fragen
nachvollziehbar bleibt.

### `points` und Fehlerpunkte

`points` ist die Zahl der Fehlerpunkte, die im amtlichen Prüfungswesen für eine
falsche Antwort auf diese Frage berechnet werden. Prüfungssimulationen werden
danach zusammengestellt, dass Fehlerpunktbudget der Prüfungsordnung eingehalten
wird.

### `history`

`addedOn`, `changedOn` und `retiredOn` machen den Stand einer Frage nachvollziehbar.
Zurückgezogene Fragen (`retiredOn`) bleiben im Katalog, damit alte Lernstatistiken
reproduzierbar bleiben, werden aber nie neu gezogen. Gesperrte Fragen
(`locked="true"`) werden ebenfalls nie gezogen, können aber noch in alten
Statistiken vorkommen.

### Katalog prüfen und normalisieren

Das Kommandozeilenwerkzeug `hrt-catalog-tool` prüft Kataloge ohne grafische
Oberfläche und eignet sich für CI:

```sh
# Kennzahlen und Beanstandungen ausgeben
./build/hrt-catalog-tool --info data/catalogs/*.xml

# Semantisch prüfen, Exitcode 1 bei Fehlern
./build/hrt-catalog-tool data/catalogs/*.xml

# Prüfen, ob eine Datei dem kanonischen Format entspricht (nichts wird geschrieben)
./build/hrt-catalog-tool --normalize data/catalogs/*.xml

# Prüfen und anschließend in das kanonische Format zurückschreiben
./build/hrt-catalog-tool --normalize --write data/catalogs/*.xml
```

Zusätzlich prüft `xmllint` gegen das Schema:

```sh
xmllint --noout --schema schema/catalog.xsd data/catalogs/de-amateurfunk-E.xml
```

## Projektstruktur

```
CMakeLists.txt          Projekt, Build, Installation
LICENSE                 GPL-3.0-or-later
schema/catalog.xsd      verbindliche Definition des Katalogformats
data/catalogs/          Beispielkataloge N, E, A (Platzhalter, keine amtlichen Fragen)
src/core/Catalog.*      Datenmodell
src/core/CatalogXml.*   Lesen und Schreiben im Klartext-XML
src/core/CatalogValidator.*  semantische Prüfung über das XSD hinaus
src/ui/MainWindow.*     Fensterrahmen und Katalogbrowser
tools/catalog-tool.cpp  Kommandozeilenwerkzeug
tests/tst_catalog.cpp   Testsuite
```

## Entwicklungsstand

Erledigt:

* Katalogformat als Klartext-XML mit Schema
* Datenmodell, XML-Leser, XML-Schreiber, semantischer Validator
* Testsuite (13 Tests) und Kommandozeilenwerkzeug
* Beispielkataloge für N, E, A
* GitHub-Actions-Build für Linux und Windows 10/11

Noch offen:

* Lernmodus mit Fortschrittsstatistik
* Prüfungssimulation nach Prüfungsordnung
* Katalogpflege in der Anwendung
* Erfassung des aktuellen BNetzA-Fragenstands
* Installer und Linux-Pakete (AppImage, Flatpak)

## Lizenzfragen zu den Musterfragen

Die GPL gilt für den Quellcode dieser Anwendung. Sie gilt **nicht** automatisch
für die amtlichen Musterfragen der BNetzA beziehungsweise des DL. Bevor ein
Katalog mit echtem Fragenmaterial veröffentlicht wird, muss geklärt werden,
unter welcher Lizenz dieses Material steht. Diese Frage ist im Projekt noch offen
und blockiert die Veröffentlichung der fertigen Kataloge.

## Geschichte

* Vorgänger *HamRadioTrainer* von DM1OLI, erschienen 2011–2015, inzwischen
  eingestellt. Kataloge im proprietären Format `.etq`, verschlüsselt.
* Dieses Projekt: Neuanfang 2026 unter GPL-3.0-or-later.