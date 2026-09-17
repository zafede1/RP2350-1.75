#pragma once
#include <stdint.h>

// Efectos de sonido del juego. En RP2350 se reproducen de forma sincronica
// durante unas pocas decenas de ms y despues liberan los pines compartidos con SD.
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
