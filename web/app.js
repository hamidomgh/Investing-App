// Programmlogik der Weboberfläche von MarketLens (E10).
// Lädt die Daten über die eigene HTTP-JSON-Schnittstelle (E8) und zeichnet die Diagramme selbst als SVG.
// Keine Bibliothek, kein externer Dienst: alles in dieser Datei.

// Strenger Modus: macht typische Fehler (z. B. Tippfehler bei Variablennamen) zu echten Fehlern
"use strict";

// Namensraum für SVG-Elemente (nur eine Kennung, es wird nichts aus dem Netz geladen)
const SVG_NS = "http://www.w3.org/2000/svg";

// Kurzschreibweise: Element der Seite über seine id holen
const el = (id) => document.getElementById(id);

// Zahlen im deutschen Format mit zwei Nachkommastellen, z. B. 1.234,56
const zahl = new Intl.NumberFormat("de-DE", { minimumFractionDigits: 2, maximumFractionDigits: 2 });
// Prozent mit Vorzeichen, z. B. +1,23 % (0,0123 wird zu +1,23 %)
const prozent = new Intl.NumberFormat("de-DE", {
  style: "percent", minimumFractionDigits: 2, maximumFractionDigits: 2, signDisplay: "exceptZero",
});

// Zuletzt geladene Daten; null, wenn der jeweilige Abruf fehlgeschlagen ist
const zustand = { kurse: null, indikatoren: null, ereignisse: null };
// Nummer des letzten Ladevorgangs: Antworten eines älteren Ladevorgangs werden verworfen
let ladeNummer = 0;

// ------------------------------------------------------------------ Zugriff auf die Schnittstelle

// Fehler der Schnittstelle: HTTP-Status (0 = Server nicht erreichbar) und Meldung des Servers
class ApiFehler extends Error {
  // Status und Meldung merken
  constructor(status, meldung) {
    // Meldung an die Basisklasse Error geben
    super(meldung);
    // Status merken
    this.status = status;
  }
}

// Holt JSON von der eigenen Schnittstelle. Leere Parameter werden weggelassen.
// Wirft ApiFehler, wenn der Server nicht erreichbar ist oder einen Fehlerstatus liefert.
async function holeJson(pfad, parameter = {}) {
  // URL-Parameter zusammenbauen; URLSearchParams kodiert Sonderzeichen wie "," richtig
  const query = new URLSearchParams();
  // Jeden Parameter übernehmen, wenn er einen Wert hat
  for (const [name, wert] of Object.entries(parameter)) {
    // Leere Felder (z. B. kein Datum gewählt) nicht mitschicken
    if (wert !== "" && wert !== null && wert !== undefined) {
      // Parameter setzen
      query.set(name, wert);
    }
  }
  // Fertige URL (relativ: gleicher Server, von dem die Seite kommt)
  const url = query.toString() ? `${pfad}?${query}` : pfad;
  // Hier landet die HTTP-Antwort
  let antwort;
  // Anfrage senden
  try {
    // fetch liefert nur bei Netzwerkproblemen einen Fehler, nicht bei 404 o. Ä.
    antwort = await fetch(url);
  } catch (e) {
    // Keine Verbindung zum Server
    throw new ApiFehler(0, "Der Server ist nicht erreichbar.");
  }
  // Inhalt als JSON lesen
  let inhalt;
  try {
    // Auch Fehlerantworten der Schnittstelle sind JSON ({"fehler": ..., "status": ...})
    inhalt = await antwort.json();
  } catch (e) {
    // Kein JSON: sollte bei unserer Schnittstelle nicht vorkommen
    throw new ApiFehler(antwort.status, "Die Antwort des Servers ist kein gültiges JSON.");
  }
  // Fehlerstatus: Meldung des Servers weitergeben
  if (!antwort.ok) {
    // Status und Text aus dem Feld "fehler"
    throw new ApiFehler(antwort.status, inhalt.fehler || "Unbekannter Fehler");
  }
  // Erfolg: Daten zurückgeben
  return inhalt;
}

// Verständliche Erklärung zu einem Status (die Codes kommen aus E8)
function erklaere(status) {
  // Je nach Status eine kurze Erklärung für Benutzer
  switch (status) {
    // Netzwerk
    case 0: return "Keine Verbindung zum Server. Läuft MarketLens (./build/projekt)?";
    // Eingabe
    case 400: return "Ungültige Eingabe. Bitte die Einstellungen prüfen.";
    // Unbekannt
    case 404: return "Nicht gefunden.";
    // Zu wenige Daten
    case 422: return "Für diese Einstellungen gibt es zu wenige Kursdaten (z. B. Zeitraum verlängern oder Periode verkürzen).";
    // Anbieter
    case 502: return "Die Datenquelle liefert für dieses Wertpapier keine Daten.";
    // Kontingent
    case 503: return "Das Anfragekontingent des Datenanbieters ist erschöpft. Bitte später erneut versuchen.";
    // Alles andere
    default: return "Unerwarteter Fehler auf dem Server.";
  }
}

