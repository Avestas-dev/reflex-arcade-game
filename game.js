(() => {
  'use strict';

  const canvas = document.getElementById('game');
  const ctx = canvas.getContext('2d');
  const stage = document.getElementById('stage');

  const el = (id) => document.getElementById(id);
  const ui = {
    score: el('score'), combo: el('combo'), level: el('level'), lives: el('lives'), best: el('best'),
    start: el('start'), pause: el('pause'), over: el('over'),
    finalScore: el('finalScore'), finalCombo: el('finalCombo'),
    finalReaction: el('finalReaction'), finalAccuracy: el('finalAccuracy'), newBest: el('newBest'),
  };

  const COLORS = { normal: '#3ef0ff', bonus: '#ffd23e', hazard: '#ff4d6d' };
  const MAX_LIVES = 3;
  const BEST_KEY = 'reflex-arcade-best';

  // ---------- Storage (best score) ----------
  function loadBest() {
    try { return Number(localStorage.getItem(BEST_KEY)) || 0; } catch { return 0; }
  }
  function saveBest(v) {
    try { localStorage.setItem(BEST_KEY, String(v)); } catch { /* ignore */ }
  }

  // ---------- Canvas sizing ----------
  let W = 0, H = 0, DPR = 1;
  function resize() {
    DPR = Math.min(window.devicePixelRatio || 1, 2);
    W = stage.clientWidth;
    H = stage.clientHeight;
    canvas.width = Math.round(W * DPR);
    canvas.height = Math.round(H * DPR);
    ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
  }
  window.addEventListener('resize', resize);
  resize();

  // ---------- Game state ----------
  const state = {
    mode: 'menu', // menu | play | paused | over
    score: 0, combo: 0, bestCombo: 0, lives: MAX_LIVES, level: 1,
    best: loadBest(),
    elapsed: 0, spawnTimer: 0,
    targets: [], particles: [], floaters: [],
    shake: 0, flash: 0,
    hits: 0, clicks: 0, reactionTotal: 0,
  };

  function difficulty() {
    const t = state.elapsed;
    state.level = 1 + Math.floor(t / 15);
    const lvl = state.level - 1;
    return {
      spawnEvery: Math.max(0.35, 1.1 - lvl * 0.09),
      lifetime: Math.max(0.75, 2.0 - lvl * 0.13),
      maxOnScreen: Math.min(7, 2 + Math.floor(lvl / 1.5)),
      hazardChance: Math.min(0.3, 0.08 + lvl * 0.03),
      bonusChance: 0.08,
    };
  }

  // ---------- Entities ----------
  function spawnTarget() {
    const d = difficulty();
    if (state.targets.length >= d.maxOnScreen) return;

    const roll = Math.random();
    let kind = 'normal';
    if (roll < d.hazardChance) kind = 'hazard';
    else if (roll < d.hazardChance + d.bonusChance) kind = 'bonus';

    const base = Math.max(22, Math.min(W, H) * 0.055);
    const r = kind === 'bonus' ? base * 0.65 : base;
    const life = kind === 'bonus' ? d.lifetime * 0.6 : kind === 'hazard' ? d.lifetime * 1.3 : d.lifetime;

    // Find a spot that doesn't overlap existing targets.
    let x, y, ok = false;
    for (let i = 0; i < 20 && !ok; i++) {
      x = r + 8 + Math.random() * (W - 2 * (r + 8));
      y = r + 8 + Math.random() * (H - 2 * (r + 8));
      ok = state.targets.every((t) => Math.hypot(t.x - x, t.y - y) > t.r + r + 12);
    }
    if (!ok) return;

    state.targets.push({ x, y, r, kind, life, age: 0, pop: 0 });
  }

  function burst(x, y, color, n = 16) {
    for (let i = 0; i < n; i++) {
      const a = Math.random() * Math.PI * 2;
      const s = 80 + Math.random() * 260;
      state.particles.push({
        x, y, vx: Math.cos(a) * s, vy: Math.sin(a) * s,
        life: 0.4 + Math.random() * 0.4, age: 0, color, size: 2 + Math.random() * 3,
      });
    }
  }

  function floatText(x, y, text, color) {
    state.floaters.push({ x, y, text, color, age: 0, life: 0.8 });
  }

  function loseLife(x, y, reason) {
    state.lives -= 1;
    state.combo = 0;
    state.shake = 0.35;
    state.flash = 0.25;
    floatText(x, y, reason, COLORS.hazard);
    if (state.lives <= 0) endGame();
  }

  // ---------- Input ----------
  function pointerPos(e) {
    const rect = canvas.getBoundingClientRect();
    return { x: e.clientX - rect.left, y: e.clientY - rect.top };
  }

  canvas.addEventListener('pointerdown', (e) => {
    if (state.mode !== 'play') return;
    e.preventDefault();
    const { x, y } = pointerPos(e);
    state.clicks += 1;

    // Topmost (most recently spawned) target wins.
    for (let i = state.targets.length - 1; i >= 0; i--) {
      const t = state.targets[i];
      if (Math.hypot(t.x - x, t.y - y) > t.r * 1.1) continue;
      state.targets.splice(i, 1);

      if (t.kind === 'hazard') {
        burst(t.x, t.y, COLORS.hazard, 24);
        loseLife(t.x, t.y, 'OUCH');
        return;
      }

      state.hits += 1;
      state.reactionTotal += t.age;
      state.combo += 1;
      state.bestCombo = Math.max(state.bestCombo, state.combo);

      const speed = 1 - t.age / t.life; // 1 = instant, 0 = last moment
      const base = t.kind === 'bonus' ? 50 : 10;
      const mult = 1 + Math.floor(state.combo / 5);
      const pts = Math.round(base * (1 + speed) * mult);
      state.score += pts;

      burst(t.x, t.y, COLORS[t.kind], t.kind === 'bonus' ? 28 : 16);
      floatText(t.x, t.y, `+${pts}`, COLORS[t.kind]);
      return;
    }

    // Clicked empty space.
    if (state.combo > 0) floatText(x, y, 'combo lost', '#8a90b5');
    state.combo = 0;
  });

  window.addEventListener('keydown', (e) => {
    if (e.key === 'p' || e.key === 'P' || e.key === 'Escape') {
      if (state.mode === 'play') setMode('paused');
      else if (state.mode === 'paused') setMode('play');
    } else if ((e.key === 'Enter' || e.key === ' ') && (state.mode === 'menu' || state.mode === 'over')) {
      e.preventDefault();
      startGame();
    }
  });

  document.addEventListener('visibilitychange', () => {
    if (document.hidden && state.mode === 'play') setMode('paused');
  });

  el('startBtn').addEventListener('click', startGame);
  el('againBtn').addEventListener('click', startGame);
  el('resumeBtn').addEventListener('click', () => setMode('play'));

  // ---------- Flow ----------
  function setMode(mode) {
    state.mode = mode;
    ui.start.classList.toggle('visible', mode === 'menu');
    ui.pause.classList.toggle('visible', mode === 'paused');
    ui.over.classList.toggle('visible', mode === 'over');
    lastTime = performance.now();
  }

  function startGame() {
    Object.assign(state, {
      score: 0, combo: 0, bestCombo: 0, lives: MAX_LIVES, level: 1,
      elapsed: 0, spawnTimer: 0.4,
      targets: [], particles: [], floaters: [],
      shake: 0, flash: 0, hits: 0, clicks: 0, reactionTotal: 0,
    });
    setMode('play');
  }

  function endGame() {
    const isBest = state.score > state.best;
    if (isBest) { state.best = state.score; saveBest(state.best); }
    ui.finalScore.textContent = state.score.toLocaleString();
    ui.finalCombo.textContent = state.bestCombo;
    ui.finalReaction.textContent = state.hits ? `${Math.round((state.reactionTotal / state.hits) * 1000)} ms` : '–';
    ui.finalAccuracy.textContent = state.clicks ? `${Math.round((state.hits / state.clicks) * 100)}%` : '–';
    ui.newBest.hidden = !isBest;
    setMode('over');
  }

  // ---------- Update ----------
  function update(dt) {
    state.elapsed += dt;
    const d = difficulty();

    state.spawnTimer -= dt;
    if (state.spawnTimer <= 0) {
      spawnTarget();
      state.spawnTimer = d.spawnEvery * (0.7 + Math.random() * 0.6);
    }

    for (let i = state.targets.length - 1; i >= 0; i--) {
      const t = state.targets[i];
      t.age += dt;
      t.pop = Math.min(1, t.pop + dt * 8);
      if (t.age >= t.life) {
        state.targets.splice(i, 1);
        if (t.kind === 'hazard') continue; // hazards expire harmlessly
        burst(t.x, t.y, '#555b80', 8);
        if (t.kind === 'normal') loseLife(t.x, t.y, 'MISS');
        else state.combo = 0; // a missed bonus only breaks the combo
        if (state.mode !== 'play') return;
      }
    }
  }

  function updateEffects(dt) {
    for (let i = state.particles.length - 1; i >= 0; i--) {
      const p = state.particles[i];
      p.age += dt;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.vx *= 0.92;
      p.vy *= 0.92;
      if (p.age >= p.life) state.particles.splice(i, 1);
    }
    for (let i = state.floaters.length - 1; i >= 0; i--) {
      const f = state.floaters[i];
      f.age += dt;
      f.y -= 40 * dt;
      if (f.age >= f.life) state.floaters.splice(i, 1);
    }
    state.shake = Math.max(0, state.shake - dt);
    state.flash = Math.max(0, state.flash - dt);
  }

  // ---------- Render ----------
  function drawGrid() {
    ctx.strokeStyle = 'rgba(80, 90, 160, 0.08)';
    ctx.lineWidth = 1;
    const step = 40;
    ctx.beginPath();
    for (let x = 0; x < W; x += step) { ctx.moveTo(x + 0.5, 0); ctx.lineTo(x + 0.5, H); }
    for (let y = 0; y < H; y += step) { ctx.moveTo(0, y + 0.5); ctx.lineTo(W, y + 0.5); }
    ctx.stroke();
  }

  function drawTarget(t) {
    const color = COLORS[t.kind];
    const remain = 1 - t.age / t.life;
    const scale = easeOutBack(t.pop);
    const r = t.r * scale;

    ctx.save();
    ctx.translate(t.x, t.y);

    // Countdown ring
    ctx.strokeStyle = color;
    ctx.globalAlpha = 0.35;
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.arc(0, 0, r + 8, 0, Math.PI * 2);
    ctx.stroke();
    ctx.globalAlpha = 1;
    ctx.beginPath();
    ctx.arc(0, 0, r + 8, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * remain);
    ctx.stroke();

    // Body
    ctx.shadowColor = color;
    ctx.shadowBlur = 18;
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.arc(0, 0, r, 0, Math.PI * 2);
    ctx.fill();
    ctx.shadowBlur = 0;

    ctx.fillStyle = '#0b0d17';
    if (t.kind === 'hazard') {
      ctx.strokeStyle = '#0b0d17';
      ctx.lineWidth = Math.max(3, r * 0.18);
      ctx.lineCap = 'round';
      const k = r * 0.4;
      ctx.beginPath();
      ctx.moveTo(-k, -k); ctx.lineTo(k, k);
      ctx.moveTo(k, -k); ctx.lineTo(-k, k);
      ctx.stroke();
    } else {
      ctx.beginPath();
      ctx.arc(0, 0, r * 0.62, 0, Math.PI * 2);
      ctx.fill();
      ctx.fillStyle = color;
      ctx.beginPath();
      ctx.arc(0, 0, r * 0.3, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();
  }

  function render() {
    ctx.save();
    ctx.fillStyle = '#0b0d17';
    ctx.fillRect(0, 0, W, H);

    if (state.shake > 0) {
      const m = state.shake * 24;
      ctx.translate((Math.random() - 0.5) * m, (Math.random() - 0.5) * m);
    }

    drawGrid();
    state.targets.forEach(drawTarget);

    for (const p of state.particles) {
      ctx.globalAlpha = 1 - p.age / p.life;
      ctx.fillStyle = p.color;
      ctx.fillRect(p.x - p.size / 2, p.y - p.size / 2, p.size, p.size);
    }

    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.font = '700 18px ui-sans-serif, system-ui, sans-serif';
    for (const f of state.floaters) {
      ctx.globalAlpha = 1 - f.age / f.life;
      ctx.fillStyle = f.color;
      ctx.fillText(f.text, f.x, f.y);
    }
    ctx.globalAlpha = 1;
    ctx.restore();

    if (state.flash > 0) {
      ctx.fillStyle = `rgba(255, 77, 109, ${state.flash * 0.8})`;
      ctx.fillRect(0, 0, W, H);
    }
  }

  function renderHud() {
    ui.score.textContent = state.score.toLocaleString();
    const mult = 1 + Math.floor(state.combo / 5);
    ui.combo.textContent = `${state.combo} (x${mult})`;
    ui.level.textContent = state.level;
    ui.lives.textContent = '♥'.repeat(Math.max(0, state.lives)) + '♡'.repeat(MAX_LIVES - Math.max(0, state.lives));
    ui.best.textContent = state.best.toLocaleString();
  }

  function easeOutBack(x) {
    const c1 = 1.70158, c3 = c1 + 1;
    return 1 + c3 * Math.pow(x - 1, 3) + c1 * Math.pow(x - 1, 2);
  }

  // ---------- Loop ----------
  let lastTime = performance.now();
  function frame(now) {
    const dt = Math.min(0.05, (now - lastTime) / 1000);
    lastTime = now;
    if (state.mode === 'play') update(dt);
    if (state.mode !== 'paused') updateEffects(dt);
    render();
    renderHud();
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  // Expose for debugging/tests.
  window.__reflex = state;
})();
