# TamaPoke web installer — RP2350

This page is for the **Waveshare RP2350-Touch-AMOLED-1.75** port. It deliberately does not use the old ESP Web Tools ESP32 manifest.

## Firmware

The page sends the user to [pico⚡flash](https://picoflash.org/) for browser flashing. pico⚡flash implements Raspberry Pi's PICOBOOT protocol and supports RP2040 and RP2350 over WebUSB.

The repository CI workflow builds `TamaPoke-RP2350-1.75.uf2` as an Actions artifact. Download that UF2 from the workflow/release and flash it with pico⚡flash while the board is in BOOTSEL mode.

`web/manifest.json` and the old ESP32 `web/firmware/tamapoke.bin` are intentionally absent so the RP2350 page cannot accidentally offer an ESP32 image.

## Sprites

`index.html` uses Web Serial to send the same `PUT mons/<file> <bytes>` protocol as `tools/send_sd.py`. `sprites.pak` is the TPAK bundle produced by the sprite tooling. The page supports resume-after-interruption using browser local storage and also accepts individual `.bin` files.

Keep pico⚡flash and the serial uploader in separate steps: a Chromium browser should not have two pages competing for the same USB device.

## Local test

WebUSB/Web Serial require a secure context. For local testing:

```bash
cd web
python3 -m http.server 8000
```

Open `http://localhost:8000` in Chrome or Edge.

## GitHub Pages

Serve `/web` from GitHub Pages. HTTPS is provided by Pages automatically. The automatic sprite loader expects `sprites.pak` to be reachable at the same origin.

## License notes

The firmware port is based on the original TamaPoke project. The browser flasher is [pico⚡flash](https://github.com/piersfinlayson/picoflash), which is MIT licensed. Sprites come from PMD SpriteCollab (CC BY-NC); Pokémon is a trademark of Nintendo / Game Freak.

## Local use on Windows

Do not open index.html directly with file://. Chrome gives local files an opaque origin, which can block local resource loading and browser APIs.

From this directory run:

```bat
python -m http.server 8000
```

Then open:

```text
http://localhost:8000/
```

Or double-click run-local.bat.
