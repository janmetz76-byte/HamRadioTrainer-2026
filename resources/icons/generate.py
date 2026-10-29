#!/usr/bin/env python3
"""Erzeugt die Icondateien unter resources/icons/.

Die Icons sind handgezeichnete Pfade auf einem 24er-Raster, 1,7 Strichstaerke,
abgerundete Enden. Sie liegen bewusst als Quelle hier und nicht nur als Ergebnis
im Repository: wer ein Icon aendern will, aendert eine Zeile in ICONS und
laesst dieses Skript laufen. Dadurch bleiben alle Icons buendig - gleiche
Rastermasse, gleiche Strichstaerke, gleiche Ecken.

Jedes Icon entsteht zweimal, einmal mit dunkler und einmal mit heller Tinte.
Qt kann eine SVG-Datei keine Farbe von aussen geben, und eine hellgraue Taste
verschwindet auf dunklem Grund. AppIcons waehlt deshalb zur Laufzeit die
passende Variante.

    python3 resources/icons/generate.py
"""

import pathlib
import subprocess
import tempfile

# (Name, Pfadkoerper)
# "f" = Fuellung (nur fuer kleine Akzente), "s" = Strich
ICONS = {
    # ---- Menue Datei ----------------------------------------------------
    "assistant": """
        <circle class="s" cx="12" cy="12" r="8.6"/>
        <path class="s" d="M15.8 8.2 L13.9 13.9 L8.2 15.8 L10.1 10.1 Z"/>
    """,
    "open": """
        <path class="s" d="M3.6 19.6 V6.4 a1 1 0 0 1 1-1 h4.1 l2 2.3 h8.7 a1 1 0 0 1 1 1 v1.5"/>
        <path class="s" d="M3.6 19.6 l2.5-7.1 h14.3 l-2.5 7.1 Z"/>
    """,
    "reload": """
        <path class="s" d="M20 12 a8 8 0 1 1-2.7-6"/>
        <path class="s" d="M20.4 3.2 v3.9 h-3.9"/>
    """,
    "update": """
        <path class="s" d="M4 15.4 v3.6 a1 1 0 0 0 1 1 h14 a1 1 0 0 0 1-1 v-3.6"/>
        <path class="s" d="M12 3.8 v10.4"/>
        <path class="s" d="M8.4 10.6 L12 14.2 L15.6 10.6"/>
    """,
    "reset-progress": """
        <path class="s" d="M4.4 9.4 A8.2 8.2 0 1 1 4 13.6"/>
        <path class="s" d="M4.4 4.6 v4.8 h4.8"/>
        <path class="s" d="M9.2 19.6 v-3.8 M12 19.6 v-6.2 M14.8 19.6 v-4.6"/>
    """,
    "quit": """
        <path class="s" d="M12 3.6 v7.9"/>
        <path class="s" d="M7.7 6.9 a6.6 6.6 0 1 0 8.6 0"/>
    """,
    # ---- Menue Pruefbericht --------------------------------------------
    "report": """
        <path class="s" d="M9 4.4 H5.6 a1 1 0 0 0-1 1 v14 a1 1 0 0 0 1 1 h12.8 a1 1 0 0 0 1-1 v-14 a1 1 0 0 0-1-1 H15"/>
        <rect class="s" x="9" y="2.8" width="6" height="3.4" rx="1"/>
        <path class="s" d="M8.4 12.4 l2.3 2.3 L15.9 9.5"/>
    """,
    "recheck": """
        <path class="s" d="M8.6 3.4 h-3 a1 1 0 0 0-1 1 v15.2 a1 1 0 0 0 1 1 h6.2"/>
        <rect class="s" x="8.6" y="2.2" width="6" height="3.4" rx="1"/>
        <path class="s" d="M8.6 12.6 h3.4 M8.6 16.4 h2.6"/>
        <circle class="s" cx="16.4" cy="16.4" r="4.2"/>
        <path class="s" d="M19.6 19.6 L22 22"/>
    """,
    "copy": """
        <rect class="s" x="8.4" y="8.4" width="11.8" height="11.8" rx="1.4"/>
        <path class="s" d="M15.6 8.4 V5.8 a1.4 1.4 0 0 0-1.4-1.4 H5.8 a1.4 1.4 0 0 0-1.4 1.4 v8.4 a1.4 1.4 0 0 0 1.4 1.4 h2.6"/>
    """,
    "about": """
        <circle class="s" cx="12" cy="12" r="8.6"/>
        <path class="s" d="M12 11.1 v5.4"/>
        <circle class="f" cx="12" cy="7.7" r="1.05"/>
    """,
    # ---- Ansichten ------------------------------------------------------
    "view-catalog": """
        <path class="s" d="M5.4 3.6 h13.2 a1 1 0 0 1 1 1 v15.8 h-15.2 V4.6 a1 1 0 0 1 1-1 Z"/>
        <path class="s" d="M8.6 8.2 h8 M8.6 12 h8 M8.6 15.8 h5"/>
    """,
    "view-learn": """
        <path class="s" d="M2.8 9.2 L12 5.2 l9.2 4 -9.2 4 Z"/>
        <path class="s" d="M6.8 11.4 v4.4 c0 1.5 2.4 2.7 5.2 2.7 s5.2-1.2 5.2-2.7 v-4.4"/>
        <path class="s" d="M20.4 9.6 v5"/>
    """,
    "view-exam": """
        <path class="s" d="M6 3.4 h8.4 L19 8 v12.6 H6 Z"/>
        <path class="s" d="M14.2 3.6 V8 H19"/>
        <path class="s" d="M8.8 11.6 h4.4 M8.8 15 h3"/>
        <path class="s" d="M12.6 17.6 L17.2 13 l1.9 1.9 -4.6 4.6 -2.5.6 Z"/>
    """,
    "view-statistics": """
        <path class="s" d="M3.4 20.6 h17.2"/>
        <path class="s" d="M6.2 20.4 v-5.6 M11 20.4 v-10.4 M15.8 20.4 v-7.4
                           M20 20.4 v-13"/>
    """,
    # ---- Ansichtsleiste --------------------------------------------------
    "collapse": """
        <path class="s" d="M14.5 7.5 L9.5 12 L14.5 16.5"/>
        <path class="s" d="M20 5.4 v13.2"/>
    """,
    "expand": """
        <path class="s" d="M9.5 7.5 L14.5 12 L9.5 16.5"/>
        <path class="s" d="M4 5.4 v13.2"/>
    """,
    # ---- Lernen und Pruefen --------------------------------------------
    "play": """
        <circle class="s" cx="12" cy="12" r="8.6"/>
        <path class="f" d="M10 8.4 L16 12 L10 15.6 Z"/>
    """,
    "confident": """
        <circle class="s" cx="12" cy="12" r="8.6"/>
        <path class="s" d="M8 12.3 l2.7 2.7 L16.1 9.6"/>
    """,
    "unsure": """
        <circle class="s" cx="12" cy="12" r="8.6"/>
        <path class="s" d="M10 9.7 a2.1 2.1 0 1 1 2.7 2.3 c-.6.3-.7.9-.7 1.5"/>
        <circle class="f" cx="12" cy="16.4" r="1.05"/>
    """,
    "guessed": """
        <rect class="s" x="4.2" y="4.2" width="15.6" height="15.6" rx="3.6"/>
        <circle class="f" cx="9" cy="9" r="1.25"/>
        <circle class="f" cx="15" cy="15" r="1.25"/>
        <circle class="f" cx="12" cy="12" r="1.25"/>
    """,
    "next": """
        <path class="s" d="M4.6 12 h11.8"/>
        <path class="s" d="M13.4 7.2 L18.2 12 L13.4 16.8"/>
    """,
    "skip": """
        <path class="s" d="M4.4 12 h8.6"/>
        <path class="s" d="M10.6 7.6 L15 12 L10.6 16.4"/>
        <path class="s" d="M19.2 6.6 v10.8"/>
    """,
    "back": """
        <path class="s" d="M19.4 12 H7.6"/>
        <path class="s" d="M10.6 7.2 L5.8 12 L10.6 16.8"/>
    """,
    "evaluate": """
        <path class="s" d="M4.4 6.4 h6.2 M4.4 12 h6.2 M4.4 17.6 h3.8"/>
        <path class="s" d="M13.4 15.4 l2.7 2.7 L21.4 12.8"/>
    """,
}

