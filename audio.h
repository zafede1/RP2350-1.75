#pragma once
#include <stdint.h>

// Effetti sonori del gioco. Su RP2350 vengono riprodotti in modo sincrono.
// La microSD usa SPI0 sui GPIO 18/19/20/21 e non condivide i pin con I2S.
enum Sfx : uint8_t {
  SFX_TAP = 0,
  SFX_EAT,
  SFX_PLAY,
  SFX_HEART,
  SFX_HATCH,
  SFX_EVOLVE,
  SFX_MEDAL,
  SFX_DENY,
  SFX_BYE,
  SFX_LEVEL,
  SFX_COUNT
};

void audioBegin();
void sfxPlay(uint8_t id);
void audioSetEnabled(bool on);
bool audioEnabled();
void audioSetSleeping(bool sleeping);
