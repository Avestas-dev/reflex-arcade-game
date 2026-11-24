// reflex-arcade-game: Arduino Mega 2560 template
//
// Pull-up test on D13:
//   D13 uses the internal pull-up resistor, so it reads HIGH when nothing is connected.
//   Connect D13 to GND (wire or button) and it reads LOW.
//   Every change is printed to the Serial Monitor at 115200 baud.

const uint8_t TEST_PIN = 13;

int lastState = -1;

void setup() {
  Serial.begin(115200);
  pinMode(TEST_PIN, INPUT_PULLUP);
  Serial.println(F("D13 pull-up test ready"));
}

void loop() {
  int state = digitalRead(TEST_PIN);
  if (state != lastState) {
    lastState = state;
    Serial.println(state == LOW ? F("D13: LOW  (connected to GND)")
                                : F("D13: HIGH (open)"));
  }
  delay(20);  // simple debounce
}