RASTER = 24
STRICH = 1.7

# Tinte fuer hellen und dunklen Grund. Beide Werte sind ueber den
# Systemfarben gewaehlt, nicht geraten: sie stehen im Kontrastverhaeltnis von
# mindestens 4.5:1 zu ihrem jeweiligen Hintergrund.
TINTE_HELL = "#2f3437"
TINTE_DUNKEL = "#e3e8ea"


def stufen_symbol(gefuellt: int) -> str:
    """Baut das Bild einer Lernstufe.

    Fuenf Balken stehen auf einer Grundlinie, jeder einen Schritt hoeher als
    der vorige. Gefuellt sind so viele, wie die Stufe zaehlt: Stufe 1 hat
    einen gefuellten Balken, "gefestigt" alle fuenf.

    Das Bild waechst also mit der Stufe, statt nur den Namen daneben zu
    setzen - die Leiste laesst sich auf einen Blick lesen, ohne sechs
    Wortmarken zu vergleichen. Eine Treppe statt einer Ampel, weil die
    Stufen keine Reihenfolge von gut und schlecht haben: eine falsche Antwort
    schiebt zurueck, sie macht den Lernenden nicht schlechter.
    """
    teile = []
    for i in range(5):
        x = 3.4 + i * 3.8
        hoehe = 3.6 + i * 3.1
        y = 19.4 - hoehe
        art = "f" if i < gefuellt else "s"
        teile.append('        <rect class="%s" x="%.1f" y="%.1f" width="2.6"'
                     ' height="%.1f" rx="0.9"/>' % (art, x, y, hoehe))
    return "\n".join(teile)


