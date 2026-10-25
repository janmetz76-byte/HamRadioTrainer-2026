# hrt2026.de

Die Webseite zu HamRadioTrainer 2026: reines HTML5 mit CSS und etwas
JavaScript, ohne PHP, ohne Datenbank, ohne Inhalte fremder Server.

## Hochladen

Den Inhalt dieses Ordners (ohne diese README) in das Wurzelverzeichnis von
hrt2026.de kopieren, zum Beispiel per SFTP oder

    rsync -av --exclude README.md webseite/ benutzer@server:/pfad/zu/hrt2026.de/

## Vor der Veröffentlichung

- `impressum.html` und `datenschutz.html`: die gelb markierten Angaben
  (Name, Anschrift, E-Mail, Hoster, Speicherdauer der Server-Protokolle)
  ausfüllen.
- Die Download-Knöpfe führen auf
  `https://github.com/janmetz76-byte/HamRadioTrainer-2026/releases/latest`.
  Das funktioniert erst, wenn das Repository öffentlich ist und dort ein
  Release mit den Paketen veröffentlicht wurde.

## Inhalt

| Datei | Zweck |
|---|---|
| `index.html` | die Seite selbst |
| `impressum.html`, `datenschutz.html` | Rechtstexte |
| `css/style.css` | Gestaltung, hell und dunkel nach Systemeinstellung |
| `js/site.js` | Menü, Reiter der Bildschirmfotos, Vergrößerung |
| `img/logo.svg` | Programmsymbol (aus `resources/icons/app.svg`) |
| `img/shots/` | Bildschirmfotos, je groß und `-klein` als Vorschau |