// ------------------------------------------------------------------ Anzeige von Fehlern und Hinweisen

// Zeigt eine Liste von Fehlern an: [{ bereich: "Kurse", fehler: ApiFehler }, ...]; leere Liste blendet aus
function zeigeFehler(liste) {
  // Bereich für Fehler
  const box = el("fehler");
  // Alte Meldungen entfernen
  box.replaceChildren();
  // Jede Meldung als eigener Absatz
  for (const { bereich, fehler } of liste) {
    // Absatz
    const p = document.createElement("p");
    // Fett: Bereich und Erklärung
    const kurz = document.createElement("strong");
    // textContent statt innerHTML: Text vom Server wird nie als HTML ausgeführt (Schutz vor Code-Einschleusung)
    kurz.textContent = `${bereich}: ${erklaere(fehler.status)}`;
    // Darunter die genaue Meldung des Servers mit Status
    const genau = document.createElement("span");
    // Status 0 hat keinen HTTP-Code
    genau.textContent = fehler.status ? ` (HTTP ${fehler.status}: ${fehler.message})` : ` (${fehler.message})`;
    // Zusammensetzen
    p.append(kurz, genau);
    // Anhängen
    box.append(p);
  }
  // Nur sichtbar, wenn es Meldungen gibt
  box.hidden = liste.length === 0;
}

// Zeigt den Hinweis auf veraltete Daten (E7), wenn eine der Antworten ihn enthält
function zeigeHinweis() {
  // Erste Antwort mit "veraltet: true" suchen
  const quelle = [zustand.kurse, zustand.indikatoren, zustand.ereignisse].find((d) => d && d.veraltet);
  // Hinweisbereich
  const box = el("hinweis");
  // Text setzen oder ausblenden
  box.textContent = quelle ? `Hinweis: Es werden ältere Daten angezeigt. ${quelle.hinweis || ""}` : "";
  // Sichtbar nur mit Hinweis
  box.hidden = !quelle;
}

// ------------------------------------------------------------------ Laden

// Liest die Einstellungen und lädt Kurse, Indikatoren und Ereignisstudie gleichzeitig
async function lade() {
  // Neue Nummer; ältere Ladevorgänge erkennen daran, dass sie überholt wurden
  const nummer = ++ladeNummer;
  // Gemeinsame Parameter
  const basis = { symbol: el("symbol").value, von: el("von").value, bis: el("bis").value };
  // Zusätzliche Perioden für die Indikatoren
  const perioden = {
    ...basis,
    sma: el("sma").value, ema: el("ema").value, rsi: el("rsi").value,
    macd: el("macd").value.replace(/\s/g, ""), bollinger: el("bollinger").value.replace(/\s/g, ""),
  };
  // Zwischenstand anzeigen
  el("titel").textContent = "Lade …";
  // Drei Anfragen gleichzeitig; allSettled wartet auf alle, auch wenn einzelne fehlschlagen
  const [kurse, indikatoren, ereignisse] = await Promise.allSettled([
    holeJson("/api/kurse", basis),
    holeJson("/api/indikatoren", perioden),
    holeJson("/api/ereignisstudie", basis),
  ]);
  // Inzwischen wurde neu geladen: diese Antworten sind veraltet
  if (nummer !== ladeNummer) {
    // Nichts anzeigen
    return;
  }
  // Ergebnisse übernehmen (null bei Fehlschlag)
  zustand.kurse = kurse.status === "fulfilled" ? kurse.value : null;
  zustand.indikatoren = indikatoren.status === "fulfilled" ? indikatoren.value : null;
  zustand.ereignisse = ereignisse.status === "fulfilled" ? ereignisse.value : null;
  // Fehler sammeln; die übrigen Teile werden trotzdem angezeigt
  const fehler = [];
  if (kurse.status === "rejected") fehler.push({ bereich: "Kurse", fehler: kurse.reason });
  if (indikatoren.status === "rejected") fehler.push({ bereich: "Indikatoren", fehler: indikatoren.reason });
  if (ereignisse.status === "rejected") fehler.push({ bereich: "Ereignisstudie", fehler: ereignisse.reason });
  // Alles anzeigen
  zeigeFehler(fehler);
  zeigeHinweis();
  zeigeTitel();
  zeichne();
  zeigeKennzahlen();
  zeigeEreignisse();
}

// ------------------------------------------------------------------ Hilfen für die Diagramme

// Erzeugt ein SVG-Element mit Attributen und hängt es an "eltern" an
function svgEl(name, attribute, eltern) {
  // SVG-Elemente brauchen den SVG-Namensraum
  const e = document.createElementNS(SVG_NS, name);
  // Alle Attribute setzen
  for (const [schluessel, wert] of Object.entries(attribute)) {
    // Attribut setzen
    e.setAttribute(schluessel, wert);
  }
  // Anhängen, falls gewünscht
  if (eltern) eltern.append(e);
  // Element zurückgeben
  return e;
}