# ---- Lernstufen ----------------------------------------------------------
# Stufe 0 bis 5, erzeugt statt handgeschrieben: sie unterscheiden sich nur in
# der Zahl der gefuellten Balken. Von Hand sechs fast gleiche Bilder zu
# zeichnen hiesse, sie sechsmal gleich zu zeichnen und einmal falsch.
STATUS_BALKEN = {
    "status-new": 0,
    "status-low": 1,
    "status-mid": 3,
    "status-high": 5,
}

for _stufe in range(6):
    ICONS["stufe-%d" % _stufe] = stufen_symbol(_stufe)

for _name, _balken in STATUS_BALKEN.items():
    ICONS[_name] = stufen_symbol(_balken)

for _name, _balken in STATUS_BALKEN.items():
    ICONS[_name] = stufen_symbol(_balken)

def kopf(titel: str, tinte: str) -> str:
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d">\n'
        '  <title>%s</title>\n'
        '  <g fill="none" stroke="%s" stroke-width="%s" stroke-linecap="round"'
        ' stroke-linejoin="round">\n' % (RASTER, RASTER, RASTER, RASTER, titel, tinte, STRICH)
    )


# Die Stufen tragen Farbe: rot angefaenglich bis gruen am Ende, wie ein
# Empfaenger. Jede Stufe hat zwei Farben, eine fuer hellen und eine fuer
# dunklen Grund - nicht dieselbe in zwei Toenen, sondern zwei nach dem
# Kontrastverhaeltnis zum jeweiligen Hintergrund gewaehlte Toene. Ein
# einzelnes Rot ist auf weissem Grund gut sichtbar und auf schwarzem fast
# weg; umgekehrt gilt es fuer ein helles Gelb.
#
# Stufe 0 bleibt unfaerbig: "neu" ist kein schlechtes Ergebnis, sondern
# schlicht keins. Eine rote Ampel neben einer noch nie gesehenen Frage waere
# eine Bewertung, die es nicht gibt.
# Die Toene liegen alle ueber 3:1 Kontrast auch gegen die dunkelste
# Zellenfarbe. Angefangen ist Rot bewusst kraeftig, damit es auf hellblauem
# Grund nicht nur auffaellt, sondern lesbar bleibt.
STUFEN_HELL = {
    1: "#b3261e",  # rot
    2: "#8f4707",  # orange
    3: "#775700",  # bernstein
    4: "#486507",  # gelbgruen
    5: "#256b28",  # gruen
}
STUFEN_DUNKEL = {
    1: "#ff6b5e",
    2: "#ff9d4d",
    3: "#e8c33f",
    4: "#a3d45c",
    5: "#6ee787",
}


# Die Statussymbole tragen dieselben Farben wie die Stufen, aber eine andere
# Zahl gefuellter Balken: hier bedeutet ein Balken "ein Zehntel des Teils ist
# gefestigt", dort "eine von fuenf Stufen". Die Zahl ist also nicht
# vergleichbar, die Farbe schon - das ist Absicht, sie gehoeren zur selben
# Sprache.



def fuellung(name: str, tinte: str, dunkel: bool) -> str:
    """Die Farbe, mit der die Akzente eines Bildes gefuellt werden.

    Gewoehnlich die Tinte des Grundes, damit ein Icon zum Rest des Satzes
    passt. Die Stufen sind der eine Fall, der eine eigene Farbe traegt -
    sie sollen als Reihe lesbar sein, nicht nur einzeln richtig.
    """
    if name in STATUS_BALKEN:
        tabelle = STUFEN_DUNKEL if dunkel else STUFEN_HELL
        # "neu" faerbt sich nicht: nichts gelernt zu haben ist kein rotes
        # Ergebnis, sondern noch keins.
        return tabelle.get(STATUS_BALKEN[name], tinte)
    if name.startswith("stufe-"):
        stufe = int(name.split("-", 1)[1])
        tabelle = STUFEN_DUNKEL if dunkel else STUFEN_HELL
        return tabelle.get(stufe, tinte)
    return tinte


