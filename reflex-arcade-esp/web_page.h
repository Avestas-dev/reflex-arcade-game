#pragma once
#include <pgmspace.h>

// The control page served by the ESP8266. tools/dev_bridge.py serves the same
// page from this file, so there is only one copy to edit.

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Reflex Arcade</title>
<style>
:root{--bg:#0b0d17;--panel:#151a2e;--line:#262c4a;--text:#e8ebff;--muted:#8a90b5;--p1:#3ef0ff;--p2:#ff5ed1;--ok:#3dff8a;--bad:#ff4d6d;--gold:#ffd23e}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
header{display:flex;align-items:center;gap:12px;padding:12px 16px;background:var(--panel);border-bottom:1px solid var(--line);position:sticky;top:0;z-index:5}
h1{margin:0;font-size:18px;letter-spacing:.02em}
h1 b{background:linear-gradient(90deg,var(--p1),var(--p2));-webkit-background-clip:text;background-clip:text;color:transparent}
.pill{margin-left:auto;display:flex;align-items:center;gap:6px;font-size:12px;color:var(--muted)}
.dot{width:10px;height:10px;border-radius:50%;background:var(--bad);box-shadow:0 0 8px var(--bad)}
.pill.on .dot{background:var(--ok);box-shadow:0 0 8px var(--ok)}
nav{display:flex;gap:4px;padding:10px 16px 0}
nav button{flex:1;padding:10px;border:0;border-radius:10px 10px 0 0;background:transparent;color:var(--muted);font-weight:600;font-size:15px;cursor:pointer}
nav button.active{background:var(--panel);color:var(--text)}
main{padding:0 16px 32px;max-width:900px;margin:0 auto}
section{display:none}section.active{display:block}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:14px;margin-top:12px}
.row{display:flex;flex-wrap:wrap;align-items:center;gap:8px;margin:8px 0}
.row>label{width:120px;color:var(--muted);font-size:13px}
.seg{display:flex;flex-wrap:wrap;gap:6px}
.seg button,.btn{padding:9px 14px;border:1px solid var(--line);border-radius:10px;background:#1c2240;color:var(--text);font-size:14px;cursor:pointer}
.seg button.sel{border-color:var(--p1);background:#123a48;color:#fff}
.seg button:disabled,.btn:disabled{opacity:.4;cursor:not-allowed}
.btn.go{background:linear-gradient(90deg,var(--p1),#7b8cff);color:#0b0d17;font-weight:700;border:0;padding:12px 28px;font-size:16px}
.btn.stop{background:var(--bad);border:0;color:#fff;font-weight:700;padding:12px 28px;font-size:16px}
.actions{display:flex;gap:10px;margin-top:12px;flex-wrap:wrap}
.status{display:flex;align-items:baseline;justify-content:space-between;gap:12px}
#timer{font-size:48px;font-weight:800;font-variant-numeric:tabular-nums}
#phase{color:var(--muted);font-size:14px}
.players{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px}
.player{position:relative;transition:box-shadow .15s}
.player h3{margin:0 0 4px;font-size:15px}
.player.p0 h3{color:var(--p1)}.player.p1 h3{color:var(--p2)}
.score{font-size:44px;font-weight:800;font-variant-numeric:tabular-nums;line-height:1.1}
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin:8px 0;font-size:12px;color:var(--muted)}
.stats b{display:block;color:var(--text);font-size:16px;font-variant-numeric:tabular-nums}
.lamps{display:grid;grid-template-columns:repeat(8,1fr);gap:6px;max-width:420px}
.lamp{aspect-ratio:1;border-radius:50%;background:#20263f;border:2px solid #2c3356;display:flex;align-items:center;justify-content:center;font-size:11px;color:var(--muted);transition:background .08s}
.p0 .lamp.on{background:var(--p1);box-shadow:0 0 12px var(--p1);color:#0b0d17}
.p1 .lamp.on{background:var(--p2);box-shadow:0 0 12px var(--p2);color:#0b0d17}
.lamp.hit{border-color:var(--ok)}.lamp.miss{border-color:var(--bad)}
.player.flash-hit{box-shadow:0 0 0 2px var(--ok)}
.player.flash-miss{box-shadow:0 0 0 2px var(--bad)}
.player.winner::after{content:"ZWYCIĘZCA";position:absolute;top:12px;right:14px;color:var(--gold);font-weight:800;font-size:12px;letter-spacing:.1em}
.last{font-size:12px;color:var(--muted);height:16px}
#overlay{position:fixed;inset:0;display:none;align-items:center;justify-content:center;background:rgba(11,13,23,.7);font-size:160px;font-weight:900;z-index:10;pointer-events:none}
#overlay.show{display:flex}
.note{color:var(--muted);font-size:13px}
.diag-grid{display:grid;grid-template-columns:repeat(8,1fr);gap:6px;margin:6px 0 14px}
.cell{border:2px solid var(--line);border-radius:10px;padding:8px 2px;text-align:center;cursor:pointer;background:#1a1f38;user-select:none}
.cell .n{font-size:11px;color:var(--muted)}
.cell .c{font-size:18px;font-weight:700;font-variant-numeric:tabular-nums}
.cell .l{width:12px;height:12px;border-radius:50%;margin:4px auto 0;background:#2c3356}
.cell .pin{font-size:11px;font-weight:700;color:var(--text);font-variant-numeric:tabular-nums}
.cell .pin.led{font-weight:400;color:var(--gold);margin-top:2px}
.pins-line{font-size:14px}.pins-line b{color:var(--gold)}
.cell.lit .l{background:var(--gold);box-shadow:0 0 8px var(--gold)}
.cell.down{border-color:var(--ok);background:#143b2a}
.cell.sel{outline:2px solid var(--p1);outline-offset:1px}
.detail{display:flex;flex-wrap:wrap;gap:14px;align-items:center}
.detail .big{font-size:28px;font-weight:800}
.badge{padding:3px 8px;border-radius:6px;font-size:12px;font-weight:700;background:#2a3050}
.badge.on{background:var(--ok);color:#0b0d17}
@media(max-width:520px){.row>label{width:100%}#timer{font-size:40px}.score{font-size:36px}.cell .c{font-size:15px}}
</style>
</head>
<body>
<header>
  <h1>Reflex <b>Arcade</b></h1>
  <div class="pill" id="conn"><span class="dot"></span><span id="connText">Łączenie…</span></div>
</header>
<nav>
  <button class="active" data-tab="game">Gra</button>
  <button data-tab="diag">Diagnostyka</button>
</nav>
<main>
<section id="game" class="active">
  <div class="card" id="settings">
    <div class="row"><label>Tryb</label><div class="seg" data-key="multi">
      <button data-v="0">1 gracz</button><button data-v="1">2 graczy</button></div></div>
    <div class="row"><label>Świateł naraz</label><div class="seg" data-key="n">
      <button data-v="1">1</button><button data-v="2">2</button><button data-v="3">3</button></div></div>
    <div class="row" id="patRow"><label>Cele</label><div class="seg" data-key="same">
      <button data-v="1">Te same dla obu (wyścig)</button><button data-v="0">Losowe dla każdego</button></div></div>
    <div class="row"><label>Czas</label><div class="seg" data-key="dur">
      <button data-v="30">30 s</button><button data-v="45">45 s</button><button data-v="60">1 min</button>
      <button data-v="120">2 min</button><button data-v="180">3 min</button><button data-v="300">5 min</button></div></div>
    <div class="actions">
      <button class="btn go" id="startBtn">Start</button>
      <button class="btn stop" id="stopBtn">Stop</button>
    </div>
    <p class="note" id="sharedNote" hidden>Płytka ma wspólne 8 świateł dla obu graczy, więc w trybie 2 graczy cele są zawsze te same.</p>
  </div>

  <div class="card status">
    <div id="timer">0:00</div>
    <div id="phase">Gotowe</div>
  </div>

  <div class="players">
    <div class="card player p0" id="pc0">
      <h3>Gracz 1</h3><div class="score" id="s0">0</div>
      <div class="stats"><div>Trafienia<b id="h0">0</b></div><div>Pudła<b id="m0">0</b></div><div>Średnio<b id="a0">–</b></div><div>Najlepszy<b id="b0">–</b></div></div>
      <div class="lamps" id="l0"></div><div class="last" id="x0"></div>
    </div>
    <div class="card player p1" id="pc1">
      <h3>Gracz 2</h3><div class="score" id="s1">0</div>
      <div class="stats"><div>Trafienia<b id="h1">0</b></div><div>Pudła<b id="m1">0</b></div><div>Średnio<b id="a1">–</b></div><div>Najlepszy<b id="b1">–</b></div></div>
      <div class="lamps" id="l1"></div><div class="last" id="x1"></div>
    </div>
  </div>
  <p class="note">Punkty = trafienia − pudła. Pudło to wciśnięcie zgaszonego przycisku.</p>
</section>

<section id="diag">
  <div class="card">
    <div class="status"><strong id="diagState">Diagnostyka wyłączona</strong>
      <button class="btn" id="diagToggle">Włącz diagnostykę</button></div>
    <p class="note">Wybierz przycisk, steruj jego światłem i sprawdzaj licznik wciśnięć. Zielona ramka = przycisk wciśnięty teraz.
      W każdym polu: biały numer = pin przycisku (druga nóżka do GND), żółty = pin światła.</p>
  </div>
  <div class="card">
    <h3 style="margin:0;color:var(--p1)">Gracz 1</h3><div class="diag-grid" id="d0"></div>
    <h3 style="margin:0;color:var(--p2)">Gracz 2</h3><div class="diag-grid" id="d1"></div>
    <div class="actions">
      <button class="btn" id="allOn">Wszystkie światła ON</button>
      <button class="btn" id="allOff">Wszystkie OFF</button>
      <button class="btn" id="resetCnt">Zeruj liczniki</button>
    </div>
  </div>
  <div class="card detail" id="detail">
    <div><div class="note" id="selName">Gracz 1 · przycisk 1</div><div class="big" id="selCount">0</div><div class="note">wciśnięć</div></div>
    <div><div class="note">Podłączenie</div><div class="pins-line" id="selPins">–</div></div>
    <div><div class="note">Stan</div><span class="badge" id="selDown">puszczony</span></div>
    <div><div class="note">Światło</div><span class="badge" id="selLit">wyłączone</span></div>
    <div class="actions" style="margin:0">
      <button class="btn" id="lOn">Włącz światło</button>
      <button class="btn" id="lOff">Wyłącz</button>
      <button class="btn" id="lBlink">Mrugaj</button>
    </div>
  </div>
</section>
</main>
<div id="overlay"></div>

<script>
(() => {
  const $ = (id) => document.getElementById(id);
  const PHASES = {idle:'Gotowe', countdown:'Odliczanie…', play:'Gra trwa', over:'Koniec gry', diag:'Diagnostyka'};
  const cfg = {multi:'0', n:'1', same:'1', dur:'60'};
  try { Object.assign(cfg, JSON.parse(localStorage.getItem('reflexCfg') || '{}')); } catch (e) {}
  let st = null, stAt = 0, sel = {p:0, i:0}, blink = null, pins = null;

  // ---------- WebSocket with automatic reconnect ----------
  const wsUrl = (() => {
    const host = location.hostname || 'reflex.local';
    const port = location.port && location.port !== '80' ? (+location.port + 1) : 81;
    return 'ws://' + host + ':' + port + '/';
  })();
  let ws = null, tries = 0, lastMsg = 0, timer = null;

  function connect() {
    clearTimeout(timer);
    if (ws && ws.readyState <= 1) return;
    setConn(false, 'Łączenie…');
    try { ws = new WebSocket(wsUrl); } catch (e) { return retry(); }
    ws.onopen = () => { tries = 0; lastMsg = Date.now(); setConn(true, 'Połączono'); send('STATE?'); };
    ws.onmessage = (e) => {
      lastMsg = Date.now();
      if (e.data === 'pong') return;
      let m; try { m = JSON.parse(e.data); } catch (err) { return; }
      onMsg(m);
    };
    ws.onclose = () => { setConn(false, 'Rozłączono – ponawiam…'); retry(); };
    ws.onerror = () => { try { ws.close(); } catch (e) {} };
  }
  function retry() { clearTimeout(timer); timer = setTimeout(connect, Math.min(5000, 400 * Math.pow(2, tries++))); }
  function send(s) { if (ws && ws.readyState === 1) { ws.send(s); return true; } return false; }
  setInterval(() => {
    if (ws && ws.readyState === 1) {
      ws.send('ping');
      if (Date.now() - lastMsg > 7000) ws.close();   // silent connection: force reconnect
    }
  }, 2500);
  document.addEventListener('visibilitychange', () => { if (!document.hidden) connect(); });
  window.addEventListener('online', connect);
  function setConn(on, text) {
    $('conn').classList.toggle('on', on);
    $('connText').textContent = text + (on && st && st.ip ? ' · ' + st.ip : '');
  }

  // ---------- Tabs ----------
  document.querySelectorAll('nav button').forEach((b) => b.onclick = () => {
    document.querySelectorAll('nav button').forEach((x) => x.classList.toggle('active', x === b));
    document.querySelectorAll('section').forEach((s) => s.classList.toggle('active', s.id === b.dataset.tab));
  });

  // ---------- Settings ----------
  document.querySelectorAll('.seg').forEach((seg) => {
    seg.querySelectorAll('button').forEach((b) => b.onclick = () => {
      cfg[seg.dataset.key] = b.dataset.v;
      try { localStorage.setItem('reflexCfg', JSON.stringify(cfg)); } catch (e) {}
      renderSettings();
    });
  });
  function renderSettings() {
    const busy = st && (st.ph === 'play' || st.ph === 'countdown');
    document.querySelectorAll('.seg').forEach((seg) => {
      seg.querySelectorAll('button').forEach((b) => {
        b.classList.toggle('sel', cfg[seg.dataset.key] === b.dataset.v);
        b.disabled = busy;
      });
    });
    const shared = st && st.shared;
    $('patRow').style.display = cfg.multi === '1' ? '' : 'none';
    $('patRow').querySelectorAll('button').forEach((b) => { if (shared) b.disabled = true; });
    $('sharedNote').hidden = !(shared && cfg.multi === '1');
    $('pc1').style.display = (st && (st.ph === 'play' || st.ph === 'countdown' || st.ph === 'over') ? st.multi : cfg.multi === '1') ? '' : 'none';
    $('startBtn').disabled = busy;
    $('stopBtn').disabled = !st || st.ph === 'idle';
  }
  $('startBtn').onclick = () => {
    send('START ' + (cfg.multi === '1' ? 'multi' : 'single') + ' ' + cfg.n + ' ' + (cfg.same === '1' ? 'same' : 'ind') + ' ' + cfg.dur);
  };
  $('stopBtn').onclick = () => send('STOP');

  // ---------- Game view ----------
  for (let p = 0; p < 2; p++) {
    $('l' + p).innerHTML = Array.from({length: 8}, (_, i) => '<div class="lamp">' + (i + 1) + '</div>').join('');
    $('d' + p).innerHTML = Array.from({length: 8}, (_, i) =>
      '<div class="cell" data-p="' + p + '" data-i="' + i + '"><div class="n">#' + (i + 1) + '</div><div class="pin">–</div>' +
      '<div class="c">0</div><div class="l"></div><div class="pin led">–</div></div>').join('');
  }
  const fmt = (ms) => { const s = Math.ceil(ms / 1000); return Math.floor(s / 60) + ':' + String(s % 60).padStart(2, '0'); };

  function renderGame() {
    if (!st) return;
    $('phase').textContent = PHASES[st.ph] || st.ph;
    for (let p = 0; p < 2; p++) {
      const P = st.p[p];
      $('s' + p).textContent = P.s;
      $('h' + p).textContent = P.h;
      $('m' + p).textContent = P.m;
      $('a' + p).textContent = P.avg ? P.avg + ' ms' : '–';
      $('b' + p).textContent = P.best ? P.best + ' ms' : '–';
      const lamps = $('l' + p).children;
      for (let i = 0; i < 8; i++) lamps[i].classList.toggle('on', st.ph !== 'diag' && !!(P.lit & (1 << i)));
      $('pc' + p).classList.toggle('winner', st.ph === 'over' && st.win === p);
    }
    renderSettings();
  }

  function tick() {
    if (st) {
      const left = Math.max(0, st.left - (performance.now() - stAt));
      if (st.ph === 'play') $('timer').textContent = fmt(left);
      else if (st.ph === 'countdown') $('timer').textContent = fmt(st.dur * 1000);
      else if (st.ph !== 'over') $('timer').textContent = fmt((+cfg.dur) * 1000);
      const ov = $('overlay');
      if (st.ph === 'countdown') { ov.textContent = Math.max(1, Math.ceil(left / 1000)); ov.classList.add('show'); }
      else ov.classList.remove('show');
    }
    requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);

  function flash(p, b, kind, text) {
    const card = $('pc' + p), lamp = $('l' + p).children[b];
    card.classList.add('flash-' + kind); lamp.classList.add(kind);
    $('x' + p).textContent = text;
    setTimeout(() => { card.classList.remove('flash-' + kind); lamp.classList.remove(kind); }, 250);
  }

  // ---------- Diagnostics ----------
  document.querySelectorAll('.cell').forEach((c) => c.onclick = () => { sel = {p: +c.dataset.p, i: +c.dataset.i}; stopBlink(); renderDiag(); });
  $('diagToggle').onclick = () => send(st && st.ph === 'diag' ? 'STOP' : 'DIAG');
  $('allOn').onclick = () => { stopBlink(); send('ALL 1'); };
  $('allOff').onclick = () => { stopBlink(); send('ALL 0'); };
  $('resetCnt').onclick = () => send('RESET');
  $('lOn').onclick = () => { stopBlink(); send('LIGHT ' + sel.p + ' ' + sel.i + ' 1'); };
  $('lOff').onclick = () => { stopBlink(); send('LIGHT ' + sel.p + ' ' + sel.i + ' 0'); };
  $('lBlink').onclick = () => {
    if (blink) return stopBlink();
    const target = sel;
    blink = setInterval(() => send('LIGHT ' + target.p + ' ' + target.i + ' t'), 300);
    $('lBlink').textContent = 'Przestań mrugać';
  };
  function stopBlink() { if (blink) { clearInterval(blink); blink = null; } $('lBlink').textContent = 'Mrugaj'; }

  function renderPins() {
    if (!pins) return;
    for (let p = 0; p < 2; p++) {
      [...$('d' + p).children].forEach((c, i) => {
        const [btn, led] = c.querySelectorAll('.pin');
        btn.textContent = 'D' + pins.btn[p][i];
        led.textContent = 'D' + pins.led[p][i];
        c.title = 'Przycisk: D' + pins.btn[p][i] + ' + GND, światło: D' + pins.led[p][i];
      });
      [...$('l' + p).children].forEach((l, i) => {
        l.title = 'Przycisk D' + pins.btn[p][i] + ', światło D' + pins.led[p][i];
      });
    }
    renderSelPins();
  }
  function renderSelPins() {
    if (!pins) return;
    $('selPins').innerHTML = 'przycisk <b>D' + pins.btn[sel.p][sel.i] + '</b> + GND<br>światło <b>D' +
      pins.led[sel.p][sel.i] + '</b> → rezystor → LED → GND';
  }

  function renderDiag() {
    renderSelPins();
    if (!st) return;
    const on = st.ph === 'diag';
    $('diagState').textContent = on ? 'Diagnostyka włączona' : (st.ph === 'play' || st.ph === 'countdown' ? 'Trwa gra – zatrzymaj ją, aby testować' : 'Diagnostyka wyłączona');
    $('diagToggle').textContent = on ? 'Wyłącz diagnostykę' : 'Włącz diagnostykę';
    for (let p = 0; p < 2; p++) {
      const P = st.p[p];
      [...$('d' + p).children].forEach((c, i) => {
        c.querySelector('.c').textContent = P.cnt[i];
        c.classList.toggle('lit', !!(P.lit & (1 << i)));
        c.classList.toggle('down', !!(P.btn & (1 << i)));
        c.classList.toggle('sel', sel.p === p && sel.i === i);
      });
    }
    const P = st.p[sel.p], m = 1 << sel.i;
    $('selName').textContent = 'Gracz ' + (sel.p + 1) + ' · przycisk ' + (sel.i + 1);
    $('selCount').textContent = P.cnt[sel.i];
    $('selDown').textContent = P.btn & m ? 'WCIŚNIĘTY' : 'puszczony';
    $('selDown').classList.toggle('on', !!(P.btn & m));
    $('selLit').textContent = P.lit & m ? 'włączone' : 'wyłączone';
    $('selLit').classList.toggle('on', !!(P.lit & m));
    if (!on) stopBlink();
  }

  // ---------- Messages ----------
  function onMsg(m) {
    if (m.t === 'pins') {
      pins = m; renderPins();
    } else if (m.t === 'state') {
      st = m; stAt = performance.now();
      renderGame(); renderDiag();
      setConn(true, 'Połączono');
    } else if (m.t === 'hit') {
      flash(m.p, m.b, 'hit', 'Trafienie #' + (m.b + 1) + ' w ' + m.ms + ' ms');
    } else if (m.t === 'miss') {
      flash(m.p, m.b, 'miss', 'Pudło #' + (m.b + 1));
    } else if (m.t === 'over') {
      $('x0').textContent = ''; $('x1').textContent = '';
    }
  }

  renderSettings();
  connect();
})();
</script>
</body>
</html>
)rawliteral";