// Schreibt einen Text ins SVG
function svgText(eltern, x, y, text, attribute = {}) {
  // Textelement
  const t = svgEl("text", { x, y, class: "achse", ...attribute }, eltern);
  // Inhalt als reiner Text (nie als HTML)
  t.textContent = text;
  // Element zurückgeben
  return t;
}

// Bereitet ein SVG vor: leeren, Größe bestimmen, x-Abbildung für n Werte berechnen
function rahmen(svg, n) {
  // Alten Inhalt entfernen
  svg.replaceChildren();
  // Tatsächliche Größe auf der Seite (CSS bestimmt Breite und Höhe)
  const breite = svg.clientWidth || 800;
  const hoehe = svg.clientHeight || 300;
  // Koordinatensystem = Pixel
  svg.setAttribute("viewBox", `0 0 ${breite} ${hoehe}`);
  // Ränder für die Achsenbeschriftung; links und rechts in allen Diagrammen gleich, damit die Tage untereinander stehen
  const rand = { links: 60, rechts: 70, oben: 10, unten: 22 };
  // Breite eines Tages
  const tag = (breite - rand.links - rand.rechts) / Math.max(n, 1);
  // x-Position der Mitte von Tag i
  const x = (i) => rand.links + (i + 0.5) * tag;
  // Alles Nötige zurückgeben
  return { breite, hoehe, rand, tag, x };
}

// Kleinster und größter Wert einer Liste (null wird ignoriert) mit 5 % Luft
function bereich(werte) {
  // Nur echte Zahlen
  const zahlen = werte.filter((v) => v !== null && v !== undefined && Number.isFinite(v));
  // Ohne Werte ein Standardbereich
  if (zahlen.length === 0) return [0, 1];
  // Minimum und Maximum
  let min = Math.min(...zahlen);
  let max = Math.max(...zahlen);
  // Alle gleich (z. B. konstante Kurse): künstlich auseinanderziehen
  if (min === max) { min -= 1; max += 1; }
  // 5 % Luft oben und unten
  const luft = (max - min) * 0.05;
  // Bereich zurückgeben
  return [min - luft, max + luft];
}

// "Schöne" Teilstriche für die y-Achse: Schritte von 1, 2 oder 5 mal einer Zehnerpotenz
function schoeneTicks(min, max, anzahl = 5) {
  // Grober Schritt
  const roh = (max - min) / anzahl;
  // Zehnerpotenz darunter, z. B. 10 für 23
  const potenz = Math.pow(10, Math.floor(Math.log10(roh)));
  // Verhältnis zur Zehnerpotenz
  const f = roh / potenz;
  // Auf 1, 2, 5 oder 10 runden
  const schritt = (f < 1.5 ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10) * potenz;
  // Teilstriche sammeln
  const ticks = [];
  // Vom ersten Vielfachen über min bis max
  for (let v = Math.ceil(min / schritt) * schritt; v <= max + schritt * 1e-9; v += schritt) {
    // Rundungsfehler entfernen (0.30000000000000004 -> 0.3)
    ticks.push(Number(v.toPrecision(12)));
  }
  // Liste zurückgeben
  return ticks;
}

// Zeichnet waagrechte Rasterlinien mit Beschriftung links
function yAchse(svg, r, y, ticks, format) {
  // Für jeden Teilstrich
  for (const t of ticks) {
    // Linie über die ganze Zeichenfläche
    svgEl("line", { x1: r.rand.links, x2: r.breite - r.rand.rechts, y1: y(t), y2: y(t), class: "raster" }, svg);
    // Beschriftung rechtsbündig vor der Linie
    svgText(svg, r.rand.links - 6, y(t) + 4, format(t), { "text-anchor": "end" });
  }
}

// Datum "2026-09-17" als "17.09.26"
function kurzesDatum(text) {
  // Bestandteile
  const [j, m, t] = text.split("-");
  // Umgestellt
  return `${t}.${m}.${j.slice(2)}`;
}

// Beschriftet die x-Achse mit etwa 6 gleichmäßig verteilten Daten
function xAchse(svg, r, daten) {
  // Abstand zwischen beschrifteten Tagen
  const schritt = Math.max(1, Math.ceil(daten.length / 6));
  // Jeden "schritt"-ten Tag beschriften
  for (let i = 0; i < daten.length; i += schritt) {
    // Zentriert unter dem Tag
    svgText(svg, r.x(i), r.hoehe - 6, kurzesDatum(daten[i]), { "text-anchor": "middle" });
  }
}

