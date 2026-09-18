#!/usr/bin/env python3
"""Carica gli sprite sulla microSD della scheda TamaPoke via USB seriale.

Esempi:
  python tools/send_sd.py --port COM6
  python tools/send_sd.py --port COM6 --ls

Richiede pyserial:
  python -m pip install pyserial
"""
import argparse
import glob
import os
import sys
import time

import serial
from serial.tools import list_ports


def find_port():
    ports = list(list_ports.comports())
    if not ports:
        sys.exit("Nessuna porta seriale trovata. Collega la scheda e riprova.")
    if len(ports) == 1:
        return ports[0].device

    print("Porte seriali disponibili:")
    for p in ports:
        print(f"  {p.device}: {p.description}")
    sys.exit("Specifica la porta, per esempio: python tools/send_sd.py --port COM6")


def wait_for(ser, expected, timeout=10, show=True):
    """Attende una riga esatta; restituisce (ok, errore_o_none)."""
    end = time.time() + timeout
    while time.time() < end:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode(errors="replace").strip()
        if not line:
            continue
        if show:
            print(f"  scheda: {line}")
        if line == expected:
            return True, None
        if line.startswith("ERR"):
            return False, line
    return False, "timeout"


def check_sd(ser):
    ser.reset_input_buffer()
    ser.write(b"SDINFO\n")
    ser.flush()
    ok, err = wait_for(ser, "DONE", 8)
    if not ok:
        print()
        print("ERRORE: la microSD non e' disponibile sulla scheda.")
        print(f"Dettaglio firmware: {err}")
        print("Usa il nuovo firmware RP2350 con il fix SD SPI0 (GPIO 18/19/20, CS 21).")
        return False
    return True


def send_file(ser, path):
    size = os.path.getsize(path)
    name = f"mons/{os.path.basename(path)}"
    print(f"-> {name} ({size / 1024:.0f} KB)")

    ser.write(f"PUT {name} {size}\n".encode())
    ser.flush()
    ok, err = wait_for(ser, "OK", 8)
    if not ok:
        print(f"   ERRORE: PUT rifiutato ({err})")
        return False, err

    t0 = time.time()
    sent = 0
    with open(path, "rb") as src:
        while True:
            chunk = src.read(2048)
            if not chunk:
                break
            ser.write(chunk)
            ser.flush()
            ok, err = wait_for(ser, "#", 10, show=False)
            if not ok:
                print(f"   ERRORE durante il trasferimento a {sent / 1024:.0f} KB: {err}")
                return False, err
            sent += len(chunk)

    ok, err = wait_for(ser, "DONE", 30)
    if not ok:
        print(f"   ERRORE finale: {err}")
        return False, err

    speed = size / 1024 / max(0.01, time.time() - t0)
    print(f"   OK ({speed:.0f} KB/s)")
    return True, None


def main():
    parser = argparse.ArgumentParser(description="Carica gli sprite TamaPoke sulla microSD via USB.")
    parser.add_argument("--port", help="Porta seriale, es. COM6")
    parser.add_argument("--ls", action="store_true", help="Elenca i file presenti in /mons")
    args = parser.parse_args()

    port = args.port or find_port()
    print(f"Porta: {port}")

    try:
        ser = serial.Serial(port, 115200, timeout=1, write_timeout=10)
    except serial.SerialException as exc:
        sys.exit(f"Impossibile aprire {port}: {exc}")

    try:
        time.sleep(1.5)
        if not check_sd(ser):
            sys.exit(2)

        if args.ls:
            print("Contenuto /mons:")
            ser.write(b"LS\n")
            ser.flush()
            ok, err = wait_for(ser, "DONE", 10)
            if not ok:
                sys.exit(f"Errore durante LS: {err}")
            return

        pattern = os.path.join(os.path.dirname(__file__), "sdcard", "mons", "*.bin")
        files = sorted(glob.glob(pattern))
        if not files:
            sys.exit("Nessun .bin trovato in tools/sdcard/mons.")

        print(f"Trovati {len(files)} file. Inizio trasferimento.")
        completed = 0
        for path in files:
            ok, err = send_file(ser, path)
            if not ok:
                # Un errore di mount/open/write non migliora passando al file successivo.
                sys.exit(f"Trasferimento interrotto: {err}")
            completed += 1

        print(f"Completato: {completed}/{len(files)} file caricati.")
        print("Riavvia la scheda per ricaricare sprite e Pokédex.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
