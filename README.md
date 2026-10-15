# HamRadioTrainer 2026

Neu aufgebaut von **Jan DC6DX**.

Lern- und Prüfungstrainer für den Amateurfunk, mit ausdrücklich bearbeitbaren
Fragenkatalogen für die Prüfungsklassen **N**, **E** und **A**.

Der Quellcode steht unter der **GNU General Public License v3 oder später**
(siehe [LICENSE](LICENSE)).

Die **Fragentexte selbst sind nicht unter der GPL.** Sie stammen aus dem
maschinenlesbaren Fragenkatalog der Bundesnetzagentur und stehen unter der
**Datenlizenz Deutschland – Namensnennung – Version 2.0 (DL-DE-BY-2.0)**. Diese
Lizenz erlaubt ausdrücklich Vervielfältigung, Bearbeitung und Weitergabe, verlangt
aber den Quellenvermerk und den Hinweis auf Änderungen. Beides ist in den
Metadaten jedes Katalogs abgelegt — siehe [Lizenz und Quellenvermerk](#lizenz-und-quellenvermerk).

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

### Import aus dem BNetzA-Fragenkatalog

Die amtlichen Daten liegen als `fragenkatalog3b.json` plus Verzeichnis `svgs/`
vor. `hrt-catalog-import` erzeugt daraus je Prüfungsklasse einen Katalog und
kopiert die benötigten Bilder:

```sh
./build/hrt-catalog-import \
    --source /pfad/zum/Pruefungsfragen \
    --output data/catalogs
```

Ergebnis des Imports der 3. Auflage, März 2024:

| Katalog | Technische Kenntnisse | Betriebliche Kenntnisse | Kenntnisse von Vorschriften | gesamt |
|---|---:|---:|---:|---:|
| Klasse N | 195 | 172 | 204 | 571 |
| Klasse E | 463 | 172 | 204 | 839 |
| Klasse A | 716 | 172 | 204 | 1092 |

Es werden 687 SVG-Dateien kopiert.

#### Die beiden gemeinsamen Prüfungsteile

Die BNetzA führt „Betriebliche Kenntnisse" und „Kenntnisse von Vorschriften"
ausschließlich mit der Klassenangabe 1. Ihre Zugehörigkeit zu den Klassen E und
A lässt sich aus den Prüfungsunterlagen begründen:

> Für den Erwerb einer Amateurfunkprüfungsbescheinigung müssen die
> Prüfungsteile „Technische Kenntnisse", „Betriebliche Kenntnisse" und
> „Kenntnisse von Vorschriften" erfolgreich abgelegt werden. Die Inhalte und
> Anforderungen der Prüfungsteile unterscheiden sich bei den Klassen A, E und N
> nur im Prüfungsteil „Technische Kenntnisse".
> — Bundesnetzagentur, https://www.bundesnetzagentur.de/Amateurfunk

Alle drei Prüfungsteile werden also für jede Klasse verlangt, und nur
„Technische Kenntnisse" ist klassenspezifisch. Der Import behandelt die beiden
anderen deshalb als gemeinsamen Pool aller Klassen und interpretiert die
Klassenangabe 1 als „eingeführt mit Klasse N", nicht als „nur in Klasse N".

Das ist eine **Auslegung der Prüfungsordnung**, keine Aussage der Datei. Sie
steht deshalb in den `notes` jedes erzeugten Katalogs und lässt sich mit
`--no-shared-parts` abschalten. Zwei Tests sichern die Regel ab: jeder Katalog
muss drei Prüfungsteile führen, und die beiden gemeinsamen Teile müssen in allen
drei Klassen identisch sein.

Daraus folgt auch die Erklärung der Zahlen aus dem vorherigen Abschnitt: Klasse N
umfasst mehr Fragen als E, weil N alle drei Prüfungsteile abdeckt, E und A
dagegen nur die Technischen Kenntnisse klassenspezifisch haben. Die **Poolgröße
ist nicht der Prüfungsumfang** – wie viele Fragen tatsächlich gezogen werden,
steht nicht in der Quelldatei (siehe `selection`).

Zwei Eigenheiten der Quelle, die im Format berücksichtigt sind:

* **Antwort A ist immer richtig.** Die BNetzA führt die richtige Antwort
  ausschließlich in `answer_a`. Der Importer setzt `correct="true"` genau dort.
  Für die Prüfungssimulation müssen die Antworten vor der Anzeige gemischt
  werden.
* **77 Fragen haben gar keinen Antworttext**, sondern vier Antwortbilder. Das
  Schema erlaubt deshalb bei `answer` ein Attribut `image` ohne `text`.

**Fehlerpunkte werden in der Quelle nicht veröffentlicht.** Alle importierten
Fragen haben daher `points="0"`, was im Schema „unbekannt" bedeutet. Die
Prüfungssimulation kann das Fehlerpunktbudget der Prüfungsordnung erst
einhalten, wenn die Punkte ergänzt werden; der Validator weist darauf einmal je
Katalog hin.

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
data/catalogs/          Kataloge N, E, A (importiert aus der BNetzA-Quelle)
data/catalogs/images/   687 SVG-Dateien zu den Bildfragen
src/core/Catalog.*      Datenmodell
src/core/CatalogXml.*   Lesen und Schreiben im Klartext-XML
src/core/CatalogValidator.*  semantische Prüfung über das XSD hinaus
src/learning/Scheduler.*  Lernzustand je Frage, Leitner-Ordnung, Gewichtung
src/exam/PaperBuilder.*   Papierzusammenstellung und Mischen der Antworten
src/ui/MainWindow.*     Fensterrahmen und Katalogbrowser
tools/catalog-tool.cpp  Kommandozeilenwerkzeug zum Prüfen und Normalisieren
tools/catalog-import.cpp  Import des BNetzA-Fragenkatalogs
tests/                  Testsuite (Katalog und Lernlogik), 27 Tests
```

## Lernlogik

### Antworten werden gemischt

Der Quellkatalog führt die richtige Antwort immer an erster Stelle. Diese
Reihenfolge darf den Lernenden nie erreichen, sonst wäre „immer das erste Feld
anklicken" eine gewinnende Strategie.

`PaperBuilder` erzeugt deshalb für jede Frage eine eigene Antwortreihenfolge,
abgeleitet aus einer Startzahl der Prüfung. Gleiche Startzahl und gleiche Frage
ergeben immer dieselbe Reihenfolge – eine Prüfung lässt sich damit später
exakt nachvollziehen oder erneut ausdrucken. Eine feste Reihenfolge wie
`D,B,A` lässt sich vorgeben, etwa für einen gemeldeten Fehler in der
Originalprüfung.

### Gewichtete Auswahl

Sicher beherrschte Fragen sollen seltener gestellt werden als unsichere. Der
`Scheduler` hält je Frage einen Leitner-Ordner (0–5), die Trefferquote, das
Datum der letzten Beantwortung, den Fälligkeitstermin und die
Selbsteinschätzung des Lernenden.

Gewicht einer Frage:

| Faktor | Wirkung |
|---|---|
| noch nie gestellt | Faktor 10, damit der Pool vollständig einmal durchlaufen wird |
| Leitner-Ordner | Faktor 0,45 je Ordner |
| „sicher" beantwortet | × 0,30 |
| „ziemlich sicher" | × 0,55 |
| „unsicher" | × 1,60 |
| „geraten" | × 2,20 |
| heute schon gestellt | × 0,05, verhindert Doppelungen in einer Sitzung |
| lange nicht gesehen | × bis 2,0 nach 60 Tagen |

Eine richtige Antwort, die der Lernende aber nur geraten hat, erhöht den Ordner
kaum – sie kommt also bald wieder. Eine als „sicher" empfundene richtige
Antwort erhöht ihn doppelt und verschiebt die Frage weit hinaus. Falsche
Antworten senken den Ordner um zwei Stufen.

Eine Prüfungssimulation kann die Fragen je Prüfungsteil als Quote vorgeben
(`Options::questionsPerPart`), was für Klasse N mit ihren drei Prüfungsteilen
notwendig ist.

## Entwicklungsstand

Erledigt:

* Katalogformat als Klartext-XML mit Schema
* Datenmodell, XML-Leser, XML-Schreiber, semantischer Validator
* Testsuite (13 Tests) und Kommandozeilenwerkzeug
* Import des amtlichen BNetzA-Fragenkatalogs: 2502 katalogisierte Fragen für
  N, E und A (571 / 839 / 1092) inklusive 687 Bildern, schemakonform geprüft
* alle drei Prüfungsteile je Klasse, gemeinsame Teile als Pool modelliert
* GitHub-Actions-Build für Linux und Windows 10/11

Noch offen:

* Lernlogik mit Leitner-Ordnern, Fälligkeit und Selbsteinschätzung
* Prüfungssimulation mit gemischter Antwortreihenfolge und gewichteter Auswahl
* Anbindung von Lernlogik und Simulation an die Oberfläche
* Katalogpflege in der Anwendung
* Ergänzung der Fehlerpunkte, die die Quelle nicht veröffentlicht
* Darstellung der LaTeX-Formeln (KaTeX)
* Installer und Linux-Pakete (AppImage, Flatpak)

## Lizenz und Quellenvermerk

| Gegenstand | Lizenz |
|---|---|
| Quellcode der Anwendung | GPL-3.0-or-later |
| Fragentexte und Bilder | DL-DE-BY-2.0 |

Der Quellenvermerk ist in jedem Katalog als `<attribution>` abgelegt und lautet
wörtlich, wie von der BNetzA vorgegeben:

> Prüfungsfragen zum Erwerb von Amateurfunkprüfungsbescheinigungen,
> Bundesnetzagentur, 3. Auflage, März 2024,
> (www.bundesnetzagentur.de/amateurfunk), Datenlizenz Deutschland –
> Namensnennung – Version 2.0 (www.govdata.de/dl-de/by-2-0)

Sobald ein Katalog von Hand geändert wird, verlangt DL-DE-BY-2.0 zusätzlich den
**Hinweis auf die Änderung**. Dafür gibt es das Element
`<modifiedSinceSource>true</modifiedSinceSource>`; der Validator erinnert daran.
Beides gehört in die Anzeige der Anwendung.

DL-DE-BY-2.0 und GPL-3.0 stehen nicht im Widerspruch: Die Namensnennungsplicht
bindet den Fragentext, nicht den Anwendungscode.

## Geschichte

* Vorgänger *HamRadioTrainer* von DM1OLI, erschienen 2011–2015, inzwischen
  eingestellt. Kataloge im proprietären Format `.etq`, verschlüsselt.
* Dieses Projekt: Neuanfang 2026 unter GPL-3.0-or-later.