def schreiben(verzeichnis: pathlib.Path) -> None:
    for name, koerper in ICONS.items():
        for endung, tinte in (("", TINTE_HELL), ("-dark", TINTE_DUNKEL)):
            dunkel = bool(endung)
            inhalt = kopf(name, tinte)
            inhalt += koerper.strip("\n") + "\n"
            inhalt += "  </g>\n</svg>\n"
            # Die Klassen in das uebersetzen, was SVG braucht. Der Strich
            # erbt Farbe und Staerke von der Gruppe; die Akzente brauchen
            # eine eigene Fuellung, sonst waeren sie unsichtbar.
            farbe = fuellung(name, tinte, dunkel)
            inhalt = inhalt.replace('class="s"', "").replace(
                'class="f"', 'fill="%s" stroke="none"' % farbe)
            ziel = verzeichnis / ("%s%s.svg" % (name, endung))
            ziel.write_text(inhalt, encoding="utf-8")


def schreibe_ico(verzeichnis: pathlib.Path) -> None:
    """Erzeugt app.ico fuer Windows.

    Windows nimmt das Symbol aus der Programmdatei selbst, nicht aus einer
    Datei daneben. Ohne sie zeigt der Explorer ein Platzhalterbild und die
    Taskleiste gar keins - gleichgueltig, wie gut das SVG aussieht. Das .ico
    enthaelt dieselben Stufen wie das SVG, damit es in der Taskleiste scharf
    bleibt und nicht von Windows hochskaliert werden muss.

    Es braucht rsvg-convert und ImageMagick; sind beide nicht da, entsteht
    keine Datei, und CMake warnt beim Einrichten und baut ohne Symbol weiter.
    """
    quelle = verzeichnis / "app.svg"
    if not quelle.exists():
        return
    try:
        subprocess.run(["rsvg-convert", "--version"], check=True,
                       capture_output=True)
        subprocess.run(["convert", "-version"], check=True, capture_output=True)
    except (OSError, subprocess.CalledProcessError):
        print("rsvg-convert oder ImageMagick fehlt - app.ico wurde nicht erzeugt")
        return

    with tempfile.TemporaryDirectory() as tmp:
        stufen = []
        for seite in (16, 24, 32, 48, 64, 128, 256):
            ziel = pathlib.Path(tmp) / ("%d.png" % seite)
            subprocess.run(["rsvg-convert", "-w", str(seite), "-h", str(seite),
                            str(quelle), "-o", str(ziel)], check=True)
            stufen.append(str(ziel))
        subprocess.run(["convert"] + stufen
                       + ["-define", "icon:auto-resize=256,128,64,48,32,24,16",
                          str(verzeichnis / "app.ico")], check=True,
                       capture_output=True)
    print("app.ico geschrieben")


def schreibe_icns(verzeichnis: pathlib.Path) -> None:
    """Erzeugt app.icns fuer macOS.

    Der Finder und das Dock nehmen das Symbol aus dem Bundle
    (Contents/Resources), nicht aus der Programmdatei; ein SVG liest macOS
    dort nicht. Wie beim .ico wird jede Stufe einzeln aus dem SVG gerechnet,
    statt eine grosse Fassung herunterzuskalieren.

    Es braucht rsvg-convert und Pillow; fehlt eins davon, entsteht keine
    Datei, und das Bundle zeigt das Standardsymbol.
    """
    quelle = verzeichnis / "app.svg"
    if not quelle.exists():
        return
    try:
        from PIL import Image
        subprocess.run(["rsvg-convert", "--version"], check=True,
                       capture_output=True)
    except (ImportError, OSError, subprocess.CalledProcessError):
        print("rsvg-convert oder Pillow fehlt - app.icns wurde nicht erzeugt")
        return

    with tempfile.TemporaryDirectory() as tmp:
        stufen = []
        for seite in (1024, 512, 256, 128, 64, 32, 16):
            ziel = pathlib.Path(tmp) / ("%d.png" % seite)
            subprocess.run(["rsvg-convert", "-w", str(seite), "-h", str(seite),
                            str(quelle), "-o", str(ziel)], check=True)
            stufen.append(Image.open(ziel))
        stufen[0].save(verzeichnis / "app.icns", format="ICNS",
                       append_images=stufen[1:])
    print("app.icns geschrieben")


if __name__ == "__main__":
    hier = pathlib.Path(__file__).parent
    schreiben(hier)
    print("%d Icons geschrieben" % (len(ICONS) * 2))
    schreibe_ico(hier)
    schreibe_icns(hier)
