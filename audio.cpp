#include "audio.h"

// RP2350 port: l'audio ES8311/I2S viene tenuto isolato dal core del gioco.
// Il backend ritorna OFF finché non viene aggiunta la pipeline I2S; questo
// evita di bloccare il firmware su FreeRTOS/ESP_I2S non disponibili sul Pico 2.
static bool gOn = false;
static bool gSleeping = false;

void audioBegin() { gSleeping = false; }
void sfxPlay(uint8_t) {}
void audioSetEnabled(bool on) { gOn = on; }
bool audioEnabled() { return gOn; }
void audioSetSleeping(bool sleeping) { gSleeping = sleeping; (void)gSleeping; }
