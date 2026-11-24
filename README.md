# Reflex Arcade

Gra refleksowa dla 1 lub 2 graczy na płytce **Arduino Mega 2560 + ESP8266 (2w1)**, sterowana ze strony WWW w lokalnej sieci Wi-Fi.

Zapalają się przyciski, a gracz ma jak najszybciej wcisnąć podświetlony. Po każdym trafieniu zapala się inny przycisk. Wygrywa ten, kto zdobędzie więcej punktów, a punkty liczą się jako trafienia minus pudła.

## Ustawienia gry (na stronie)

| Opcja | Wartości |
|---|---|
| Tryb | 1 gracz / 2 graczy |
| Świateł naraz | 1, 2 lub 3 |
| Cele (tryb 2 graczy) | **Te same dla obu**: wyścig o ten sam przycisk, punkt dostaje szybszy gracz. **Losowe dla każdego**: każdy gracz ma własne cele. |
| Czas | 30 s, 45 s, 1 min, 2 min, 3 min, 5 min |

Strona pokazuje na żywo czas, punkty, trafienia, pudła, średni i najlepszy czas reakcji oraz zapalone światła.

**Diagnostyka:** wybierasz przycisk, włączasz, wyłączasz albo migasz jego światłem, widzisz na żywo, czy jest wciśnięty, i sprawdzasz licznik wciśnięć.

## Jak to działa

```
telefon / komputer  ──Wi-Fi, WebSocket──▶  ESP8266  ──Serial3──▶  ATmega2560  ──▶  16 przycisków + 16 świateł
```

- **Mega** (`reflex-arcade-mega/`) prowadzi grę: odczytuje przyciski, steruje światłami i mierzy czas reakcji. Działa niezależnie od Wi-Fi, więc chwilowy zanik sieci nie psuje gry ani pomiarów.
- **ESP8266** (`reflex-arcade-esp/`) łączy się z Wi-Fi, wyświetla stronę i przekazuje komunikaty między stroną a Megą. Ma wyłączone oszczędzanie energii Wi-Fi, sam łączy się ponownie z siecią i odcina martwe połączenia (heartbeat).
- **Strona** sama łączy się ponownie po utracie połączenia i co 2,5 s sprawdza połączenie (`ping`/`pong`). Po ponownym połączeniu od razu dostaje aktualny stan gry.

## Podłączenie

Każdy przycisk: jedna nóżka do pinu, druga do **GND** (rezystory nie są potrzebne, używane są wewnętrzne pull-upy).
Każde światło: pin → rezystor **220 Ω** → LED → **GND**.

| | Przyciski | Światła |
|---|---|---|
| Gracz 1 (1–8) | D22 – D29 | D38 – D45 |
| Gracz 2 (1–8) | D30 – D37 | D46 – D53 |

- Jeden pin daje maks. około **20 mA**, więc wystarcza na zwykłą diodę LED. Przyciski arcade z podświetleniem 12 V albo taśmy LED trzeba podłączyć przez sterownik, np. ULN2803 albo moduł tranzystorów lub przekaźników. Dla modułów włączanych stanem niskim ustaw `LED_ACTIVE_HIGH 0`.
- **Tylko 8 świateł w sumie?** Ustaw `SHARED_LIGHTS 1` w `reflex-arcade-mega.ino`. Używane są wtedy piny D38–D45, światło N oznacza przycisk N u obu graczy, a w trybie 2 graczy cele są zawsze te same.
- Pinów 0, 1, 14 i 15 nie używaj: służą do USB i do komunikacji z ESP8266. Pin 13 ma na płytce diodę „L”, więc nie nadaje się na wejście.

## Przełączniki DIP na płytce

Przełączniki DIP decydują, kto z kim jest połączony. **Przestawiaj je przy odłączonym USB.**

| Do czego | Przełączniki na ON | Pozostałe |
|---|---|---|
| Wgrywanie programu na Megę | 3, 4 | OFF |
| Wgrywanie programu na ESP8266 | 5, 6, 7 | OFF |
| **Normalna gra** (Mega ↔ ESP + podgląd przez USB) | 1, 2, 3, 4 | OFF |
| Normalna gra bez komputera | 1, 2 | OFF |

Do gry suwak **RXD3/TXD3 ↔ RXD0/TXD0** musi być ustawiony na **RXD3/TXD3**.

## Wgrywanie

Wymagane: [arduino-cli](https://arduino.github.io/arduino-cli/), rdzenie `arduino:avr` i `esp8266:esp8266` oraz biblioteka `WebSockets` (Markus Sattler).

```bash
arduino-cli config add board_manager.additional_urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install arduino:avr esp8266:esp8266
arduino-cli lib install WebSockets
```

**1. Mega** (DIP 3, 4 ON):

```bash
arduino-cli compile --fqbn arduino:avr:mega reflex-arcade-mega
arduino-cli upload -p /dev/cu.usbserial-2120 --fqbn arduino:avr:mega reflex-arcade-mega
```

**2. ESP8266** (DIP 5, 6, 7 ON). Najpierw skopiuj `reflex-arcade-esp/secrets.example.h` do `reflex-arcade-esp/secrets.h` i wpisz w nim nazwę i hasło sieci (tylko 2,4 GHz). Plik `secrets.h` jest w `.gitignore`.

```bash
arduino-cli compile --fqbn esp8266:esp8266:generic reflex-arcade-esp
arduino-cli upload -p /dev/cu.usbserial-2120 --fqbn esp8266:esp8266:generic:ResetMethod=nodemcu reflex-arcade-esp
```

**3. Gra** (DIP 1, 2, 3, 4 ON, suwak na RXD3/TXD3): otwórz `http://reflex.local`. Jeśli ta nazwa nie działa, użyj adresu IP płytki. Pokazuje go Mega w monitorze portu (pole `"ip"`):

```bash
arduino-cli monitor -p /dev/cu.usbserial-2120 -c baudrate=115200
```

## Gra przez USB, bez Wi-Fi

`tools/dev_bridge.py` robi to samo co ESP8266, tylko na komputerze. Przydaje się do testów okablowania. Wystarczy DIP 3 i 4 na ON.

```bash
pip install pyserial websockets
python3 tools/dev_bridge.py --port /dev/cu.usbserial-2120
```

Potem otwórz <http://localhost:8090>. Z opcją `--lan` stronę otworzysz też z telefonu w tej samej sieci.

## Protokół (Mega ↔ ESP8266/USB, 115200, linia tekstu)

Polecenia: `START single|multi <1-4> same|ind <sekundy>`, `STOP`, `STATE?`, `DIAG`, `LIGHT <gracz 0-1> <przycisk 0-7> <0|1|t>`, `ALL <0|1>`, `RESET`.
Odpowiedzi (JSON): `{"t":"state",...}` przy każdej zmianie i co 500 ms, `{"t":"hit",...}`, `{"t":"miss",...}`, `{"t":"over","win":0|1|-1}`.