// SVG-Pfad für eine Linie mit Lücken: null unterbricht die Linie (z. B. Einschwingphase)
function linienpfad(werte, x, y) {
  // Pfadtext
  let d = "";
  // Ob gerade eine Linie läuft
  let offen = false;
  // Alle Werte
  werte.forEach((v, i) => {
    // Lücke
    if (v === null || v === undefined) { offen = false; return; }
    // "M" beginnt eine neue Teillinie, "L" setzt sie fort
    d += `${offen ? "L" : "M"}${x(i).toFixed(1)},${y(v).toFixed(1)}`;
    // Linie läuft
    offen = true;
  });
  // Fertiger Pfad
  return d;
}

// Zeichnet eine Linie für eine Werte-Reihe
function linie(svg, werte, x, y, farbe, breite = 1.6, strich = "") {
  // Pfad mit Farbe aus den CSS-Variablen; "fill: none", damit nur die Linie zu sehen ist
  return svgEl("path", {
    d: linienpfad(werte, x, y),
    style: `fill:none;stroke:var(${farbe});stroke-width:${breite};${strich ? `stroke-dasharray:${strich};` : ""}`,
  }, svg);
}

// Ordnet eine Reihe der Indikatoren den Kerzen zu (über das Datum, nicht über die Position).
// Ergebnis: Array mit einem Wert (oder null) pro Kerze.
function ausrichten(daten, werte, indexNachDatum, n) {
  // Zunächst überall "kein Wert"
  const ergebnis = new Array(n).fill(null);
  // Jeden Wert an die Position seines Datums schreiben
  daten.forEach((datum, j) => {
    // Position der Kerze mit diesem Datum
    const i = indexNachDatum.get(datum);
    // Nur übernehmen, wenn es die Kerze gibt
    if (i !== undefined) ergebnis[i] = werte[j];
  });
  // Ausgerichtete Reihe
  return ergebnis;
}

// ------------------------------------------------------------------ Diagramme

// Zeichnet alle drei Diagramme neu (z. B. nach dem Laden, beim Umschalten oder bei neuer Fenstergröße)
function zeichne() {
  // Kursdiagramm
  zeichneKurschart();
  // RSI und MACD nur, wenn eingeschaltet und Indikatoren vorhanden
  const ind = zustand.indikatoren;
  el("rsiBox").hidden = !(ind && el("zeigeRsi").checked);
  el("macdBox").hidden = !(ind && el("zeigeMacd").checked);
  // Sichtbare Unterdiagramme zeichnen
  if (!el("rsiBox").hidden) zeichneRsi();
  if (!el("macdBox").hidden) zeichneMacd();
}

