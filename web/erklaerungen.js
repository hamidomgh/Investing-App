// Erklärungen („?“) zu allen Zahlen und Indikatoren (Feature F2, Beschreibung in Features/F2_Erklaerungen.md).
// Enthält (1) alle Texte im Objekt ERKLAERUNGEN und (2) das Erklärungsfenster im Objekt Hilfe.
//
// Jede Erklärung hat bis zu fünf Teile:
//   titel   Überschrift
//   was     Was ist das? (Alltagssprache)
//   wie     Wie wird es hier berechnet? (genau so, wie MarketLens rechnet, siehe docs/E3_… bis docs/E6_…)
//   lesen   Wie liest man es? (übliche Deutung)
//   aktuell Satz mit den aktuellen Werten; wird nur gezeigt, wenn alle seine Werte vorhanden sind
//   beratung true = Hinweis „keine Anlageberatung“ darunter
// Platzhalter wie {sma} werden mit den aktuellen Werten aus app.js ersetzt (Hilfe.init bekommt eine Funktion dafür).
//
// Wichtig für den Test Weboberflaeche.ErklaerungenVollstaendig: Jeder Schlüssel steht in einer eigenen Zeile,
// genau zwei Leerzeichen eingerückt, gefolgt von ": {".

// Strenger Modus
"use strict";

