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
| 1 | GPIO 2   | GPIO 21    | 3B/3C      |
| 2 | GPIO 10  | GPIO 18    | 7B/7C      |
| 3 | GPIO 3   | GPIO 17    | 5B/5C      |
| 4 | GPIO 1   | GPIO 43    | 1B/1C      |

Numerointi on fyysinen (lamppu/nappi n kotelossa). Mitattu `iomap`-ohjelmalla 7.11.2026.
ULN-kanavat on päätelty kytkentäkuvasta (GPIO1→1B, GPIO2→3B, GPIO3→5B, GPIO10→7B), ei mitattu.

LEDit ohjataan ULN2803:n kautta (low-side), LED+ yhteinen +5V, 220 Ω LEDin katodipuolella.
Protolevyn kytkentä, reikäpaikat ja tehty/tekemättä-tila: `docs/kytkenta.md` ja `docs/kytkenta.png`.
Pinnit on määritelty yhdessä paikassa: `include/io_pins.h` (käyttävät `src/main.cpp` ja `iotest/iotest.cpp`).
IO-testi (nappien tilat + LEDien ohjaus näytöllä): `iotest/iotest.cpp`, ajo `pio run -e iotest -t upload`.

Napit: active LOW, sisäinen pull-up. LEDit: active HIGH (GPIO HIGH → ULN johtaa → LED syttyy).

Näyttö käyttää 8-bittistä rinnakkaisväylää (ei SPI):
- Data: D0-D7 = GPIO 39-48
- WR=8, RD=9, DC=7, CS=6, RST=5, BL=38
- PIN_POWER_ON=15 (asetettava HIGH ennen lcd.init())

## Pelilogiikka

Lamppu n ja nappi n ovat pari (fyysinen numerointi, ks. pinnitaulukko).

- Lamput syttyvät ja sammuvat nopeutuvalla tahdilla omia aikojaan (satunnainen, mutta sama lamppu ei syty kahdesti peräkkäin).
  Ne eivät jää palamaan odottamaan painallusta.
- Pelaajan pitää painaa nappeja samassa järjestyksessä kuin lamput syttyivät. Pelaaja saa jäädä jälkeen:
  syttyneet lamput menevät jonoon, ja painallus täsmää jonon vanhimpaan.
- Peli loppuu, kun: väärä nappi, nappi ennen kuin yhtään lamppua on painamatta (liian aikaisin), tai
  jonossa on yli `MAX_LAG` (4) painamatonta lamppua (liian jäljessä).
- Tahti nopeutuu lineaarisesti: 1000 ms ensimmäisellä lampulla, 350 ms (nopein) 150. lampulla ja siitä eteenpäin
  (noin 4,4 ms lyhenemä per lamppu). Jokaisen lampun väli vaihtelee satunnaisesti ±20 % perusnopeudesta (`JITTER_PCT`). Lamppu palaa 60 % välistä.
  Asetukset: `START_MS`, `MIN_MS`, `FULL_SPEED_LAMP`, `ON_PCT`, `MAX_LAG` main.cpp:n alussa.
- Peli loppuu → LCD näyttää oikeiden painallusten määrän ja ennätyksen (vain RAM:ssa, nollautuu uudelleenkäynnistyksessä)
- Web UI (WebSocket) ei ole vielä toteutettu

## Nappien suodatus ja testit

- `include/button_filter.h`: nappisuodatin. Napit ovat itsenäisiä, ja päällekkäin pidetty nappi (edellinen vielä alhaalla) on
  sallittu. Painallus hyväksytään kun se on vakaa 15 ms, vapautus kun se on vakaa 8 ms, saman napin uusi painallus alle 70 ms
  edellisestä hylätään. Kaksi eri nappia, joiden raaka painallus alkoi alle 12 ms toisistaan, on yhtäaikainen: kumpaakaan ei
  hyväksytä ja peli päättyy ("Kaksi nappia yhtaikaa"). Päätös tehdään alkuhetkestä (`pressOnset`), ei napin indeksistä.
- `include/game_core.h`: pelin jono ja säännöt (ei Arduino-riippuvuuksia).
- Koneella ajettavat testit (ei laitetta tarvita):
  `g++ -std=c++11 -Wall -Wextra -Iinclude tools/host_tests/test_logic.cpp -o /tmp/test_logic && /tmp/test_logic`
- IO-testi (`iotest`) kirjaa jokaisen napin GPIO-reunan keskeytyksestä mikrosekunnin aikaleimalla (`RAW`), LED-ulostulot
  (`LED`) ja suodattimen päätökset (`SUODATETTU`). Käytä kun nappi reagoi väärin tai kaksi nappia reagoi yhtä aikaa.
- Oikosulkutesti (`shorttest`): `pio run -e shorttest -t upload`. Ajaa testin kerran käynnistyksessä ja tulostaa tuloksen konsoliin (BOOT = uusi ajo): mitkä pinnit ovat kiinni
  toisissaan, GND:ssä tai jännitteessä (ei saa painaa nappeja). Testin jälkeen näyttö näyttää syötteiden tilat livenä
  (napit 1–4 ja vapaat pinnit 16, 44, 13, 12, 11; painettu = vihreä) ja muutokset kirjataan konsoliin (`IN ...`). Logiikka: `include/short_scan.h`, testit `tools/host_tests/test_shortscan.cpp`.
- Sarjamonitorin loki (`pio device monitor`): jokainen lamppu (`LAMPPU n PAALLE`), jokainen painallus
  (`PAINETTU nappi x | odotettiin nappi y | jono [...] | OIKEIN/syy`) sekä suodattimen hylkäämät painallukset
  (`HYLATTY ...`) ja lyhyet piikit (`PIIKKI ...`). Aikaleimat ovat ms pelin alusta.
- Näyttö: pelin aikana vain pistemäärä isona (7-segmenttifontti, koko 2). Tahti, jono ja pelin päättymisen syy näkyvät
  vain sarjalokissa. Loppunäytössä on "PELI LOPPUI", pistemäärä samalla koolla ja ennätys.

## Lataus

`platformio.ini`:n `default_envs` on peli, joten `pio run -t upload` lataa vain sen.
Muut envit (`iotest`, `iomap`) ajetaan aina `-e`-lipulla.