// Hauptdiagramm: Kerzen, Linien der Indikatoren, Support/Resistance, Pivot-Punkte, Zinsentscheidungen
function zeichneKurschart() {
  // SVG des Kursdiagramms
  const svg = el("kurschart");
  // Kerzen (leer, wenn das Laden scheiterte)
  const kerzen = zustand.kurse ? zustand.kurse.kerzen : [];
  // Rahmen vorbereiten
  const r = rahmen(svg, kerzen.length);
  // Ohne Kerzen nur ein Hinweis
  if (kerzen.length === 0) {
    // Text in der Mitte
    svgText(svg, r.breite / 2, r.hoehe / 2, "Keine Kursdaten", { "text-anchor": "middle" });
    // Fertig
    return;
  }
  // Anzahl Tage
  const n = kerzen.length;
  // Datum -> Position, um die Indikatoren richtig zuzuordnen
  const indexNachDatum = new Map(kerzen.map((k, i) => [k.datum, i]));
  // Indikatoren (können fehlen, z. B. bei 422)
  const ind = zustand.indikatoren;
  // Ausgerichtete Reihen der eingeschalteten Linien
  const linien = [];
  // Wert-Reihe ausrichten, falls Indikatoren da sind
  const reihe = (werte) => ausrichten(ind.daten, werte, indexNachDatum, n);
  // SMA
  if (ind && el("zeigeSma").checked) linien.push({ werte: reihe(ind.sma), farbe: "--sma", name: `SMA ${ind.parameter.sma}` });
  // EMA
  if (ind && el("zeigeEma").checked) linien.push({ werte: reihe(ind.ema), farbe: "--ema", name: `EMA ${ind.parameter.ema}` });
  // Bollinger-Bänder
  const bb = ind && el("zeigeBollinger").checked
    ? { oben: reihe(ind.bollinger.oben), mitte: reihe(ind.bollinger.mitte), unten: reihe(ind.bollinger.unten) } : null;
  // Support/Resistance
  const niveaus = ind && el("zeigeNiveaus").checked ? ind.niveaus : [];
  // Pivot-Punkte
  const pivot = ind && el("zeigePivot").checked ? ind.pivot : null;

  // Wertebereich der y-Achse: alle Kerzen und alle sichtbaren Linien
  const werte = kerzen.flatMap((k) => [k.low, k.high]);
  linien.forEach((l) => werte.push(...l.werte));
  if (bb) werte.push(...bb.oben, ...bb.unten);
  niveaus.forEach((nv) => werte.push(nv.preis));
  if (pivot) werte.push(pivot.r2, pivot.s2);
  const [ymin, ymax] = bereich(werte);
  // Abbildung Preis -> y-Pixel (oben = hoher Preis)
  const y = (v) => r.rand.oben + ((ymax - v) / (ymax - ymin)) * (r.hoehe - r.rand.oben - r.rand.unten);

  // Raster und Achsen
  yAchse(svg, r, y, schoeneTicks(ymin, ymax), (t) => zahl.format(t));
  xAchse(svg, r, kerzen.map((k) => k.datum));

  // Bollinger: Fläche zwischen oberem und unterem Band, dann die Mittellinie
  if (bb) {
    // Tage, an denen beide Bänder vorhanden sind
    const tage = bb.oben.map((v, i) => (v !== null && bb.unten[i] !== null ? i : -1)).filter((i) => i >= 0);
    // Fläche nur, wenn es solche Tage gibt
    if (tage.length > 1) {
      // Oben von links nach rechts, unten von rechts nach links: ergibt eine geschlossene Fläche
      const punkte = tage.map((i) => `${r.x(i)},${y(bb.oben[i])}`)
        .concat(tage.slice().reverse().map((i) => `${r.x(i)},${y(bb.unten[i])}`));
      // Halbdurchsichtige Fläche
      svgEl("polygon", { points: punkte.join(" "), style: "fill:var(--bollinger);opacity:.12" }, svg);
    }
    // Mittellinie gestrichelt
    linie(svg, bb.mitte, r.x, y, "--bollinger", 1.2, "4 3");
  }

  // Support/Resistance: waagrechte gestrichelte Linien mit Beschriftung rechts
  for (const nv of niveaus) {
    // Farbe nach Art
    const farbe = nv.art === "support" ? "--support" : "--resistance";
    // Linie über die ganze Breite
    svgEl("line", {
      x1: r.rand.links, x2: r.breite - r.rand.rechts, y1: y(nv.preis), y2: y(nv.preis),
      style: `stroke:var(${farbe});stroke-width:1;stroke-dasharray:6 4;opacity:.8`,
    }, svg);
    // Beschriftung: S oder R, Preis, Anzahl Berührungen
    svgText(svg, r.breite - r.rand.rechts + 4, y(nv.preis) + 4,
      `${nv.art === "support" ? "S" : "R"} ${zahl.format(nv.preis)} (${nv.beruehrungen}×)`,
      { style: `fill:var(${farbe});font-size:10px` });
  }

  // Pivot-Punkte: gepunktete Linien im letzten Viertel (sie gelten für den nächsten Tag)
  if (pivot) {
    // Beginn der Linien
    const start = r.x(Math.floor(n * 0.75));
    // Die fünf Werte
    for (const [name, wert] of [["R2", pivot.r2], ["R1", pivot.r1], ["P", pivot.p], ["S1", pivot.s1], ["S2", pivot.s2]]) {
      // Linie
      svgEl("line", {
        x1: start, x2: r.breite - r.rand.rechts, y1: y(wert), y2: y(wert),
        style: "stroke:var(--pivot);stroke-width:1;stroke-dasharray:2 3",
      }, svg);
      // Name links an der Linie
      svgText(svg, start - 4, y(wert) + 4, name, { "text-anchor": "end", style: "fill:var(--pivot);font-size:10px" });
    }
  }

  // Kerzen: Docht (High bis Low) und Körper (Open bis Close)
  const koerperbreite = Math.max(1, r.tag * 0.65);
  // Jede Kerze
  kerzen.forEach((k, i) => {
    // Grün, wenn der Kurs gestiegen ist, sonst rot
    const farbe = k.close >= k.open ? "--steigt" : "--faellt";
    // Docht
    svgEl("line", { x1: r.x(i), x2: r.x(i), y1: y(k.high), y2: y(k.low), style: `stroke:var(${farbe});stroke-width:1` }, svg);
    // Körper; mindestens 1 Pixel hoch, damit auch Open = Close sichtbar ist
    const oben = y(Math.max(k.open, k.close));
    const hoehe = Math.max(1, Math.abs(y(k.open) - y(k.close)));
    svgEl("rect", { x: r.x(i) - koerperbreite / 2, y: oben, width: koerperbreite, height: hoehe, style: `fill:var(${farbe})` }, svg);
  });

  // Linien der Indikatoren über den Kerzen
  linien.forEach((l) => linie(svg, l.werte, r.x, y, l.farbe));

  // Zinsentscheidungen: senkrechte Linie am Reaktionstag mit Name der Zentralbank
  if (zustand.ereignisse && el("zeigeEreignisse").checked) {
    // Jedes Ereignis
    for (const e of zustand.ereignisse.ereignisse) {
      // Position des Reaktionstags
      const i = indexNachDatum.get(e.reaktionstag);
      // Nur, wenn der Tag im Diagramm liegt
      if (i === undefined) continue;
      // Senkrechte Linie
      svgEl("line", {
        x1: r.x(i), x2: r.x(i), y1: r.rand.oben, y2: r.hoehe - r.rand.unten,
        style: "stroke:var(--ereignis);stroke-width:1.5;stroke-dasharray:3 3",
      }, svg);
      // Beschriftung oben: Zentralbank und neuer Leitzins
      svgText(svg, r.x(i) + 3, r.rand.oben + 10, `${e.zentralbank} ${zahl.format(e.leitzins)} %`,
        { style: "fill:var(--ereignis);font-size:10px" });
    }
  }

  // Legende der Linien oben links
  linien.forEach((l, j) => {
    // Farbiger Text
    svgText(svg, r.rand.links + 6 + j * 80, r.rand.oben + 10, l.name, { style: `fill:var(${l.farbe});font-size:11px` });
  });

  // Fadenkreuz und Infokasten beim Überfahren mit der Maus
  fadenkreuz(svg, r, kerzen, linien);
}

