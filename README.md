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

### Katalog und Prüfungsstruktur

Die fünf Prüfungsteile und der Prüfungsumfang folgen der **Amtsblatt-Verfügung
29/2024** (gültig ab 24.06.2024), nicht der Gliederung der Quelldatei:

| Prüfungsteil | Inhalt                      | Fragen | Punkte | Bestehen | Fehlerpunkte | Zeit |
|--------------|-----------------------------|--------|--------|----------|--------------|------|
| V            | Kenntnisse von Vorschriften  | 25     | 25     | 19       | 6            | 45 min |
| B            | Betriebliche Kenntnisse      | 25     | 25     | 19       | 6            | 45 min |
| N            | Technische Kenntnisse, Klasse N | 25  | 25     | 19       | 6            | 45 min |
| E            | Technische Kenntnisse, Klasse E | 25  | 25     | 19       | 6            | 45 min |
| A            | Technische Kenntnisse, Klasse A | 25  | 25     | 19       | 6            | 60 min |

Eine Frage zählt einen Fehlerpunkt, da die Verfügung für jede richtige Antwort
einen Punkt vorsieht. Bestanden sind 19 der 25 Punkte, also sind 6 falsche
Antworten zulässig. Wird nur ein Teil nicht bestanden, folgt die mündliche
Nachprüfung in genau diesem Teil.

Eine Bescheinigung umfasst alle Teile bis einschließlich der eigenen Klasse, weil
Inhaber einer niedrigeren Klasse nur die höheren Teile ablegen:

| Zertifikat | Prüfungsteile          | Bögen | Fragen je Prüfung | Pool im Katalog |
|------------|------------------------|-------|-------------------|-----------------|
| Klasse N   | V, B, N                | 3     | 75                | 571             |
| Klasse E   | V, B, N, E             | 4     | 100               | 1034            |
| Klasse A   | V, B, N, E, A          | 5     | 125               | 1750            |

Die Quelldatei kennt diese Aufteilung nicht. Sie gliedert nach
„Betriebliche Kenntnisse“, „Kenntnisse von Vorschriften“ und
„Technische Kenntnisse“; letztere trägt je Frage eine Klassenmarkierung, die
Teile V und B tragen überhaupt keine. Der Importer führt diese Gliederung in die
amtlichen Prüfungsteile über: V und B werden klassenübergreifend gemeinsam
genutzt, die technischen Fragen werden nach ihrer Klassenmarkierung auf N, E
und A verteilt. Für Klasse E enthält der Katalog deshalb auch den
N-Prüfungsteil, für Klasse A zusätzlich N und E.

### Quellen

- `Prüfungsfragen/fragenkatalog3b.json`, 3. Auflage, März 2024, gültig ab 24.06.2024
- `Pruefungsfragen/svgs/`, 687 benötigte Bilder
- Prüfungsumfang: Amtsblatt-Verfügung 29/2024

Antwort A ist in der Quelle immer richtig. Die Quelldatei nennt keine
Fehlerpunkte je Frage; der Wert 1 stammt aus der Verfügung und steht als
`defaultPoints` in den Katalogmetadaten.

### Neu importieren

```sh
hrt-catalog-import --source <Pfad zu Pruefungsfragen> --output data/catalogs
hrt-catalog-tool --info data/catalogs/*.xml
```

### Katalogformat

Unverschlüsseltes, handeditierbares XML gemäß `schema/catalog.xsd`. Es lässt
sich in jedem Texteditor öffnen, gezielt durchsuchen und ergänzen. Die
Prüfungsumfang-Angaben liegen am Prüfungsteil:

```xml
<part id="DE-A-teil5-a" title="A (Technische Kenntnisse, Klasse A) …" order="5">
  <selection questions="25" errorPoints="6" passScore="19" minutes="60"/>
  <section id="DE-A-teil5-a-abschnitt1" title="…" order="1">
    <chapters>
      <chapter id="…" title="…" order="1">
        <question id="DE-A-a-0123" type="single">
          <text>…</text>
          <answers>
            <answer id="DE-A-a-0123-a" correct="true">…</answer>
          </answers>
        </question>
      </chapter>
    </chapters>
  </section>
</part>
```

Lizenzen: Programm GPL-3.0-or-later, Fragentexte und Bilder
DL-DE-BY-2.0 mit verpflichtendem `<attribution>`.

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

## Entwicklungstand

Stand: alle 135 Tests in 11 Programmen sind grün. Die Anwendung ist
funktionsfähig; offen sind vor allem die Verpackung in weitere Linux-Formate
und die Veröffentlichung.

Erledigt:

* Katalogformat als Klartext-XML mit Schema
* Datenmodell, XML-Leser, XML-Schreiber, semantischer Validator
* Kommandozeilenwerkzeuge `hrt-catalog-tool` und `hrt-catalog-import`
* Import des amtlichen BNetzA-Fragenkatalogs: 2502 katalogisierte Fragen für
  N, E und A (571 / 1034 / 1750) inklusive 687 Bildern, schemakonform geprüft
* die fünf Prüfungsteile V, B, N, E, A mit Prüfungsumfang nach
  Amtsblatt-Verfügung 29/2024
* Lernlogik mit Leitner-Ordnern, Fälligkeit und Selbsteinschätzung
* gewichtete Fragenauswahl ohne Ersetzung und immer gemischte Antwortreihenfolge
* Prüfungssimulation: 25 Fragen je Teil, Bestehensschwelle 19 Punkte je Teil,
  Zeitlimit 45 Minuten bzw. 60 Minuten für Klasse A, Auswertung je Prüfungsteil
  mit Hinweis auf die mündliche Nachprüfung
* Oberfläche mit Katalogbrowser, Lernansicht und Prüfungsansicht, SVG-Frage- und
  Antwortbildern
* Lernansicht: Stand je Prüfungsteil, Leitner-Verteilung, Übungsrunde über die
  offenen Fragen mit Erhebung der Selbsteinschätzung
* Lernfortschritt wird je Katalog gespeichert und beim Start wiederhergestellt,
  gebunden an den Prüfsummen-Abdruck der Katalogdatei
* Formeln der 219 LaTeX-Fragen werden dargestellt
* Testsuite (121 Tests) und GitHub-Actions-Build für Linux und Windows 10/11

### Lernfortschritt

Der Fortschritt liegt je Prüfung im Anwendungsdatenverzeichnis, nicht im
Katalog, weil die Kataloge bearbeitbar und Teil der Auslieferung sind:

| System | Pfad |
|--------|------|
| Linux | `~/.local/share/HamRadioTrainer2026/fortschritt-de-e.json` |
| Windows | `%APPDATA%\HamRadioTrainer2026\fortschritt-de-e.json` |

Die Datei wird nach jeder ausgewerteten Prüfung und beim Beenden atomar
geschrieben. Sie enthält Leitner-Ordner, Trefferquote, Fälligkeit,
Selbsteinschätzung und den SHA-256-Abdruck der Katalogdatei, zu der sie gehört.

**Wird der Katalog von Hand geändert, gilt der Fortschritt als veraltet und wird
verworfen.** Das ist beabsichtigt: nach dem Umschreiben einer Frage wäre ihr alter
Ordner sonst eine falsche Aussage darüber, wie gut sie beherrscht wird. Nach dem
Verwerfen baut sich der Fortschritt neu auf. Lernstände zu Fragen, die es im
Katalog nicht mehr gibt, werden beim Laden ebenfalls verworfen.

Unter *Datei* lässt sich der Fortschritt bewusst zurücksetzen.

### Lernen und Üben

Der Tab *Lernen* zeigt den Stand und lädt zum Üben ein:

* **Zusammenfassung** – beantwortete Fragen, Trefferquote, fällige Wiederholungen,
  nie gesehene, wackelige und gefestigte Fragen
* **Leitner-Verteilung** – wie viele beantwortete Fragen in welchem Ordner liegen
* **Fortschritt je Prüfungsteil** – Fragen, beantwortet, gefestigt, wackelig und
  Trefferquote für V, B, N, E und A
* **Übungsrunde** – die schwachen Fragen zuerst, filterbar nach „nur fällige“,
  „nur wackelige“ und Prüfungsteil

Nach jeder Antwort fragt die Runde, **wie sicher die Antwort war**. Das ist der
Punkt, an dem eine Prüfung nicht weiterkommt: eine Prüfung enthält keine
Selbstauskunft und speichert deshalb ein neutrales Ergebnis. Erst die
Übungsrunde erhebt sie, und damit sinkt eine richtig geratene Frage schneller
zurück als eine sicher gewusste.

Ohne Angabe fällt der Wert „unsicher“ an: die bereits gemeldete Selbsteinschätzung
bleibt erhalten, statt überschrieben zu werden.

Wird „nur fällige“ gewählt, kommen die fälligen Wiederholungen vor den noch nie
gesehenen Fragen. Sonst würde eine kurze Runde vollständig aus neuen Fragen
bestehen und das Filterziel verfehlt.

### Formeln

219 der 1750 Fragen der Klasse A enthalten Formeln. Der Vorrat wurde in den
Katalogen selbst gemessen und ist klein:

