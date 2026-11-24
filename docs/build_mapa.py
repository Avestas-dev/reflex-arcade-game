#!/usr/bin/env python3
"""Builds docs/mapa-przyciskow.html (button layout map). Render to PDF with Chrome:

    python3 docs/build_mapa.py
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new \
        --no-pdf-header-footer --print-to-pdf=docs/Mapa-przyciskow.pdf docs/mapa-przyciskow.html

Edit LAYOUT or the pin lists if your panel differs. Pins must match reflex-arcade-mega.ino.
"""
import pathlib

# Button number -> (column, row), seen from the player's own seat. Column 0 = player's left, row 0 = top (far).
LAYOUT = {1: (0, 0), 2: (0, 1), 3: (0, 2), 4: (1, 0), 5: (1, 1), 6: (1, 2), 7: (2, 0), 8: (2, 1)}
PLAYERS = [
    {"name": "Gracz 1", "color": "#00b8d4", "btn": list(range(22, 30)), "led": list(range(38, 46))},
    {"name": "Gracz 2", "color": "#d6339c", "btn": list(range(30, 38)), "led": list(range(46, 54))},
]
ROW_NAMES = ["góra", "środek", "dół"]
COL_NAMES = ["lewa", "środek", "prawa"]


def panel_svg(pl, scale=1.0, show_title=True):
    """One player's 3-3-2 panel as seen by that player."""
    cw, rh, r = 118, 108, 40
    w, h = 3 * cw + 30, 3 * rh + (52 if show_title else 20)
    top = 44 if show_title else 12
    out = [f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg">']
    out.append(f'<rect x="4" y="{top-8}" width="{w-8}" height="{3*rh+8}" rx="16" fill="#151a33"/>')
    if show_title:
        out.append(f'<text x="{w/2}" y="24" text-anchor="middle" font-size="19" font-weight="800" fill="{pl["color"]}">{pl["name"]}</text>')
    for n, (c, rr) in LAYOUT.items():
        cx, cy = 15 + c * cw + cw / 2, top + rr * rh + rh / 2 - 4
        out.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{pl["color"]}" stroke="#fff" stroke-width="3"/>')
        out.append(f'<text x="{cx}" y="{cy-6}" text-anchor="middle" font-size="25" font-weight="900" fill="#0b0f22">#{n}</text>')
        out.append(f'<text x="{cx}" y="{cy+14}" text-anchor="middle" font-size="14" font-weight="800" fill="#fff">D{pl["btn"][n-1]}</text>')
        out.append(f'<rect x="{cx-30}" y="{cy+r+4}" width="60" height="19" rx="9" fill="#f2a900"/>')
        out.append(f'<text x="{cx}" y="{cy+r+18}" text-anchor="middle" font-size="12.5" font-weight="800" fill="#0b0f22">☀ D{pl["led"][n-1]}</text>')
    out.append('</svg>')
    return "".join(out)


def table_view():
    """Top view of the whole table: player 1 at the bottom, player 2 at the top (rotated 180 degrees)."""
    W, H = 640, 450
    s = 0.5
    pw, ph = 384 * s, 344 * s
    x0 = (W - pw) / 2
    inner = lambda pl: panel_svg(pl, show_title=False).split('>', 1)[1].rsplit('</svg>', 1)[0]
    y1 = H - ph - 44          # player 1 panel (bottom)
    y2 = 44                   # player 2 panel (top, rotated)
    return f'''<svg viewBox="0 0 {W} {H}" width="100%" xmlns="http://www.w3.org/2000/svg">
  <rect x="110" y="8" width="{W-220}" height="{H-16}" rx="22" fill="#eef1fa" stroke="#c9cee3" stroke-width="2"/>
  <g transform="translate({x0},{y1}) scale({s})">{inner(PLAYERS[0])}</g>
  <g transform="translate({x0+pw},{y2+ph}) rotate(180) scale({s})">{inner(PLAYERS[1])}</g>
  <text x="{W/2}" y="{H-18}" text-anchor="middle" font-size="15" font-weight="800" fill="#00b8d4">▲ tu stoi GRACZ 1 ▲</text>
  <text x="{W/2}" y="32" text-anchor="middle" font-size="15" font-weight="800" fill="#d6339c" transform="rotate(180 {W/2} 26)">▲ tu stoi GRACZ 2 ▲</text>
  <g font-size="12" fill="#5b6385">
    <text x="{x0-10}" y="{y1+ph/2}" text-anchor="end">lewa ręka</text><text x="{x0-10}" y="{y1+ph/2+15}" text-anchor="end">gracza 1 ◂</text>
    <text x="{x0+pw+10}" y="{y2+ph/2}">lewa ręka</text><text x="{x0+pw+10}" y="{y2+ph/2+15}">▸ gracza 2</text>
  </g>
  <text x="24" y="{H/2}" font-size="12" fill="#5b6385" transform="rotate(-90 24 {H/2})" text-anchor="middle">widok z góry na cały stół</text>
</svg>'''


def rows(pl):
    trs = []
    for n in range(1, 9):
        c, rr = LAYOUT[n]
        trs.append(
            f'<tr><td><span class="num" style="background:{pl["color"]}">#{n}</span></td>'
            f'<td style="white-space:nowrap">kol. <b>{c+1}</b> · rząd <b>{rr+1}</b><br><span class="small">{COL_NAMES[c]} / {ROW_NAMES[rr]}</span></td>'
            f'<td><b>D{pl["btn"][n-1]}</b><br><span class="small">+ GND</span></td>'
            f'<td><b class="gold">D{pl["led"][n-1]}</b><br><span class="small">+ 220 Ω</span></td>'
            f'<td class="box">☐</td><td class="box">☐</td></tr>')
    return "".join(trs)


HTML = f'''<!doctype html>
<html lang="pl"><head><meta charset="utf-8"><title>Reflex Arcade – mapa przycisków</title>
<style>
@page {{ size: A4; margin: 0; }}
:root{{--ink:#141a33;--muted:#5b6385;--line:#dfe3f0;--soft:#f4f6fc;--navy:#0f1430;--cyan:#00b8d4;--mag:#d6339c;--gold:#f2a900;--green:#1e9e5a;--red:#e0384f}}
*{{box-sizing:border-box;margin:0;padding:0}}
body{{font-family:-apple-system,"SF Pro Text","Helvetica Neue",Arial,sans-serif;color:var(--ink);font-size:9pt;line-height:1.4;-webkit-print-color-adjust:exact;print-color-adjust:exact}}
.page{{width:210mm;height:297mm;position:relative;overflow:hidden;page-break-after:always;padding:0 12mm 10mm}}
.page:last-child{{page-break-after:auto}}
.top{{height:13mm;margin:0 -12mm 5mm;padding:0 12mm;background:var(--navy);color:#fff;display:flex;align-items:center;justify-content:space-between;font-size:8pt;letter-spacing:.04em}}
.top b{{background:linear-gradient(90deg,#3ef0ff,#ff5ed1);-webkit-background-clip:text;background-clip:text;color:transparent;font-weight:800;font-size:9.5pt}}
.top .pg{{display:flex;gap:4px}}.top .pg i{{width:18px;height:4px;border-radius:2px;background:#39406b}}.top .pg i.on{{background:#3ef0ff}}
h1{{font-size:22pt;line-height:1.08;letter-spacing:-.02em;margin-bottom:2mm}}
h1 em{{font-style:normal;background:linear-gradient(90deg,var(--cyan),var(--mag));-webkit-background-clip:text;background-clip:text;color:transparent}}
h2{{font-size:12pt;margin-bottom:2mm}}
.lead{{font-size:9.8pt;color:var(--muted);margin-bottom:4mm}}
.grid2{{display:grid;grid-template-columns:1fr 1fr;gap:5mm}}
.card{{border:1px solid var(--line);border-radius:3mm;padding:3mm 3.5mm}}
.legend{{display:flex;gap:6mm;flex-wrap:wrap;font-size:8.8pt;margin:3mm 0}}
.legend span{{display:inline-flex;align-items:center;gap:2mm}}
.dot{{width:12px;height:12px;border-radius:50%;display:inline-block}}
table{{width:100%;border-collapse:collapse;font-size:8.8pt}}
th{{text-align:left;font-size:7.2pt;text-transform:uppercase;letter-spacing:.06em;color:var(--muted);padding:1.2mm 1.4mm;border-bottom:1.5px solid var(--ink)}}
td{{padding:1.1mm 1.4mm;border-bottom:1px solid var(--line);vertical-align:middle}}
.num{{display:inline-block;min-width:24px;text-align:center;border-radius:6px;color:#0b0f22;font-weight:900;padding:1px 4px}}
.small{{font-size:7.4pt;color:var(--muted)}}
.gold{{color:#b07a00}}
.box{{font-size:13pt;text-align:center;color:#8a90b5}}
.call{{border-radius:3mm;padding:2.6mm 3.4mm;margin-top:3.5mm;border-left:4px solid}}
.call.info{{background:#eefaff;border-color:var(--cyan)}}.call.warn{{background:#fff4f5;border-color:var(--red)}}.call.tip{{background:#effbf5;border-color:var(--green)}}
.call h3{{font-size:9.6pt;margin-bottom:.8mm}}
.foot{{position:absolute;bottom:5mm;left:12mm;right:12mm;font-size:7pt;color:#9aa0bd;display:flex;justify-content:space-between}}
svg text{{font-family:-apple-system,"Helvetica Neue",Arial,sans-serif}}
code{{font-family:"SF Mono",Menlo,monospace;font-size:8pt;background:#eef0f8;border-radius:3px;padding:0 3px}}
</style></head><body>

<section class="page">
  <div class="top"><span><b>REFLEX ARCADE</b> &nbsp;·&nbsp; Mapa przycisków i pinów</span><span class="pg"><i class="on"></i><i></i></span></div>
  <h1>Mapa przycisków: <em>gdzie który pin</em></h1>
  <p class="lead">Każdy gracz ma 8 przycisków w układzie <b>3 – 3 – 2</b>. Numery liczymy <b>od lewej ręki gracza</b>, kolumnami, od góry. Numer <b>#N</b> jest taki sam jak na stronie gry (zakładka Diagnostyka). Obaj gracze mają ten sam układ, każdy patrząc ze swojej strony.</p>

  <div class="grid2">
    <div class="card" style="text-align:center">{panel_svg(PLAYERS[0], scale=0.98)}<div class="small" style="margin-top:1mm">widok oczami gracza 1</div></div>
    <div class="card" style="text-align:center">{panel_svg(PLAYERS[1], scale=0.98)}<div class="small" style="margin-top:1mm">widok oczami gracza 2</div></div>
  </div>

  <div class="legend">
    <span><i class="dot" style="background:#141a33"></i><b>Biały pin w kółku</b> = przycisk (druga nóżka do GND)</span>
    <span><i class="dot" style="background:var(--gold)"></i><b>☀ Żółty pin</b> = światło (pin → 220 Ω → LED → GND)</span>
  </div>

  <div class="card" style="padding:2mm;text-align:center"><div style="width:150mm;margin:0 auto">{table_view()}</div></div>

  <div class="call info">
    <h3>Najważniejsze przy podłączaniu</h3>
    <p style="margin:0">Gracz 2 stoi naprzeciwko, więc jego <b>#1 jest w rogu po przekątnej</b> względem #1 gracza 1 (patrz widok z góry). Jeśli u Ciebie dwa przyciski 3. kolumny siedzą w innych rzędach, numeracja zostaje: <b>#7 to wyższy, #8 niższy</b>.</p>
  </div>
  <div class="foot"><span>Reflex Arcade · github.com/Avestas-dev/reflex-arcade-game</span><span>1 / 2</span></div>
</section>

<section class="page">
  <div class="top"><span><b>REFLEX ARCADE</b> &nbsp;·&nbsp; Mapa przycisków i pinów</span><span class="pg"><i></i><i class="on"></i></span></div>
  <h1>Lista połączeń <em>do odhaczania</em></h1>
  <p class="lead">Podłączaj po kolei i odhaczaj. W kolumnie <b>Test</b> zaznacz, gdy w zakładce <b>Diagnostyka</b> wciśnięcie podbija licznik właściwego pola, a „Włącz światło” zapala właściwą lampkę.</p>

  <div class="grid2">
    <div class="card"><h2 style="color:var(--cyan)">Gracz 1</h2>
      <table><tr><th>#</th><th>Miejsce</th><th>Przycisk</th><th>Światło</th><th>Podł.</th><th>Test</th></tr>{rows(PLAYERS[0])}</table></div>
    <div class="card"><h2 style="color:var(--mag)">Gracz 2</h2>
      <table><tr><th>#</th><th>Miejsce</th><th>Przycisk</th><th>Światło</th><th>Podł.</th><th>Test</th></tr>{rows(PLAYERS[1])}</table></div>
  </div>

  <div class="grid2" style="margin-top:4mm">
    <div class="card">
      <h2>Jeden przycisk z lampką (4 styki)</h2>
      <svg viewBox="0 0 330 190" width="100%">
        <circle cx="85" cy="68" r="46" fill="#00b8d4" stroke="#141a33" stroke-width="3"/>
        <text x="85" y="74" text-anchor="middle" font-size="16" font-weight="900" fill="#0b0f22">#1</text>
        <rect x="42" y="120" width="16" height="12" rx="2" fill="#8a90b5"/><rect x="62" y="120" width="16" height="12" rx="2" fill="#8a90b5"/>
        <rect x="92" y="120" width="16" height="12" rx="2" fill="#f2a900"/><rect x="112" y="120" width="16" height="12" rx="2" fill="#f2a900"/>
        <g fill="none" stroke-width="2.2">
          <path d="M50 132 V162 H14" stroke="#141a33"/>
          <path d="M70 132 V178 H286" stroke="#141a33" stroke-dasharray="6 3"/>
          <path d="M100 132 V178" stroke="#141a33" stroke-dasharray="6 3"/>
          <path d="M120 132 V146 H196" stroke="#b07a00"/>
          <path d="M232 146 H262" stroke="#b07a00"/>
        </g>
        <rect x="196" y="138" width="36" height="16" rx="3" fill="#fff" stroke="#141a33" stroke-width="2"/>
        <g font-size="11.5" font-weight="800">
          <text x="14" y="156">D22</text>
          <text x="214" y="132" text-anchor="middle" font-size="10" font-weight="700">220 Ω</text>
          <text x="266" y="150" fill="#b07a00">D38</text>
          <text x="290" y="182">GND</text>
        </g>
        <g font-size="9.5" fill="#5b6385">
          <text x="160" y="40">szare: przełącznik</text>
          <text x="160" y="54">żółte: lampka (+ prawy, − lewy)</text>
          <text x="160" y="68">przerywana: wspólna masa</text>
        </g>
      </svg>
      <p class="small" style="margin:0">Styki przełącznika nie mają kierunku. Lampka ma: <b>+</b> idzie przez rezystor do pinu światła, a <b>−</b> do GND. Jeśli lampka nie świeci, odwróć jej przewody.</p>
    </div>
    <div>
      <div class="call tip" style="margin-top:0">
        <h3>Wspólna masa (GND)</h3>
        <p style="margin:0">Nie potrzeba 32 przewodów do GND. Poprowadź jeden przewód masy od przycisku do przycisku (tzw. „łańcuszek”) i podłącz go raz do dowolnego pinu <b>GND</b> płytki. Najlepiej po jednym łańcuszku na gracza.</p>
      </div>
      <div class="call warn">
        <h3>Lampki 12 V w przyciskach arcade</h3>
        <p style="margin:0">Jeśli lampki są na 12 V (często są), <b>nie podłączaj ich bezpośrednio do pinów</b>. Użyj sterownika ULN2803 (8 kanałów, po jednym na gracza) i zasilacza 12 V. Opis jest w poradniku, na stronie 3.</p>
      </div>
      <div class="call info">
        <h3>Zmiana pinów</h3>
        <p style="margin:0">Piny są ustawione na górze pliku <code>reflex-arcade-mega.ino</code> (<code>BTN_PINS</code>, <code>LED_PINS</code>). Po zmianie strona sama pokaże nowe numery.</p>
      </div>
    </div>
  </div>
  <div class="foot"><span>Reflex Arcade · Mapa przycisków</span><span>2 / 2</span></div>
</section>
</body></html>'''

out = pathlib.Path(__file__).with_name("mapa-przyciskow.html")
out.write_text(HTML, encoding="utf-8")
print("wrote", out)