// Senkrechte Linie und Infokasten, die der Maus folgen
function fadenkreuz(svg, r, kerzen, linien) {
  // Senkrechte Linie, zunächst unsichtbar
  const strich = svgEl("line", { y1: r.rand.oben, y2: r.hoehe - r.rand.unten, class: "raster", visibility: "hidden" }, svg);
  // Unsichtbare Fläche über dem Diagramm, die die Mausbewegungen empfängt
  const flaeche = svgEl("rect", {
    x: r.rand.links, y: 0, width: r.breite - r.rand.links - r.rand.rechts, height: r.hoehe, style: "fill:transparent",
  }, svg);
  // Infokasten
  const info = el("info");
  // Maus bewegt sich
  flaeche.addEventListener("mousemove", (ereignis) => {
    // Mausposition relativ zum SVG
    const box = svg.getBoundingClientRect();
    const mx = ereignis.clientX - box.left;
    // Nächster Tag
    const i = Math.min(kerzen.length - 1, Math.max(0, Math.floor((mx - r.rand.links) / r.tag)));
    // Kerze dieses Tages
    const k = kerzen[i];
    // Linie dorthin
    strich.setAttribute("x1", r.x(i));
    strich.setAttribute("x2", r.x(i));
    strich.setAttribute("visibility", "visible");
    // Zeilen des Infokastens
    const zeilen = [
      kurzesDatum(k.datum),
      `O ${zahl.format(k.open)}  H ${zahl.format(k.high)}  L ${zahl.format(k.low)}  C ${zahl.format(k.close)}`,
    ];
    // Werte der Linien an diesem Tag
    for (const l of linien) {
      // Nur vorhandene Werte
      if (l.werte[i] !== null) zeilen.push(`${l.name}: ${zahl.format(l.werte[i])}`);
    }
    // RSI an diesem Tag, falls vorhanden
    const rsi = zustand.indikatoren ? rsiWerte()[i] : null;
    if (rsi !== null && rsi !== undefined) zeilen.push(`RSI: ${zahl.format(rsi)}`);
    // Text setzen (jede Zeile ein eigenes div, als reiner Text)
    info.replaceChildren(...zeilen.map((z) => { const d = document.createElement("div"); d.textContent = z; return d; }));
    // Anzeigen
    info.hidden = false;
  });
  // Maus verlässt das Diagramm: ausblenden
  flaeche.addEventListener("mouseleave", () => {
    // Linie und Kasten verstecken
    strich.setAttribute("visibility", "hidden");
    info.hidden = true;
  });
}

// RSI-Werte, ausgerichtet auf die Kerzen
function rsiWerte() {
  // Kerzen und Indikatoren
  const kerzen = zustand.kurse ? zustand.kurse.kerzen : [];
  const ind = zustand.indikatoren;
  // Ohne Indikatoren nur Lücken
  if (!ind) return new Array(kerzen.length).fill(null);
  // Ausrichten
  return ausrichten(ind.daten, ind.rsi, new Map(kerzen.map((k, i) => [k.datum, i])), kerzen.length);
}

// RSI-Diagramm: 0 bis 100, Linien bei 30 (überverkauft) und 70 (überkauft)
function zeichneRsi() {
  // SVG und Kerzen
  const svg = el("rsichart");
  const kerzen = zustand.kurse ? zustand.kurse.kerzen : [];
  // Rahmen
  const r = rahmen(svg, kerzen.length);
  // Feste Skala 0..100
  const y = (v) => r.rand.oben + ((100 - v) / 100) * (r.hoehe - r.rand.oben - r.rand.unten);
  // Raster bei 30, 50, 70
  yAchse(svg, r, y, [30, 50, 70], (t) => String(t));
  // Linie
  linie(svg, rsiWerte(), r.x, y, "--sma");
  // Name
  svgText(svg, r.rand.links + 6, r.rand.oben + 10, `RSI ${zustand.indikatoren.parameter.rsi}`, { style: "fill:var(--sma);font-size:11px" });
}