| | |
|---|---|
| Formelstellen | 355 |
| Umgebungen (`\begin{…}`) | keine |
| Befehle | `\lambda`, `\Omega`, `\eta`, `\varphi`, `\text`, `\textrm`, `\cdot`, `\circ`, `\frac`, `\dfrac`, `\sqrt`, `\left`, `\right` |

Damit rechtfertigt der Bestand keine vollständige TeX-Installation. `LatexFormatter`
bildet genau diese Befehle auf Qt-Richtext ab – griechische Buchstaben als
Unicode, `^{…}` und `_{…}` als Hoch- bzw. Tiefstellung, Brüche als
`Zähler⁄Nenner` und `\sqrt` als `√(…)`. Das wiegt deutlich leichter als KaTeX
mit QtWebEngine und hängt nicht an einer zusätzlichen Laufzeitabhängigkeit.

Es werden drei Delimiter unterstützt: `$…$`, `$$…$$` und `\[…\]`.

Ein nicht abgedeckter Befehl wird **nicht** entfernt, sondern bleibt sichtbar
stehen. Eine Formel, die nicht dargestellt werden kann, soll einen sichtbaren
Riss zeigen und nicht stillschweigend fehlen.

Der Test `everyFormulaOfTheCatalogsRenders` prüft alle 356 Formeln der
ausgelieferten Kataloge darauf, dass keinerlei Rückstriche übrig bleiben.

Drei Stellen der Quelle schreiben Markup außerhalb jeder Formel (`4 \mOhm`,
`4,7 \kiloOhm`, ein `\glqq`). Der Import schreibt sie als `mΩ`, `kΩ` und
`„` in den Katalog, damit die von Hand zu bearbeitenden Dateien lesbar bleiben.

Noch offen:
* Katalogpflege in der Anwendung
* mündliche Nachprüfung als eigener Prüfungsteil-Generator
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

## Bauen und Verteilen

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build
```

Zum Ausprobieren ohne Installation genügt
`cmake --install build`; das Programm findet seine Kataloge danach von selbst.
Wird kein Katalog angegeben, lädt es den zuletzt benutzten oder den ersten
gefundenen.

### Installationspakete

| Ziel | Ergebnis |
|---|---|
| Windows | **eine `.exe`** (NSIS, mitgelieferte Qt-Laufzeit) |
| Debian, Ubuntu und Verwandte | `.deb` |
| Fedora, RHEL, CentOS Stream | `.rpm` |
| openSUSE, SuSE | `.rpm` läuft meist, ist dort aber nicht offiziell unterstützt |
| Arch Linux | **kein Binärpaket – siehe unten** |
| alles andere | `.deb`/`.rpm` als Ausgangspunkt, sonst ein noch nicht fertiges AppImage |

Gebaut wird mit CPack, die Paketabhängigkeiten werden dabei aus den wirklich
benutzten Bibliotheken ermittelt. Einzelheiten, auch zu den Grenzen je
Distribution, stehen in [`packaging/README.md`](packaging/README.md).

### Verteilen auf Arch

**Arch lässt sich hier nicht fertig ausliefern, und das ist keine
Nachlässigkeit, sondern so vorgesehen.** Arch installiert ausschließlich aus
dem AUR, und ein AUR-Eintrag ist eine `PKGBUILD`, die aus dem Quelltext baut –
kein fertiges Binary. Jemand, der Arch unterstützen will, muss dafür ein
AUR-Konto haben und den Eintrag fortlaufend pflegen; das kann dieses Projekt
nicht übernehmen.

Geliefert wird alles Übrige: [`packaging/PKGBUILD`](packaging/PKGBUILD) ist
fertig, das Quellarchiv muss als Release mit Prüfsumme bereitstehen. Wer Arch
ohne AUR bedienen möchte, baut selbst:

```sh
git clone https://aur.archlinux.org/hamradiotrainer-2026.git
cd hamradiotrainer-2026
makepkg -si
```

### Noch offen

* Ein AppImage für die Distributionen ohne passendes `.deb` oder `.rpm`
  (Void, Gentoo, Solus, Slackware) ist nicht eingerichtet. Es braucht ein
  zusätzliches Werkzeug (`appimagetool` oder `linuxdeployqt`) beim Bauen und
  liefert ohne Installation lauffähige Dateien auf jedem x86-64-Linux.

## Geschichte

* Vorgänger *HamRadioTrainer* von DM1OLI, erschienen 2011–2015, inzwischen
  eingestellt. Kataloge im proprietären Format `.etq`, verschlüsselt.
* Dieses Projekt: Neuanfang 2026 unter GPL-3.0-or-later.