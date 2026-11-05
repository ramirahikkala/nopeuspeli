# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Projekti

Speden Spelit -henkinen fyysinen nopeuspeli. Laitteena TTGO T-Display S3 (ESP32-S3, 1.9" ST7789 LCD). Kehitysympäristö: PlatformIO + VS Code.

## PlatformIO-komennot

```bash
pio run                  # käännä
pio run -t upload        # käännä ja lataa laitteelle
pio device monitor       # sarjamonitori (debug)
pio run -t upload && pio device monitor  # lataa ja seuraa
pio lib install          # asenna kirjastot platformio.ini:n mukaan
```

## Arkkitehtuuri

```
nopeuspeli/
├── platformio.ini       # levy, kirjastot, upload-asetukset
├── src/
│   └── main.cpp         # pääohjelma
└── include/             # omat headerit
```

**platformio.ini** määrittelee:
- `board = esp32-s3-devkitc-1`
- `framework = arduino`
- Kirjastot: LovyanGFX, ESPAsyncWebServer, AsyncTCP

## Hardware-pinnit

| # | LED GPIO | Nappi GPIO |
|---|----------|------------|
| 1 | GPIO 11  | GPIO 1     |
| 2 | GPIO 12  | GPIO 2     |
| 3 | GPIO 13  | GPIO 3     |
| 4 | GPIO 21  | GPIO 10    |

Napit: active LOW, sisäinen pull-up. LEDit: active HIGH, 220Ω sarjaan.

Näyttö käyttää 8-bittistä rinnakkaisväylää (ei SPI):
- Data: D0-D7 = GPIO 39-48
- WR=8, RD=9, DC=7, CS=6, RST=5, BL=38
- PIN_POWER_ON=15 (asetettava HIGH ennen lcd.init())

## Pelilogiikka

- 4 nappia, yksi LED syttyy kerrallaan satunnaisesti
- Pelaaja painaa oikeaa nappia → jatkuu
- Väärä nappi tai aikakatkaisu → peli loppuu
- Kierrosnopeus alkaa 1 s:sta ja nopeutuu
- Pisteet ja pelitila näytetään LCD:llä sekä web UI:ssa (WebSocket)