// MACD-Diagramm: Histogramm als Balken, MACD-Linie und Signallinie
function zeichneMacd() {
  // SVG, Kerzen, Indikatoren
  const svg = el("macdchart");
  const kerzen = zustand.kurse ? zustand.kurse.kerzen : [];
  const ind = zustand.indikatoren;
  // Rahmen
  const r = rahmen(svg, kerzen.length);
  // Datum -> Position
  const indexNachDatum = new Map(kerzen.map((k, i) => [k.datum, i]));
  // Die drei Reihen ausrichten
  const linie_ = ausrichten(ind.daten, ind.macd.linie, indexNachDatum, kerzen.length);
  const signal = ausrichten(ind.daten, ind.macd.signal, indexNachDatum, kerzen.length);
  const histo = ausrichten(ind.daten, ind.macd.histogramm, indexNachDatum, kerzen.length);
  // Wertebereich inklusive 0
  const [ymin, ymax] = bereich([...linie_, ...signal, ...histo, 0]);
  // Abbildung
  const y = (v) => r.rand.oben + ((ymax - v) / (ymax - ymin)) * (r.hoehe - r.rand.oben - r.rand.unten);
  // Raster
  yAchse(svg, r, y, schoeneTicks(ymin, ymax, 3), (t) => zahl.format(t));
  // Balken des Histogramms von 0 aus
  histo.forEach((v, i) => {
    // Nur vorhandene Werte
    if (v === null) return;
    // Balken nach oben (grün) oder unten (rot)
    svgEl("rect", {
      x: r.x(i) - Math.max(1, r.tag * 0.6) / 2, y: Math.min(y(v), y(0)),
      width: Math.max(1, r.tag * 0.6), height: Math.max(1, Math.abs(y(v) - y(0))),
      style: `fill:var(${v >= 0 ? "--steigt" : "--faellt"});opacity:.6`,
    }, svg);
  });
  // MACD-Linie und Signallinie
  linie(svg, linie_, r.x, y, "--sma");
  linie(svg, signal, r.x, y, "--signal");
  // Name
  const [s, l, sig] = ind.parameter.macd;
  svgText(svg, r.rand.links + 6, r.rand.oben + 10, `MACD ${s},${l},${sig}`, { style: "fill:var(--sma);font-size:11px" });
  svgText(svg, r.rand.links + 110, r.rand.oben + 10, "Signal", { style: "fill:var(--signal);font-size:11px" });
}

// ------------------------------------------------------------------ Texte, Kennzahlen, Tabelle

// Überschrift über dem Diagramm
function zeigeTitel() {
  // Erste vorhandene Antwort mit Kopfdaten
  const kopf = zustand.kurse || zustand.indikatoren || zustand.ereignisse;
  // Ohne Daten
  if (!kopf) { el("titel").textContent = "Keine Daten"; return; }
  // Text zusammensetzen
  let text = `${kopf.symbol} · ${kopf.name} · ${kopf.land}`;
  // Letzter Schlusskurs, wenn Kerzen da sind
  const kerzen = zustand.kurse ? zustand.kurse.kerzen : [];
  if (kerzen.length > 0) text += ` · ${zahl.format(kerzen[kerzen.length - 1].close)} ${kopf.waehrung}`;
  // Setzen
  el("titel").textContent = text;
}

// Kennzahlen als Name/Wert-Paare
function zeigeKennzahlen() {
  // Liste der Paare
  const paare = [];
  // Kerzen
  const kerzen = zustand.kurse ? zustand.kurse.kerzen : [];
  // Aus den Kerzen: Zeitraum, Anzahl, Veränderung
  if (kerzen.length > 0) {
    // Erster und letzter Tag
    const erste = kerzen[0];
    const letzte = kerzen[kerzen.length - 1];
    paare.push(["Zeitraum", `${kurzesDatum(erste.datum)} – ${kurzesDatum(letzte.datum)}`]);
    paare.push(["Handelstage", String(kerzen.length)]);
    paare.push(["Veränderung im Zeitraum", prozent.format(letzte.close / erste.close - 1)]);
  }
  // Aus den Indikatoren: Volatilität, Drawdown, Pivot-Punkte
  const ind = zustand.indikatoren;
  if (ind) {
    // Volatilität kann null sein (zu wenige Daten)
    paare.push(["Volatilität (annualisiert)", ind.volatilitaet === null ? "nicht verfügbar" : prozent.format(ind.volatilitaet).replace("+", "")]);
    paare.push(["Maximaler Drawdown", prozent.format(-ind.maxDrawdown)]);
    paare.push(["Pivot P", zahl.format(ind.pivot.p)]);
    paare.push(["R1 / R2", `${zahl.format(ind.pivot.r1)} / ${zahl.format(ind.pivot.r2)}`]);
    paare.push(["S1 / S2", `${zahl.format(ind.pivot.s1)} / ${zahl.format(ind.pivot.s2)}`]);
    paare.push(["Niveaus (S/R)", String(ind.niveaus.length)]);
  }
  // Liste leeren und neu füllen
  const dl = el("kennzahlen");
  dl.replaceChildren();
  // Jedes Paar als Begriff und Beschreibung
  for (const [name, wert] of paare) {
    // Name
    const dt = document.createElement("dt");
    dt.textContent = name;
    // Wert
    const dd = document.createElement("dd");
    dd.textContent = wert;
    // Anhängen
    dl.append(dt, dd);
  }
}

