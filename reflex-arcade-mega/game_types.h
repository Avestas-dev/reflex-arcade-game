#pragma once
#include <Arduino.h>

// Kept in a header so the Arduino builder's auto-generated function
// prototypes can see these types.

const uint8_t PLAYERS = 2;
const uint8_t KEYS = 8;

enum Phase : uint8_t { IDLE, COUNTDOWN, PLAY, OVER, DIAG };

struct Key {
  bool raw;
  bool down;
  uint32_t changedAt;
};

struct Player {
  uint8_t lit;               // targets (independent mode)
  uint32_t litAt[KEYS];
  uint16_t hits;
  uint16_t misses;
  uint32_t reactSum;
  uint16_t best;
};

struct Config {
  bool multi;
  uint8_t targets;
  bool same;
  uint16_t seconds;
};