// Alle Texte. Schlüssel = Wert von data-hilfe in index.html bzw. Hilfe.knopf("…") in app.js.
const ERKLAERUNGEN = {
  sma: {
    titel: "SMA – einfacher gleitender Durchschnitt",
    was: "Durchschnitt der Schlusskurse der letzten n Handelstage. Er glättet die täglichen Schwankungen, damit der Trend besser sichtbar wird.",
    wie: "SMA {sma} = Summe der letzten {sma} Schlusskurse ÷ {sma}. Für die ersten {smaMinus1} Tage gibt es noch keinen Wert (Einschwingphase), deshalb beginnt die Linie nicht am linken Rand. Die Zahl im Feld ist die Periode n.",
    lesen: "Liegt der Kurs über dem SMA, gilt der Trend als aufwärts gerichtet, darunter als abwärts. Ein längerer SMA reagiert langsamer.",
    aktuell: "Aktuell: SMA {sma} am letzten Tag = {smaLetzter}.",
    beratung: true,
  },
  ema: {
    titel: "EMA – exponentieller gleitender Durchschnitt",
    was: "Gleitender Durchschnitt, bei dem neuere Kurse stärker zählen als ältere. Er reagiert deshalb schneller auf Kursänderungen als der SMA.",
    wie: "Glättungsfaktor α = 2 ÷ ({ema} + 1) = {emaAlpha}. Startwert ist der SMA der ersten {ema} Kurse, danach EMA(heute) = α × Kurs(heute) + (1 − α) × EMA(gestern). Die Zahl im Feld ist die Periode n.",
    lesen: "Wie beim SMA: Kurs über dem EMA gilt als Aufwärtstrend. Kreuzt der Kurs den EMA, wird das oft als mögliche Trendänderung gedeutet.",
    aktuell: "Aktuell: EMA {ema} am letzten Tag = {emaLetzter}.",
    beratung: true,
  },
  bollinger: {
    titel: "Bollinger-Bänder",
    was: "Ein Band um einen gleitenden Durchschnitt, dessen Breite sich nach der Schwankung der Kurse richtet.",
    wie: "Mitte = SMA {bbN}. Oberes und unteres Band = Mitte ± {bbK} × Standardabweichung der letzten {bbN} Schlusskurse (geteilt durch n, nicht durch n − 1). Im Feld steht „Periode,k“.",
    lesen: "Ein enges Band zeigt ruhige Kurse, ein breites starke Schwankungen. Ein Kurs am oberen Band gilt im Verhältnis zur jüngsten Spanne als hoch, am unteren als niedrig.",
    beratung: true,
  },
  niveaus: {
    titel: "Support und Resistance",
    was: "Preisbereiche, an denen der Kurs mehrfach umgekehrt ist. Support liegt unter dem aktuellen Kurs (dort fiel er nicht weiter), Resistance darüber (dort stieg er nicht weiter).",
    wie: "Zuerst lokale Hochs (aus den Höchstkursen) und Tiefs (aus den Tiefstkursen): Ein Tag ist ein Hoch, wenn die {fenster} Tage davor und danach alle niedriger sind (beim Tief alle höher); ein Plateau gleicher Werte zählt einmal, Tage am Rand ohne volles Fenster zählen nicht. Dann werden die Preise aufsteigend durchgegangen: Weicht ein Preis höchstens {toleranz} vom Mittelwert des aktuellen Niveaus ab, gehört er dazu, sonst beginnt ein neues Niveau. Ein Niveau auf oder unter dem letzten Schlusskurs ist Support (S, grün), darüber Resistance (R, rot). „(2×)“ = Anzahl der Hochs und Tiefs, die zu diesem Niveau gehören.",
    lesen: "Je öfter ein Niveau berührt wurde, desto wichtiger gilt es. Bricht der Kurs durch ein Niveau, wird aus Resistance oft Support und umgekehrt.",
    aktuell: "Aktuell: {anzahlNiveaus} Niveaus.",
    beratung: true,
  },
  pivot: {
    titel: "Pivot-Punkte",
    was: "Klassische Marken für den nächsten Handelstag, berechnet aus dem letzten Tag: ein Mittelpunkt P, zwei Widerstände (R1, R2) darüber und zwei Unterstützungen (S1, S2) darunter.",
    wie: "Aus High H, Low L und Close C der letzten Kerze: P = (H + L + C) ÷ 3, R1 = 2P − L, S1 = 2P − H, R2 = P + (H − L), S2 = P − (H − L). Im Diagramm stehen sie als gepunktete Linien im letzten Viertel.",
    lesen: "Ein Kurs über P gilt als positive Stimmung, darunter als negative; R1/R2 und S1/S2 gelten als mögliche Wendemarken.",
    aktuell: "Aktuell mit H = {letzterHigh}, L = {letzterLow}, C = {letzterClose}: P = {pivotP}, R1 = {pivotR1}, R2 = {pivotR2}, S1 = {pivotS1}, S2 = {pivotS2}.",
    beratung: true,
  },
  zinsmarkierung: {
    titel: "Zinsentscheidungen im Diagramm",
    was: "Senkrechte gelbe Linien an den Tagen, an denen der Kurs auf eine Änderung des Leitzinses der zuständigen Zentralbank reagierte.",
    wie: "Die Linie steht am Reaktionstag: dem ersten Handelstag nach dem Tag, an dem der neue Leitzins wirksam wurde. Die Beschriftung nennt Zentralbank und neuen Leitzins. Zuständig ist die Zentralbank, deren Länderliste im Katalog das Land des Wertpapiers enthält (z. B. EZB für Deutschland).",
    lesen: "Die Kursveränderung an diesem Tag steht in der Tabelle „Ereignisstudie“ unten.",
  },
  rsi: {
    titel: "RSI – Relative Strength Index",
    was: "Misst, wie stark die Kursgewinne gegenüber den Verlusten der letzten Tage überwiegen. Werte von 0 bis 100.",
    wie: "Durchschnittlicher Gewinn und durchschnittlicher Verlust der letzten {rsi} Tagesänderungen, geglättet nach Wilder: neuer Durchschnitt = (alter Durchschnitt × ({rsi} − 1) + heutiger Wert) ÷ {rsi}. RS = Ø Gewinn ÷ Ø Verlust, RSI = 100 − 100 ÷ (1 + RS). Ohne Verluste ist der RSI 100, bei gleichbleibenden Kursen 50. Für die ersten {rsi} Tage gibt es keinen Wert.",
    lesen: "Über 70 gilt ein Wert als „überkauft“, unter 30 als „überverkauft“. Die Linien bei 30, 50 und 70 im RSI-Diagramm markieren diese Grenzen.",
    aktuell: "Aktuell: RSI {rsi} am letzten Tag = {rsiLetzter}.",
    beratung: true,
  },
  macd: {
    titel: "MACD – Moving Average Convergence Divergence",
    was: "Abstand zwischen einem schnellen und einem langsamen EMA. Zeigt Richtung und Stärke eines Trends.",
    wie: "MACD-Linie (blau) = EMA {macdS} − EMA {macdL}. Signallinie (orange) = EMA {macdSig} der MACD-Linie. Histogramm (Balken) = MACD-Linie − Signallinie, grün über 0, rot darunter. Die MACD-Linie beginnt am {macdL}. Handelstag, Signal und Histogramm am {macdLS}. Handelstag. Im Feld steht „schnell,langsam,signal“.",
    lesen: "Steigt die MACD-Linie über die Signallinie (das Histogramm wird positiv), wird das als positives Signal gedeutet, umgekehrt als negatives.",
    beratung: true,
  },
  zeitraum: {
    titel: "Zeitraum",
    was: "Erster und letzter Handelstag der angezeigten Kurse.",
    wie: "Ohne Angaben bei „von“ und „bis“ werden alle vorhandenen Tage gezeigt; die gespeicherten Daten enthalten die letzten etwa 100 Handelstage.",
    aktuell: "Aktuell: {zeitraum}.",
  },
  handelstage: {
    titel: "Handelstage",
    was: "Anzahl der Tage mit Kursen im Zeitraum. Wochenenden und Feiertage ohne Handel zählen nicht.",
    wie: "Jede Kerze im Kursdiagramm ist ein Handelstag.",
    lesen: "Viele Indikatoren brauchen eine Mindestzahl an Tagen, z. B. MACD {macdS},{macdL},{macdSig} mindestens {macdLS}. Bei zu wenigen Tagen erscheint eine Meldung statt der Indikatoren.",
    aktuell: "Aktuell: {handelstage} Handelstage.",
  },
  veraenderung: {
    titel: "Veränderung im Zeitraum",
    was: "Wie sich der Kurs vom ersten bis zum letzten Tag des Zeitraums verändert hat.",
    wie: "Schlusskurs am letzten Tag ÷ Schlusskurs am ersten Tag − 1, in Prozent.",
    aktuell: "Aktuell: von {ersterClose} auf {letzterClose} = {veraenderung}.",
  },
  volatilitaet: {
    titel: "Volatilität (annualisiert)",
    was: "Wie stark der Kurs schwankt, hochgerechnet auf ein Jahr.",
    wie: "Standardabweichung der täglichen logarithmischen Renditen ln(Kurs heute ÷ Kurs gestern), als Stichprobe (geteilt durch n − 1), mal √252, weil ein Jahr etwa 252 Handelstage hat. Mit weniger als 3 Kerzen ist sie nicht verfügbar.",
    lesen: "Höhere Werte bedeuten stärkere Schwankungen und damit ein höheres Risiko.",
    aktuell: "Aktuell: {volatilitaet} pro Jahr.",
    beratung: true,
  },
  drawdown: {
    titel: "Maximaler Drawdown",
    was: "Der größte zwischenzeitliche Verlust: wie weit der Kurs höchstens unter einen vorherigen Höchststand gefallen ist.",
    wie: "Für jeden Tag: (bisheriger Höchststand − Schlusskurs) ÷ bisheriger Höchststand. Der größte dieser Werte ist der maximale Drawdown; angezeigt mit Minuszeichen.",
    lesen: "Er zeigt das schlimmste Zwischenergebnis: Wer am ungünstigsten Tag (auf dem Höchststand) gekauft hätte, lag zwischenzeitlich um diesen Anteil im Minus.",
    aktuell: "Aktuell: {maxDrawdown}.",
    beratung: true,
  },
  waehrung: {
    titel: "Währung",
    was: "Die Währung, in der die Kurse dieses Wertpapiers angegeben sind. Es wird nicht umgerechnet.",
    wie: "Sie steht im Katalog (config/katalog.json). GBX bedeutet Pence: Kurse an der Londoner Börse werden üblicherweise in Pence angegeben, 100 GBX = 1 GBP.",
    aktuell: "Aktuell: {waehrung}.",
  },
  ereignisstudie: {
    titel: "Ereignisstudie",
    was: "Wie hat der Kurs am Handelstag nach einer Änderung des Leitzinses reagiert?",
    wie: "Für jede Leitzinsänderung der zuständigen Zentralbank im Zeitraum: Schlusskurs am ersten Handelstag nach dem Wirksamkeitstag gegenüber dem Schlusskurs am Handelstag davor. Fällt der Wirksamkeitstag auf ein Wochenende, zählt der nächste Handelstag. Gibt es im Katalog keine Zentralbank für das Land (z. B. Großbritannien), bleibt die Tabelle leer. Die Zinsdaten stammen von FRED.",
    lesen: "Ein einzelner Tag zeigt nur die unmittelbare Reaktion; andere Nachrichten am selben Tag können mitwirken.",
    aktuell: "Aktuell: {anzahlEreignisse} Ereignisse im Zeitraum.",
  },
  zentralbank: {
    titel: "Zentralbank",
    was: "Die Zentralbank, deren Leitzins sich geändert hat: Fed für die USA, EZB für die Länder des Euroraums.",
  },
  wirksamAb: {
    titel: "Wirksam ab",
    was: "Der Tag, an dem der neue Leitzins laut FRED wirksam wurde.",
    wie: "Das ist nicht der Tag der Sitzung: bei der Fed meist der Tag danach, bei der EZB bis zu einer Woche später. Sitzungen, die den Zins nicht ändern, sind in den Daten nicht sichtbar und fehlen deshalb.",
  },
  leitzins: {
    titel: "Leitzins",
    was: "Der neue Leitzins in Prozent nach der Änderung.",
    wie: "Fed: Obergrenze des Zielbands für den Tagesgeldsatz (FRED DFEDTARU). EZB: Zinssatz der Einlagefazilität (FRED ECBDFR). Negative Werte sind möglich (EZB 2014 bis 2022).",
  },
  reaktionstag: {
    titel: "Reaktionstag",
    was: "Der erste Handelstag nach dem Wirksamkeitstag. An diesem Tag wird die Kursreaktion gemessen.",
  },
  kursVorher: {
    titel: "Kurs vorher",
    was: "Schlusskurs am Handelstag vor dem Reaktionstag.",
  },
  kursNachher: {
    titel: "Kurs nachher",
    was: "Schlusskurs am Reaktionstag.",
  },
  veraenderungEreignis: {
    titel: "Veränderung",
    was: "Wie stark sich der Kurs am Reaktionstag verändert hat.",
    wie: "Kurs nachher ÷ Kurs vorher − 1. Grün bei einem Anstieg, rot bei einem Rückgang.",
  },
  kursdiagramm: {
    titel: "Kursdiagramm",
    was: "Kerzendiagramm: Jede Kerze ist ein Handelstag.",
    wie: "Der dünne Strich (Docht) reicht vom Low (Tiefstkurs) bis zum High (Höchstkurs), der dicke Körper vom Open (Eröffnungskurs) bis zum Close (Schlusskurs). Grün: Close ≥ Open (gestiegen), rot: gefallen. Gestrichelte waagrechte Linien „S …“ und „R …“ sind Support und Resistance, gelbe senkrechte Linien Zinsänderungen, gepunktete Linien P, R1, R2, S1, S2 Pivot-Punkte. Fährt man mit der Maus über das Diagramm, zeigt der Kasten oben links die Werte des Tages (O, H, L, C und die Linien).",
  },
  zeichenwerkzeuge: {
    titel: "Zeichenwerkzeuge",
    was: "Eigene Linien für die technische Analyse. Sie werden in diesem Browser gespeichert, getrennt je Wertpapier.",
    wie: "Trendlinie: zwei Punkte. Strahl: wie eine Trendlinie, aber bis zum Rand verlängert. Waagrechte: ein Klick auf den Preis. Zone: zwei gegenüberliegende Ecken. Fibonacci: zwei Punkte, dazu Linien bei 0, 23,6, 38,2, 50, 61,8, 78,6 und 100 % des Wegs dazwischen (0 % beim zweiten, 100 % beim ersten Klick). Magnet: Ein Klick rastet auf Open, High, Low oder Close der Kerze ein, wenn er höchstens 15 Pixel entfernt ist. Auswählen per Klick, Löschen mit Entf, Rückgängig mit Strg+Z, Abbrechen mit Esc.",
    lesen: "Fibonacci-Stufen gelten bei Rücksetzern als mögliche Wendemarken; am meisten beachtet werden 38,2 %, 50 % und 61,8 %.",
    beratung: true,
  },
  anbieter: {
    titel: "Anbieter der Kursdaten",
    was: "Der Anbieter, von dem dieser Server seine Kurse bezieht.",
    wie: "Jeder Server von MarketLens arbeitet mit genau einem Anbieter (Programmoption --quelle). Alpha Vantage liefert alle Wertpapiere des Katalogs, Twelve Data im kostenlosen Tarif nur die US-Wertpapiere. Laufen zwei Server nebeneinander (z. B. Port 8080 und 8081), hat jeder eigene Kursdaten, einen eigenen Zwischenspeicher und eigene Zeichnungen; nur Katalog und Zinsdaten sind gemeinsam.",
    lesen: "Die Schlusskurse beider Anbieter stimmen bis auf Rundungen (etwa 0,01) überein. Das gehandelte Volumen zählen die Anbieter unterschiedlich.",
    aktuell: "Aktuell: {anbieter}.",
  },
  veraltet: {
    titel: "Ältere Daten",
    was: "Die Daten stammen aus dem Zwischenspeicher und sind älter als ein Tag, weil keine aktuellen Daten abgerufen werden konnten (Netz nicht erreichbar oder Anfragekontingent des Anbieters erschöpft).",
    wie: "Die Uhrzeit im Hinweis ist der Zeitpunkt des letzten erfolgreichen Abrufs. Sobald ein Abruf wieder klappt, werden aktuelle Daten angezeigt.",
  },
};

