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

| # | LED GPIO | Nappi GPIO | ULN-kanava |
|---|----------|------------|------------|
| 1 | GPIO 1   | GPIO 43    | 1B/1C      |
| 2 | GPIO 2   | GPIO 18    | 3B/3C      |
| 3 | GPIO 3   | GPIO 17    | 5B/5C      |
| 4 | GPIO 10  | GPIO 21    | 7B/7C      |

LEDit ohjataan ULN2803:n kautta (low-side), LED+ yhteinen +5V, 220 Ω LEDin katodipuolella.
Protolevyn kytkentä, reikäpaikat ja tehty/tekemättä-tila: `docs/kytkenta.md` ja `docs/kytkenta.png`.
Huom: `src/main.cpp` ohjaa toistaiseksi vain painiketta 1 (`LED_PIN 1`, `BTN_PIN 43`).

Napit: active LOW, sisäinen pull-up. LEDit: active HIGH (GPIO HIGH → ULN johtaa → LED syttyy).

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
