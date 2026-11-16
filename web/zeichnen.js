// Zeichenwerkzeuge im Kursdiagramm (Feature F1, Beschreibung in Features/F1_Zeichenwerkzeuge.md).
// Trendlinie, Strahl, waagrechte Linie, Zone und Fibonacci-Retracement; Auswählen, Löschen, Rückgängig.
// Die Zeichnungen werden im Browser gespeichert (localStorage), getrennt je Wertpapier.
// Jeder Punkt wird als (Datum, Preis) gespeichert, nicht in Pixeln: So bleiben die Zeichnungen an der richtigen
// Stelle, wenn sich die Fenstergröße, der Zeitraum oder der Preisbereich des Diagramms ändert.
//
// Zusammenspiel mit app.js:
//   Zeichnen.init()                    einmal beim Start: Werkzeugleiste und Tasten verbinden
//   Zeichnen.setzeWertpapier(symbol)   beim Laden: Zeichnungen dieses Wertpapiers holen
//   Zeichnen.zeichne(svg, kontext)     nach jedem Zeichnen des Kursdiagramms (kontext = Umrechnung Pixel <-> Daten)
//   Zeichnen.bewege / klick / verlasse Mausereignisse aus dem Kursdiagramm
//   Zeichnen.aktiv()                   true, wenn gerade ein Zeichenwerkzeug gewählt ist

// Strenger Modus
"use strict";