// Tabelle der Ereignisstudie
function zeigeEreignisse() {
  // Tabellenkörper leeren
  const tbody = el("ereignisTabelle");
  tbody.replaceChildren();
  // Daten der Ereignisstudie
  const daten = zustand.ereignisse;
  // Erklärung des Datums (E6): Tag der Wirksamkeit, nicht der Sitzung
  el("ereignisHinweis").textContent = daten && daten.datumsart === "wirksam"
    ? "Datum = Tag, an dem der neue Leitzins wirksam wurde (Quelle FRED), nicht der Sitzungstag. Veränderung = Schlusskurs am ersten Handelstag danach gegenüber dem Vortag."
    : "";
  // Ohne Daten (Fehler wird oben angezeigt)
  if (!daten) return;
  // Keine Ereignisse: erklärende Zeile statt leerer Tabelle
  if (daten.ereignisse.length === 0) {
    // Zeile über alle Spalten
    const tr = document.createElement("tr");
    const td = document.createElement("td");
    td.colSpan = 7;
    td.textContent = `Keine Zinsentscheidungen im Zeitraum (oder keine Zentralbank für ${daten.land} im Katalog).`;
    tr.append(td);
    tbody.append(tr);
    // Fertig
    return;
  }
  // Jedes Ereignis als Zeile
  for (const e of daten.ereignisse) {
    // Neue Zeile
    const tr = document.createElement("tr");
    // Zellen in Spaltenreihenfolge
    const zellen = [
      e.zentralbank, kurzesDatum(e.datum), `${zahl.format(e.leitzins)} %`, kurzesDatum(e.reaktionstag),
      zahl.format(e.kursVorher), zahl.format(e.kursNachher), prozent.format(e.veraenderung),
    ];
    // Zellen erzeugen
    zellen.forEach((text, j) => {
      // Zelle
      const td = document.createElement("td");
      td.textContent = text;
      // Letzte Spalte farbig nach Vorzeichen
      if (j === 6) td.className = e.veraenderung >= 0 ? "plus" : "minus";
      // Anhängen
      tr.append(td);
    });
    // Zeile anhängen
    tbody.append(tr);
  }
}

// ------------------------------------------------------------------ Start

// Wird beim Laden der Seite ausgeführt
async function start() {
  // Liste der Wertpapiere laden
  try {
    // Anfrage
    const daten = await holeJson("/api/wertpapiere");
    // Auswahlliste
    const auswahl = el("symbol");
    // Eine Option pro Wertpapier
    for (const wp of daten.wertpapiere) {
      // Option mit Symbol als Wert
      const option = document.createElement("option");
      option.value = wp.symbol;
      option.textContent = `${wp.symbol} – ${wp.name} (${wp.land}, ${wp.waehrung})`;
      auswahl.append(option);
    }
  } catch (f) {
    // Ohne Liste kann nichts gewählt werden: Fehler anzeigen und aufhören
    zeigeFehler([{ bereich: "Wertpapierliste", fehler: f }]);
    el("titel").textContent = "Keine Daten";
    return;
  }
  // "Anzeigen": Formular nicht abschicken (würde die Seite neu laden), sondern selbst laden
  el("auswahl").addEventListener("submit", (e) => { e.preventDefault(); lade(); });
  // Anderes Wertpapier gewählt: sofort laden
  el("symbol").addEventListener("change", lade);
  // Ein/Aus der Indikatoren: nur neu zeichnen, ohne neue Anfrage
  for (const id of ["zeigeSma", "zeigeEma", "zeigeBollinger", "zeigeNiveaus", "zeigePivot", "zeigeEreignisse", "zeigeRsi", "zeigeMacd"]) {
    // Bei Änderung neu zeichnen
    el(id).addEventListener("change", zeichne);
  }
  // Neue Fenstergröße: höchstens einmal pro Bild neu zeichnen
  let warte = 0;
  window.addEventListener("resize", () => { cancelAnimationFrame(warte); warte = requestAnimationFrame(zeichne); });
  // Erstes Wertpapier laden
  lade();
}

// Los geht's
start();
