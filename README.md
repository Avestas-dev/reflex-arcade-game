# Reflex Arcade

A fast browser arcade game that tests your reaction time. Targets pop up around the screen and you hit them before their countdown ring closes. It gets faster the longer you last.

## How to play

| Target | What to do |
| --- | --- |
| **Cyan** | Hit it. The faster you hit it, the more points you get. If it runs out, you lose a life. |
| **Gold** | A bonus target: small, quick and worth 5×. Letting it expire only breaks your combo. |
| **Red ✕** | A hazard. Leave it alone: hitting it costs a life. |

- You start with **3 lives**.
- Every 5 hits in a row raise your score **multiplier** by one. Missing a target or clicking empty space resets it.
- The **level** goes up every 15 seconds: targets spawn faster, live for less time, and hazards show up more often.
- When the game ends you see your best combo, average reaction time and accuracy. Your best score is saved in the browser.

**Controls:** mouse or touch to hit targets. `P` or `Esc` pauses, and `Enter` or `Space` starts a new game.

## Running it

No build step and no dependencies. Open `index.html` in a browser, or serve the folder:

```bash
python3 -m http.server 8000
```

then go to <http://localhost:8000>.

## Project layout

```
index.html   page structure, HUD and menus
style.css    styling
game.js      game loop, spawning, input, scoring and rendering (canvas)
```
