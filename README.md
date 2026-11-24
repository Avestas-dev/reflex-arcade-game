# reflex-arcade-game

Arduino Mega 2560 project template.

The sketch in `reflex-arcade-game/` is a pull-up test: D13 reads HIGH when open and LOW when connected to GND. Changes are printed to the Serial Monitor at 115200 baud.

## Build and upload

```bash
arduino-cli compile --fqbn arduino:avr:mega reflex-arcade-game
arduino-cli upload -p <port> --fqbn arduino:avr:mega reflex-arcade-game
arduino-cli monitor -p <port> -c baudrate=115200
```