// Erklärungsfenster: öffnet beim Überfahren eines „?“ mit der Maus, ersatzweise bei Tastatur-Fokus oder Antippen
const Hilfe = (() => {
  // Wartezeit vor dem Schließen (ms), damit die Maus vom „?“ ins Fenster wechseln kann, ohne dass es flackert
  const SCHLIESSEN_NACH = 200;
  // Hinweis unter Erklärungen mit üblicher Deutung
  const BERATUNG = "Übliche Deutung, keine Anlageberatung.";

  // Das Fenster (ein einziges für alle „?“)
  let fenster = null;
  // „?“, zu dem das Fenster gerade gehört
  let offenBei = null;
  // Zeitgeber für das verzögerte Schließen
  let zeitgeber = 0;
  // Funktion aus app.js, die die aktuellen Werte für die Platzhalter liefert
  let werteHolen = () => ({});

  // Ersetzt {name} durch den Wert; fehlt der Wert, entscheidet "ersatz" (null = ganzen Text verwerfen)
  function fuelle(text, werte, ersatz) {
    // Ob ein Wert gefehlt hat
    let fehlt = false;
    // Alle Platzhalter ersetzen
    const ergebnis = text.replace(/\{(\w+)\}/g, (_, name) => {
      // Wert vorhanden?
      const wert = werte[name];
      // Vorhanden: als Text einsetzen
      if (wert !== undefined && wert !== null && wert !== "") return String(wert);
      // Fehlt
      fehlt = true;
      // Ersatztext (bei null ist es egal, der Text wird verworfen)
      return ersatz === null ? "" : ersatz;
    });
    // Bei ersatz = null und fehlendem Wert: gar nicht anzeigen
    return fehlt && ersatz === null ? null : ergebnis;
  }

  // Erzeugt ein „?“ für einen Schlüssel (für Zeilen, die app.js selbst baut)
  function knopf(schluessel) {
    // Knopf, damit er mit der Tastatur erreichbar ist
    const k = document.createElement("button");
    // Kein Absenden von Formularen
    k.type = "button";
    // Gestaltung und Schlüssel
    k.className = "hilfe";
    k.dataset.hilfe = schluessel;
    // Zeichen
    k.textContent = "?";
    // Beschriftung für Bildschirmleser
    beschrifte(k);
    // Fertig
    return k;
  }

  // Setzt aria-label "Erklärung zu …" nach dem Titel des Eintrags
  function beschrifte(k) {
    // Eintrag
    const eintrag = ERKLAERUNGEN[k.dataset.hilfe];
    // Beschriftung (auch bei unbekanntem Schlüssel, damit der Knopf benannt ist)
    k.setAttribute("aria-label", `Erklärung zu ${eintrag ? eintrag.titel : k.dataset.hilfe}`);
    // Zustand für Bildschirmleser
    k.setAttribute("aria-expanded", "false");
  }

  // Zeigt die Erklärung zu einem „?“
  function zeige(k) {
    // Schließen abbrechen, falls geplant
    clearTimeout(zeitgeber);
    // Schon offen für diesen Knopf: nichts zu tun
    if (offenBei === k && !fenster.hidden) return;
    // Anderer Knopf war offen: dessen Zustand zurücksetzen
    if (offenBei) offenBei.setAttribute("aria-expanded", "false");
    // Eintrag holen
    const eintrag = ERKLAERUNGEN[k.dataset.hilfe];
    // Unbekannter Schlüssel: nichts anzeigen
    if (!eintrag) return;
    // Aktuelle Werte für die Platzhalter
    const werte = werteHolen();
    // Inhalt leeren
    fenster.replaceChildren();
    // Überschrift
    const titel = document.createElement("strong");
    titel.textContent = eintrag.titel;
    fenster.append(titel);
    // Absätze: was, wie, lesen (fehlende Werte als "…"), aktuell (nur wenn alle Werte da sind)
    for (const [teil, ersatz] of [["was", "…"], ["wie", "…"], ["lesen", "…"], ["aktuell", null]]) {
      // Teil vorhanden?
      if (!eintrag[teil]) continue;
      // Platzhalter füllen
      const text = fuelle(eintrag[teil], werte, ersatz);
      // Verworfen?
      if (text === null) continue;
      // Absatz als reiner Text (nie als HTML)
      const p = document.createElement("p");
      p.textContent = text;
      // Aktueller Wert hervorgehoben
      if (teil === "aktuell") p.className = "aktuell";
      // Anhängen
      fenster.append(p);
    }
    // Hinweis auf keine Anlageberatung
    if (eintrag.beratung) {
      // Kleiner Absatz
      const p = document.createElement("p");
      p.className = "beratung";
      p.textContent = BERATUNG;
      fenster.append(p);
    }
    // Merken und anzeigen
    offenBei = k;
    k.setAttribute("aria-expanded", "true");
    fenster.hidden = false;
    // Neben das „?“ setzen
    positioniere(k);
  }

  // Setzt das Fenster unter (oder, wenn kein Platz ist, über) das „?“ und hält es im sichtbaren Bereich
  function positioniere(k) {
    // Abstand zwischen „?“ und Fenster bzw. zwischen Fenster und Bildschirmrand
    const abstand = 6;
    const rand = 8;
    // Lage des „?“ im Fenster des Browsers
    const r = k.getBoundingClientRect();
    // Zuerst oben links in die Seite setzen und eine frühere Höhenbegrenzung aufheben. Wichtig: Nur so hat das
    // Fenster beim Messen seine volle Breite. An der alten Stelle (z. B. nahe am rechten Rand) würde der Browser es
    // schmaler machen, der Text würde anders umbrechen, und die gemessene Höhe stimmte nicht mit der späteren überein.
    fenster.style.left = "0px";
    fenster.style.top = "0px";
    fenster.style.maxHeight = "";
    // Größe des Erklärungsfensters (jetzt, wo es Inhalt und volle Breite hat)
    const breite = fenster.offsetWidth;
    const hoehe = fenster.offsetHeight;
    // Links bündig mit dem „?“, aber mindestens 8 px Abstand zu beiden Rändern
    const links = Math.max(rand, Math.min(r.left, window.innerWidth - breite - rand));
    // Freier Platz unter und über dem „?“ (bis 8 px vor dem Bildschirmrand)
    const platzUnten = window.innerHeight - r.bottom - abstand - rand;
    const platzOben = r.top - abstand - rand;
    // Hier landet die obere Kante des Fensters (in Fensterkoordinaten)
    let oben;
    if (hoehe <= platzUnten) {
      // Passt darunter: darunter
      oben = r.bottom + abstand;
    } else if (hoehe <= platzOben) {
      // Passt darüber: darüber
      oben = r.top - abstand - hoehe;
    } else if (platzUnten >= platzOben) {
      // Passt nirgends ganz (langer Text, niedriges Fenster): auf der Seite mit mehr Platz,
      // Höhe begrenzen, der Rest lässt sich im Fenster scrollen (overflow-y in style.css)
      fenster.style.maxHeight = `${Math.max(platzUnten, 60)}px`;
      oben = r.bottom + abstand;
    } else {
      // Darüber ist mehr Platz: dort, ebenfalls mit begrenzter Höhe
      const begrenzt = Math.max(platzOben, 60);
      fenster.style.maxHeight = `${begrenzt}px`;
      oben = r.top - abstand - Math.min(hoehe, begrenzt);
    }
    // Seitenkoordinaten = Fensterkoordinaten + Bildlauf
    fenster.style.left = `${links + window.scrollX}px`;
    fenster.style.top = `${oben + window.scrollY}px`;
  }

  // Schließt das Fenster sofort
  function schliesse() {
    // Zeitgeber stoppen
    clearTimeout(zeitgeber);
    // Ausblenden
    if (fenster) fenster.hidden = true;
    // Zustand des Knopfs zurücksetzen
    if (offenBei) offenBei.setAttribute("aria-expanded", "false");
    // Kein Knopf mehr offen
    offenBei = null;
  }

  // Schließt nach kurzer Wartezeit (wird abgebrochen, wenn die Maus ins Fenster oder auf ein „?“ kommt)
  function planeSchliessen() {
    // Alten Zeitgeber ersetzen
    clearTimeout(zeitgeber);
    // Neu planen
    zeitgeber = setTimeout(schliesse, SCHLIESSEN_NACH);
  }

  // Einmal beim Start: Fenster anlegen, vorhandene „?“ beschriften, Ereignisse verbinden
  function init(werteFunktion) {
    // Funktion für die aktuellen Werte merken
    if (werteFunktion) werteHolen = werteFunktion;
    // Fenster anlegen (am Ende der Seite, damit es über allem liegt)
    fenster = document.createElement("div");
    fenster.id = "erklaerung";
    fenster.className = "erklaerung";
    fenster.setAttribute("role", "tooltip");
    fenster.hidden = true;
    document.body.append(fenster);
    // „?“ aus index.html beschriften
    for (const k of document.querySelectorAll(".hilfe")) beschrifte(k);

    // Ereignisse werden am Dokument abgefangen ("Delegation"): So funktionieren auch „?“, die app.js später erzeugt.
    // Maus kommt auf ein „?“ oder ins Fenster: öffnen bzw. offen lassen
    document.addEventListener("mouseover", (e) => {
      // Ziel-Element
      const ziel = e.target instanceof Element ? e.target : null;
      // Kein Element (z. B. Text)
      if (!ziel) return;
      // „?“ unter der Maus?
      const k = ziel.closest(".hilfe");
      // Ja: dessen Erklärung zeigen
      if (k) { zeige(k); return; }
      // Maus im Fenster: Schließen abbrechen
      if (ziel.closest("#erklaerung")) clearTimeout(zeitgeber);
    });
    // Maus verlässt ein „?“ oder das Fenster: verzögert schließen
    document.addEventListener("mouseout", (e) => {
      // Ziel-Element
      const ziel = e.target instanceof Element ? e.target : null;
      // Nur, wenn es um ein „?“ oder das Fenster geht
      if (ziel && ziel.closest(".hilfe, #erklaerung")) planeSchliessen();
    });
    // Tastatur: Fokus auf einem „?“ öffnet, Verlassen schließt verzögert
    document.addEventListener("focusin", (e) => {
      // „?“ mit Fokus?
      const k = e.target instanceof Element ? e.target.closest(".hilfe") : null;
      // Öffnen
      if (k) zeige(k);
    });
    document.addEventListener("focusout", (e) => {
      // War der Fokus auf einem „?“?
      if (e.target instanceof Element && e.target.closest(".hilfe")) planeSchliessen();
    });
    // Klick bzw. Antippen: auf ein „?“ öffnet (für Touch-Geräte ohne Überfahren), anderswo schließt
    document.addEventListener("click", (e) => {
      // Ziel-Element
      const ziel = e.target instanceof Element ? e.target : null;
      // „?“ angeklickt?
      const k = ziel ? ziel.closest(".hilfe") : null;
      // Ja: öffnen; preventDefault verhindert, dass ein umgebendes <label> sein Kästchen umschaltet
      if (k) { e.preventDefault(); zeige(k); return; }
      // Klick ins Fenster: offen lassen
      if (ziel && ziel.closest("#erklaerung")) return;
      // Klick woanders: schließen
      schliesse();
    });
    // Esc schließt
    document.addEventListener("keydown", (e) => {
      // Nur Esc
      if (e.key === "Escape") schliesse();
    });
    // Beim Bildlauf oder neuer Fenstergröße schließen (die Lage würde sonst nicht mehr stimmen)
    window.addEventListener("scroll", schliesse, { passive: true });
    window.addEventListener("resize", schliesse);
  }

  // Öffentliche Funktionen
  return { init, knopf };
})();