// Alles in einer Funktion, die sofort ausgeführt wird: Nur das Objekt "Zeichnen" ist von außen sichtbar,
// alle Hilfsfunktionen und der Zustand bleiben intern
const Zeichnen = (() => {
  // Anzahl der Punkte je Werkzeug (zugleich die Liste der erlaubten Typen)
  const PUNKTE = { trend: 2, strahl: 2, waagrecht: 1, zone: 2, fibonacci: 2 };
  // Fibonacci-Stufen (Anteil des Wegs vom zweiten zum ersten Punkt)
  const FIB_STUFEN = [0, 0.236, 0.382, 0.5, 0.618, 0.786, 1];
  // Wie nah (in Pixeln) ein Klick an einer Zeichnung sein muss, um sie auszuwählen
  const TREFFER_PX = 6;
  // Wie nah (in Pixeln) ein Klick an Open/High/Low/Close sein muss, damit der Magnet einrastet
  const MAGNET_PX = 15;
  // Höchstens so viele Schritte lassen sich rückgängig machen
  const MAX_VERLAUF = 50;
  // Kurze Anleitung je Werkzeug für die Werkzeugleiste
  const ANLEITUNG = {
    trend: "Trendlinie: Startpunkt klicken, dann Endpunkt. Esc bricht ab.",
    strahl: "Strahl: Startpunkt klicken, dann einen zweiten Punkt für die Richtung. Esc bricht ab.",
    waagrecht: "Waagrechte Linie: auf den gewünschten Preis klicken. Esc bricht ab.",
    zone: "Zone: eine Ecke klicken, dann die gegenüberliegende Ecke. Esc bricht ab.",
    fibonacci: "Fibonacci: Startpunkt (z. B. Tief) klicken, dann Endpunkt (z. B. Hoch). Esc bricht ab.",
  };

  // Zustand
  const z = {
    // Gewähltes Werkzeug: "auswahl" oder einer der Typen aus PUNKTE
    werkzeug: "auswahl",
    // Gespeicherte Zeichnungen des aktuellen Wertpapiers: { id, typ, punkte: [{ datum, preis }, ...] }
    zeichnungen: [],
    // Angefangene Zeichnung (nach dem ersten Klick): { typ, punkte: [...] } oder null
    angefangen: null,
    // Punkt unter der Maus für die Vorschau oder null
    vorschau: null,
    // id der ausgewählten Zeichnung oder null
    ausgewaehlt: null,
    // Frühere Zustände für "Rückgängig" (als JSON-Text, damit es echte Kopien sind)
    verlauf: [],
    // Aktuelles Wertpapier
    symbol: null,
    // Ob der Browser-Speicher funktioniert
    speicherOk: true,
    // Umrechnung Pixel <-> Daten des zuletzt gezeichneten Diagramms (oder null)
    kontext: null,
    // SVG-Gruppe, in der die Zeichnungen liegen
    ebene: null,
    // Nächste freie id
    naechsteId: 1,
    // "Alle löschen" wurde einmal geklickt und wartet auf die Bestätigung
    alleLoeschenBereit: false,
  };
  // Zeitgeber, der die Bestätigung von "Alle löschen" wieder zurücksetzt
  let alleLoeschenZeitgeber = 0;

  // Kurzschreibweise für ein Element der Seite
  const el = (id) => document.getElementById(id);
  // Preisformat wie in app.js
  const zahl = new Intl.NumberFormat("de-DE", { minimumFractionDigits: 2, maximumFractionDigits: 2 });

  // ---------------------------------------------------------------- Speichern im Browser

  // Schlüssel im localStorage je Wertpapier
  function schluessel(symbol) {
    // z. B. "marketlens.zeichnungen.IBM"
    return `marketlens.zeichnungen.${symbol}`;
  }

  // Prüft eine gespeicherte Zeichnung: bekannter Typ, passende Anzahl Punkte, gültiges Datum, endlicher Preis.
  // Daten aus dem Speicher könnten von Hand verändert oder von einer älteren Version sein.
  function istGueltig(eintrag) {
    // Muss ein Objekt mit bekanntem Typ sein
    if (typeof eintrag !== "object" || eintrag === null || !Object.prototype.hasOwnProperty.call(PUNKTE, eintrag.typ)) {
      // Unbekannt
      return false;
    }
    // Liste der Punkte mit der richtigen Länge
    if (!Array.isArray(eintrag.punkte) || eintrag.punkte.length !== PUNKTE[eintrag.typ]) {
      // Falsche Form
      return false;
    }
    // Jeder Punkt: Datum "YYYY-MM-DD" und endliche Zahl als Preis
    return eintrag.punkte.every((p) => p !== null && typeof p === "object" && typeof p.datum === "string" &&
      /^\d{4}-\d{2}-\d{2}$/.test(p.datum) && Number.isFinite(p.preis));
  }

  // Lädt die Zeichnungen eines Wertpapiers; ungültige Einträge werden verworfen
  function ladeAusSpeicher(symbol) {
    // Gespeicherter Text
    let text = null;
    // Zugriff kann scheitern (z. B. privates Fenster mit gesperrtem Speicher)
    try {
      // Lesen
      text = window.localStorage.getItem(schluessel(symbol));
      // Speicher funktioniert
      z.speicherOk = true;
    } catch (e) {
      // Ohne Speicher weiterarbeiten
      z.speicherOk = false;
      // Keine Zeichnungen
      return [];
    }
    // Nichts gespeichert
    if (!text) return [];
    // Inhalt lesen
    let daten;
    try {
      // JSON in Objekte umwandeln
      daten = JSON.parse(text);
    } catch (e) {
      // Kaputter Inhalt: so tun, als wäre nichts gespeichert
      return [];
    }
    // Muss eine Liste sein
    if (!Array.isArray(daten)) return [];
    // Nur gültige Einträge übernehmen, jeweils mit neuer id
    return daten.filter(istGueltig).map((d) => ({
      id: z.naechsteId++,
      typ: d.typ,
      punkte: d.punkte.map((p) => ({ datum: p.datum, preis: p.preis })),
    }));
  }

  // Speichert die Zeichnungen des aktuellen Wertpapiers (ohne die internen ids)
  function speichere() {
    // Ohne Wertpapier nichts zu tun
    if (!z.symbol) return;
    // Schreiben kann scheitern (Speicher voll oder gesperrt)
    try {
      // Nur Typ und Punkte speichern
      window.localStorage.setItem(schluessel(z.symbol), JSON.stringify(z.zeichnungen.map((d) => ({ typ: d.typ, punkte: d.punkte }))));
      // Hat geklappt
      z.speicherOk = true;
    } catch (e) {
      // Hinweis in der Werkzeugleiste
      z.speicherOk = false;
    }
    // Leiste aktualisieren (Hinweis, Knöpfe)
    aktualisiereLeiste();
  }

  // ---------------------------------------------------------------- Rückgängig

  // Merkt den aktuellen Stand, bevor er geändert wird
  function merke() {
    // Kopie als Text
    z.verlauf.push(JSON.stringify(z.zeichnungen));
    // Begrenzen: ältesten Stand verwerfen
    if (z.verlauf.length > MAX_VERLAUF) z.verlauf.shift();
  }

  // Stellt den letzten gemerkten Stand wieder her
  function rueckgaengig() {
    // Nichts gemerkt
    if (z.verlauf.length === 0) return;
    // Letzten Stand holen
    z.zeichnungen = JSON.parse(z.verlauf.pop());
    // Auswahl aufheben (die ausgewählte Zeichnung gibt es eventuell nicht mehr)
    z.ausgewaehlt = null;
    // Speichern und neu zeichnen
    speichere();
    zeichneEbene();
  }

  // ---------------------------------------------------------------- Werkzeugleiste

  // Wechselt das Werkzeug; bricht eine angefangene Zeichnung ab
  function setzeWerkzeug(werkzeug) {
    // Werkzeug übernehmen
    z.werkzeug = werkzeug;
    // Angefangenes verwerfen
    z.angefangen = null;
    z.vorschau = null;
    // Beim Zeichnen keine Auswahl
    if (werkzeug !== "auswahl") z.ausgewaehlt = null;
    // Anzeige aktualisieren
    aktualisiereLeiste();
    zeichneEbene();
  }

  // Bringt Knöpfe und Hinweis der Werkzeugleiste auf den aktuellen Stand
  function aktualisiereLeiste() {
    // Alle Werkzeug-Knöpfe
    for (const knopf of document.querySelectorAll("#werkzeuge [data-werkzeug]")) {
      // Gewählt?
      const gewaehlt = knopf.dataset.werkzeug === z.werkzeug;
      // Hervorheben
      knopf.classList.toggle("aktiv", gewaehlt);
      // Für Bildschirmleser
      knopf.setAttribute("aria-pressed", String(gewaehlt));
    }
    // "Löschen" nur mit Auswahl
    el("zeichnungLoeschen").disabled = z.ausgewaehlt === null;
    // "Rückgängig" nur, wenn es etwas gibt
    el("rueckgaengig").disabled = z.verlauf.length === 0;
    // "Alle löschen" nur, wenn es Zeichnungen gibt
    el("alleLoeschen").disabled = z.zeichnungen.length === 0;
    // Hinweistext: Speicherproblem hat Vorrang, sonst die Anleitung des Werkzeugs
    el("zeichenHinweis").textContent = !z.speicherOk
      ? "Browser-Speicher nicht verfügbar: Zeichnungen werden nicht gespeichert."
      : (ANLEITUNG[z.werkzeug] || "");
  }

  // Löscht die ausgewählte Zeichnung
  function loescheAusgewaehlt() {
    // Nichts ausgewählt
    if (z.ausgewaehlt === null) return;
    // Für Rückgängig merken
    merke();
    // Entfernen
    z.zeichnungen = z.zeichnungen.filter((d) => d.id !== z.ausgewaehlt);
    // Auswahl aufheben
    z.ausgewaehlt = null;
    // Speichern und neu zeichnen
    speichere();
    zeichneEbene();
  }

  // Setzt den Knopf "Alle löschen" in den Ausgangszustand
  function alleLoeschenZuruecksetzen() {
    // Nicht mehr bereit
    z.alleLoeschenBereit = false;
    // Ursprünglicher Text
    el("alleLoeschen").textContent = "Alle löschen";
    // Zeitgeber stoppen
    clearTimeout(alleLoeschenZeitgeber);
  }

  // "Alle löschen" in zwei Schritten: erster Klick fragt nach, zweiter Klick (innerhalb von 4 s) löscht
  function alleLoeschen() {
    // Erster Klick: nachfragen
    if (!z.alleLoeschenBereit) {
      // Bereit zur Bestätigung
      z.alleLoeschenBereit = true;
      // Knopftext als Frage
      el("alleLoeschen").textContent = "Wirklich alle löschen?";
      // Nach 4 Sekunden ohne zweiten Klick zurücksetzen
      alleLoeschenZeitgeber = setTimeout(alleLoeschenZuruecksetzen, 4000);
      // Fertig
      return;
    }
    // Zweiter Klick: Knopf zurücksetzen
    alleLoeschenZuruecksetzen();
    // Nichts da
    if (z.zeichnungen.length === 0) return;
    // Für Rückgängig merken
    merke();
    // Alles entfernen
    z.zeichnungen = [];
    z.ausgewaehlt = null;
    // Speichern und neu zeichnen
    speichere();
    zeichneEbene();
  }

  // Ist das Ziel eines Tastendrucks ein Eingabefeld? Dann gehören die Tasten dem Feld (z. B. Entf, Strg+Z).
  function inEingabefeld(ereignis) {
    // Name des Elements
    const tag = ereignis.target && ereignis.target.tagName;
    // Eingabefelder und Auswahllisten
    return tag === "INPUT" || tag === "SELECT" || tag === "TEXTAREA";
  }

  // Einmal beim Start: Knöpfe und Tasten verbinden
  function init() {
    // Werkzeug-Knöpfe
    for (const knopf of document.querySelectorAll("#werkzeuge [data-werkzeug]")) {
      // Klick wählt das Werkzeug
      knopf.addEventListener("click", () => setzeWerkzeug(knopf.dataset.werkzeug));
    }
    // Löschen, Rückgängig, Alle löschen
    el("zeichnungLoeschen").addEventListener("click", loescheAusgewaehlt);
    el("rueckgaengig").addEventListener("click", rueckgaengig);
    el("alleLoeschen").addEventListener("click", alleLoeschen);
    // Magnet umschalten: Vorschau anpassen
    el("magnet").addEventListener("change", zeichneEbene);
    // Tasten
    document.addEventListener("keydown", (ereignis) => {
      // Esc: angefangene Zeichnung bzw. Werkzeug abbrechen, sonst Auswahl aufheben
      if (ereignis.key === "Escape") {
        // Werkzeug aktiv?
        if (z.werkzeug !== "auswahl") {
          // Zurück zu "Auswählen"
          setzeWerkzeug("auswahl");
        } else if (z.ausgewaehlt !== null) {
          // Auswahl aufheben
          z.ausgewaehlt = null;
          aktualisiereLeiste();
          zeichneEbene();
        }
        // Fertig
        return;
      }
      // Die übrigen Tasten nicht in Eingabefeldern abfangen
      if (inEingabefeld(ereignis)) return;
      // Entf oder Rücktaste: Auswahl löschen
      if ((ereignis.key === "Delete" || ereignis.key === "Backspace") && z.ausgewaehlt !== null) {
        // Browser soll nichts anderes tun (z. B. zurück navigieren)
        ereignis.preventDefault();
        // Löschen
        loescheAusgewaehlt();
        // Fertig
        return;
      }
      // Strg+Z (bzw. Cmd+Z auf dem Mac): rückgängig
      if ((ereignis.ctrlKey || ereignis.metaKey) && ereignis.key.toLowerCase() === "z") {
        // Kein anderes Rückgängig des Browsers
        ereignis.preventDefault();
        // Rückgängig
        rueckgaengig();
      }
    });
    // Anfangszustand der Leiste
    aktualisiereLeiste();
  }

  // Beim Laden eines Wertpapiers: dessen Zeichnungen holen
  function setzeWertpapier(symbol) {
    // Gleiches Wertpapier: nichts zu tun (z. B. nur anderer Zeitraum)
    if (symbol === z.symbol) return;
    // Wertpapier übernehmen
    z.symbol = symbol;
    // Zeichnungen aus dem Speicher
    z.zeichnungen = ladeAusSpeicher(symbol);
    // Rückgängig gilt nur innerhalb eines Wertpapiers
    z.verlauf = [];
    // Nichts ausgewählt oder angefangen
    z.ausgewaehlt = null;
    z.angefangen = null;
    z.vorschau = null;
    // Leiste aktualisieren
    aktualisiereLeiste();
  }

  // ---------------------------------------------------------------- Umrechnung und Zeichnen

  // Punkt (Datum, Preis) unter der Maus; Datum = nächster Handelstag, Preis ggf. per Magnet eingerastet
  function punktAus(mx, my) {
    // Umrechnung des Diagramms
    const k = z.kontext;
    // Index des Tages unter der Maus, begrenzt auf die vorhandenen Kerzen
    const i = Math.min(k.kerzen.length - 1, Math.max(0, Math.floor((mx - k.r.rand.links) / k.r.tag)));
    // Kerze dieses Tages
    const kerze = k.kerzen[i];
    // Preis an der Mausposition
    let preis = k.preisAusY(my);
    // Magnet: auf den nächsten der vier Kerzenpreise einrasten, wenn er nah genug ist
    if (el("magnet").checked) {
      // Nächster Kerzenpreis in Pixeln
      let besterAbstand = Infinity;
      // Die vier Preise der Kerze
      for (const kandidat of [kerze.open, kerze.high, kerze.low, kerze.close]) {
        // Abstand in Pixeln
        const abstand = Math.abs(k.y(kandidat) - my);
        // Näher als bisher und innerhalb der Grenze?
        if (abstand < besterAbstand && abstand <= MAGNET_PX) {
          // Merken
          besterAbstand = abstand;
          preis = kandidat;
        }
      }
    }
    // Punkt in Daten-Koordinaten
    return { datum: kerze.datum, preis };
  }

  // Pixel eines Punkts; null, wenn sein Datum nicht im angezeigten Zeitraum liegt
  function pixel(p) {
    // Position des Tages
    const i = z.kontext.indexNachDatum.get(p.datum);
    // Nicht sichtbar
    if (i === undefined) return null;
    // x und y
    return { x: z.kontext.r.x(i), y: z.kontext.y(p.preis) };
  }

  // Grenzen der Zeichenfläche
  function flaeche() {
    // Rahmen des Diagramms
    const r = z.kontext.r;
    // Links, rechts, oben, unten
    return { links: r.rand.links, rechts: r.breite - r.rand.rechts, oben: r.rand.oben, unten: r.hoehe - r.rand.unten };
  }

  // Endpunkt eines Strahls: vom ersten Punkt durch den zweiten bis zum Rand der Zeichenfläche
  function strahlEnde(a, b) {
    // Senkrecht: kein Verlängern möglich
    if (b.x === a.x) return b;
    // Richtung nach rechts oder links
    const rand = b.x > a.x ? flaeche().rechts : flaeche().links;
    // Anteil des Wegs bis zum Rand
    const t = (rand - a.x) / (b.x - a.x);
    // Punkt am Rand auf derselben Geraden
    return { x: rand, y: a.y + t * (b.y - a.y) };
  }

  // Preise der Fibonacci-Stufen: 0 % beim zweiten Punkt, 100 % beim ersten (wie in gängigen Chartprogrammen)
  function fibPreise(p1, p2) {
    // Für jede Stufe: Preis des zweiten Punkts plus Anteil des Wegs zurück zum ersten
    return FIB_STUFEN.map((stufe) => ({ stufe, preis: p2.preis + (p1.preis - p2.preis) * stufe }));
  }

  // Erzeugt ein SVG-Element (wie in app.js) und hängt es an die Ebene
  function svg(name, attribute) {
    // Element im SVG-Namensraum
    const e = document.createElementNS("http://www.w3.org/2000/svg", name);
    // Attribute setzen
    for (const [k, v] of Object.entries(attribute)) e.setAttribute(k, v);
    // An die Ebene hängen
    z.ebene.append(e);
    // Zurückgeben
    return e;
  }

  // Text in der Ebene
  function text(x, y, inhalt, anker = "start") {
    // Textelement in der Farbe der Zeichnungen
    const t = svg("text", { x, y, "text-anchor": anker, style: "fill:var(--zeichnung);font-size:10px" });
    // Inhalt als reiner Text
    t.textContent = inhalt;
  }

  // Zeichnet eine Zeichnung; art = "normal", "gewaehlt" oder "vorschau"
  function zeichneObjekt(d, art) {
    // Linienstil je nach Art
    const stil = `fill:none;stroke:var(--zeichnung);stroke-width:${art === "gewaehlt" ? 3 : 1.6};` +
      (art === "vorschau" ? "opacity:.6;stroke-dasharray:4 3;" : "");
    // Grenzen der Zeichenfläche
    const f = flaeche();
    // Waagrechte Linie: braucht nur den Preis, ist also immer sichtbar
    if (d.typ === "waagrecht") {
      // y-Position
      const y = z.kontext.y(d.punkte[0].preis);
      // Linie über die ganze Breite
      svg("line", { x1: f.links, x2: f.rechts, y1: y, y2: y, style: stil });
      // Preis rechts über der Linie
      text(f.rechts - 4, y - 4, zahl.format(d.punkte[0].preis), "end");
      // Fertig
      return;
    }
    // Alle anderen brauchen zwei Punkte im sichtbaren Zeitraum
    const a = pixel(d.punkte[0]);
    const b = pixel(d.punkte[1]);
    // Ein Punkt außerhalb: nicht zeichnen
    if (!a || !b) return;
    // Je nach Typ
    if (d.typ === "trend") {
      // Strecke zwischen den Punkten
      svg("line", { x1: a.x, y1: a.y, x2: b.x, y2: b.y, style: stil });
    } else if (d.typ === "strahl") {
      // Vom ersten Punkt bis zum Rand
      const e = strahlEnde(a, b);
      svg("line", { x1: a.x, y1: a.y, x2: e.x, y2: e.y, style: stil });
    } else if (d.typ === "zone") {
      // Rechteck zwischen den Ecken, halbdurchsichtig gefüllt
      svg("rect", {
        x: Math.min(a.x, b.x), y: Math.min(a.y, b.y), width: Math.abs(b.x - a.x), height: Math.abs(b.y - a.y),
        style: `${stil}fill:var(--zeichnung);fill-opacity:.12;`,
      });
    } else if (d.typ === "fibonacci") {
      // Linien ab dem linken der beiden Punkte bis zum rechten Rand
      const start = Math.min(a.x, b.x);
      // Jede Stufe
      for (const { stufe, preis } of fibPreise(d.punkte[0], d.punkte[1])) {
        // y-Position
        const y = z.kontext.y(preis);
        // Waagrechte Linie
        svg("line", { x1: start, x2: f.rechts, y1: y, y2: y, style: stil });
        // Beschriftung: Stufe und Preis, z. B. "61,8 % 228,50"
        text(start + 3, y - 3, `${(stufe * 100).toLocaleString("de-DE", { maximumFractionDigits: 1 })} %  ${zahl.format(preis)}`);
      }
      // Gestrichelte Verbindung der beiden Punkte zur Orientierung
      svg("line", { x1: a.x, y1: a.y, x2: b.x, y2: b.y, style: `${stil}stroke-dasharray:2 3;stroke-width:1;` });
    }
  }

  // Zeichnet die Ebene neu: gespeicherte Zeichnungen und die Vorschau
  function zeichneEbene() {
    // Ohne Diagramm nichts zu zeichnen
    if (!z.ebene || !z.kontext) return;
    // Ebene leeren
    z.ebene.replaceChildren();
    // Gespeicherte Zeichnungen
    for (const d of z.zeichnungen) {
      // Ausgewählte hervorheben
      zeichneObjekt(d, d.id === z.ausgewaehlt ? "gewaehlt" : "normal");
    }
    // Vorschau, solange gezeichnet wird und die Maus über dem Diagramm ist
    if (z.werkzeug !== "auswahl" && z.vorschau) {
      // Schon gesetzte Punkte plus Mauspunkt
      const punkte = (z.angefangen ? z.angefangen.punkte : []).concat([z.vorschau]);
      // Vorschau-Zeichnung, wenn genug Punkte da sind; sonst nur eine Markierung des ersten Punkts
      if (punkte.length === PUNKTE[z.werkzeug]) {
        // Wie die fertige Zeichnung, nur gestrichelt
        zeichneObjekt({ typ: z.werkzeug, punkte }, "vorschau");
      } else {
        // Kleiner Kreis an der Stelle, an der der erste Klick landen würde
        const p = pixel(z.vorschau);
        if (p) svg("circle", { cx: p.x, cy: p.y, r: 3, style: "fill:var(--zeichnung)" });
      }
    }
  }

  // Nach jedem Zeichnen des Kursdiagramms aufgerufen; kontext = null, wenn es keine Kerzen gibt
  function zeichne(svgElement, kontext) {
    // Umrechnung merken
    z.kontext = kontext;
    // Ohne Kerzen keine Ebene
    if (!kontext) { z.ebene = null; return; }
    // Grenzen der Zeichenfläche
    const r = kontext.r;
    // Ausschnitt (clipPath): Zeichnungen enden am Rand der Zeichenfläche und laufen nicht über die Achsen
    const defs = document.createElementNS("http://www.w3.org/2000/svg", "defs");
    const ausschnitt = document.createElementNS("http://www.w3.org/2000/svg", "clipPath");
    ausschnitt.setAttribute("id", "zeichenbereich");
    const rechteck = document.createElementNS("http://www.w3.org/2000/svg", "rect");
    rechteck.setAttribute("x", r.rand.links);
    rechteck.setAttribute("y", r.rand.oben);
    rechteck.setAttribute("width", r.breite - r.rand.links - r.rand.rechts);
    rechteck.setAttribute("height", r.hoehe - r.rand.oben - r.rand.unten);
    ausschnitt.append(rechteck);
    defs.append(ausschnitt);
    svgElement.append(defs);
    // Gruppe für die Zeichnungen, auf den Ausschnitt begrenzt
    z.ebene = document.createElementNS("http://www.w3.org/2000/svg", "g");
    z.ebene.setAttribute("clip-path", "url(#zeichenbereich)");
    svgElement.append(z.ebene);
    // Inhalt zeichnen
    zeichneEbene();
  }

  // ---------------------------------------------------------------- Auswählen per Klick

  // Abstand eines Punkts von einer Strecke (in Pixeln)
  function abstandZuStrecke(px, py, x1, y1, x2, y2) {
    // Richtung der Strecke
    const dx = x2 - x1;
    const dy = y2 - y1;
    // Länge zum Quadrat
    const l2 = dx * dx + dy * dy;
    // Strecke ist ein Punkt
    if (l2 === 0) return Math.hypot(px - x1, py - y1);
    // Lage des nächsten Punkts auf der Strecke (0 = Anfang, 1 = Ende), begrenzt auf die Strecke
    const t = Math.max(0, Math.min(1, ((px - x1) * dx + (py - y1) * dy) / l2));
    // Abstand zu diesem Punkt
    return Math.hypot(px - (x1 + t * dx), py - (y1 + t * dy));
  }

  // Abstand der Maus zu einer Zeichnung (Infinity, wenn nicht sichtbar)
  function abstand(d, mx, my) {
    // Grenzen
    const f = flaeche();
    // Waagrechte: senkrechter Abstand
    if (d.typ === "waagrecht") return Math.abs(my - z.kontext.y(d.punkte[0].preis));
    // Zwei Punkte
    const a = pixel(d.punkte[0]);
    const b = pixel(d.punkte[1]);
    // Nicht sichtbar
    if (!a || !b) return Infinity;
    // Je nach Typ
    if (d.typ === "trend") return abstandZuStrecke(mx, my, a.x, a.y, b.x, b.y);
    if (d.typ === "strahl") { const e = strahlEnde(a, b); return abstandZuStrecke(mx, my, a.x, a.y, e.x, e.y); }
    if (d.typ === "zone") {
      // Innerhalb des Rechtecks zählt als Treffer
      const innen = mx >= Math.min(a.x, b.x) && mx <= Math.max(a.x, b.x) && my >= Math.min(a.y, b.y) && my <= Math.max(a.y, b.y);
      return innen ? 0 : Infinity;
    }
    // Fibonacci: kleinster Abstand zu einer der Stufenlinien
    const start = Math.min(a.x, b.x);
    return Math.min(...fibPreise(d.punkte[0], d.punkte[1]).map(({ preis }) =>
      abstandZuStrecke(mx, my, start, z.kontext.y(preis), f.rechts, z.kontext.y(preis))));
  }

  // id der Zeichnung unter der Maus oder null; bei Überlagerung gewinnt die zuletzt gezeichnete
  function trefferTest(mx, my) {
    // Von hinten nach vorne (oben liegende zuerst)
    for (let j = z.zeichnungen.length - 1; j >= 0; j--) {
      // Nah genug?
      if (abstand(z.zeichnungen[j], mx, my) <= TREFFER_PX) return z.zeichnungen[j].id;
    }
    // Nichts getroffen
    return null;
  }

  // ---------------------------------------------------------------- Mausereignisse aus app.js

  // Maus bewegt sich über dem Kursdiagramm
  function bewege(mx, my) {
    // Nur beim Zeichnen eine Vorschau
    if (!z.kontext || z.werkzeug === "auswahl") return;
    // Punkt unter der Maus
    z.vorschau = punktAus(mx, my);
    // Ebene neu zeichnen
    zeichneEbene();
  }

  // Maus verlässt das Diagramm: Vorschau ausblenden
  function verlasse() {
    // Kein Mauspunkt mehr
    z.vorschau = null;
    // Neu zeichnen
    zeichneEbene();
  }

  // Klick (oder Antippen) im Kursdiagramm
  function klick(mx, my) {
    // Ohne Diagramm nichts zu tun
    if (!z.kontext) return;
    // Modus "Auswählen": Zeichnung unter der Maus auswählen (oder Auswahl aufheben)
    if (z.werkzeug === "auswahl") {
      // Treffer suchen
      z.ausgewaehlt = trefferTest(mx, my);
      // Anzeige aktualisieren
      aktualisiereLeiste();
      zeichneEbene();
      // Fertig
      return;
    }
    // Zeichenmodus: Punkt hinzufügen
    const p = punktAus(mx, my);
    // Erster Punkt: Zeichnung anfangen
    if (!z.angefangen) z.angefangen = { typ: z.werkzeug, punkte: [] };
    // Punkt anhängen
    z.angefangen.punkte.push(p);
    // Genug Punkte: fertigstellen
    if (z.angefangen.punkte.length === PUNKTE[z.angefangen.typ]) {
      // Für Rückgängig merken
      merke();
      // Fertige Zeichnung übernehmen
      z.zeichnungen.push({ id: z.naechsteId++, typ: z.angefangen.typ, punkte: z.angefangen.punkte });
      // Speichern
      speichere();
      // Zurück zu "Auswählen" (wie in gängigen Chartprogrammen); zeichnet auch neu
      setzeWerkzeug("auswahl");
      // Fertig
      return;
    }
    // Noch nicht fertig: Vorschau ab dem neuen Punkt
    z.vorschau = p;
    zeichneEbene();
  }

  // Ist gerade ein Zeichenwerkzeug gewählt?
  function aktiv() {
    // Alles außer "Auswählen"
    return z.werkzeug !== "auswahl";
  }

  // Öffentliche Funktionen
  return { init, setzeWertpapier, zeichne, bewege, verlasse, klick, aktiv };
})();
