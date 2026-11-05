# Kytkentä

Kaavio: [`kytkenta.png`](kytkenta.png) (luodaan skriptillä `kytkenta.py`).

![kytkentä](kytkenta.png)

## Tehty ja tekemättä

| | Mitä | Tila |
|---|---|---|
| Kiskot | j1 ↔ +1 (5V), j2 ↔ −1 (GND) | ✔ tehty |
| ULN | a28 ↔ +28 (COM), j28 ↔ −28 (GND) | ✔ tehty |
| Painike 1 | j11 ↔ i20 (GPIO1 → ULN 1B), vastus a19–a20, e19 → LED−, a10 → nappi (GPIO43) | ✔ tehty |
| Haaroituksen runko | +4 → LED+, −3 → nappi GND | ✔ tehty |
| Painike 2 | j10 ↔ i22 (GPIO2 → ULN 3B), vastus a21–a22, e21 → LED−, a8 → nappi (GPIO18) | ☐ tekemättä |
| Painike 3 | j9 ↔ i24 (GPIO3 → ULN 5B), vastus a23–a24, e23 → LED−, a7 → nappi (GPIO17) | ☐ tekemättä |
| Painike 4 | j8 ↔ i26 (GPIO10 → ULN 7B), vastus a25–a26, e25 → LED−, a6 → nappi (GPIO21) | ☐ tekemättä |
| Haaroittava kaapeli | +V ja GND jaetaan painikkeelta 1 painikkeille 2, 3 ja 4 | ☐ tekemättä |

### Suunnitelma (painikkeet 2–4)

- **ULN-kanavat 1, 3, 5, 7.** Kanavan 1 vastus on a19–a20, ja sama kuvio toistuu: vastus on aina
  LED−-solmun (sarakkeet 21, 23, 25) ja ULN-lähdön (sarakkeet 22, 24, 26) välissä. Näin jokaisella vastuksella
  on vapaa naapurisarake. Parillisia kanavia 2, 4, 6, 8 ei käytetä. Niiden lähdöt (b21, b23, b25) ovat samoissa
  sarakeryhmissä LED−-solmujen kanssa, mutta lähdöt ovat auki, koska tuloja ei ohjata. Tulot 2B, 4B, 6B, 8B
  kannattaa kytkeä GND:hen.
- **LEDien GPIO:t:** 1, 2, 3, 10 (kaikki i-rivillä, vierekkäin). Johtimet j-rivin reiästä ULN-sarakkeen i-rivin reikään.
- **Nappien GPIO:t:** 43, 18, 17, 21 (b-rivi, johdot a-riviin). GPIO44 jätetty pois, koska se on UART0:n RX.
- **Haaroittava kaapeli:** levylle tulee vain kaksi johtoa (+4 ja −3). LED+ (anodi) ja napin GND ovat kaikilla
  painikkeilla yhteiset, joten ne jaetaan ketjuna painikkeelta toiselle. LED− ei voi olla yhteinen,
  koska jokainen LED kytketään omalla ULN-kanavallaan. Johtoja levylle on yhteensä 2 + 4 (LED−) + 4 (napit) = 10.
- **CLAUDE.md:n pinnitaulukko ei vastaa tätä** (LEDit 11, 12, 13, 21 ja napit 1, 2, 3, 10). Päivitä se ja `main.cpp`,
  kun kytkentä on valmis.


## Protolevy

- Mitat 81 × 55 mm, rivit a–e ja f–j (kuten koekytkentälevyssä; a–e ja f–j ovat omia ryhmiään), keskikanava 7,62 mm.
- Katsotaan paneelin puolelta: **sarake 1 on vasemmalla**, **rivi a on takareunassa**.
- Mitat ja pinnien paikat on luettu STEP-mallista `kotelo/.../electronics_reference_v3_17.step`.
  Sarakkeen n x-koordinaatti: `9,535 − (n−1)·2,54` mm.

## T-Display S3 (sarakkeet 1–12, rivit b ja i)

Näyttö ylöspäin kohti paneelia, ESP32-puoli protolevyä kohti. Sarake 1 on USB-C:n puoleinen pää.

| Sarake | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **rivi b** (piikkirivi P1) | 3V | GND | GND | NC | GPIO16 | **GPIO21** | GPIO17 | GPIO18 | GPIO44 | GPIO43 | GND | GND |
| **rivi i** (piikkirivi P2) | **5V** | GND | NC | NC | **GPIO13** | **GPIO12** | **GPIO11** | **GPIO10** | **GPIO3** | **GPIO2** | **GPIO1** | 3V |

Lihavoidut ovat peliin käytetyt.

## ULN2803 (sarakkeet 20–28, rivit b ja h)

DIP-18-numerointi: pinni 1 on h20, pinni 9 on h28, pinni 10 on b28 ja pinni 18 on b20.

| Pinni | Reikä | Nimi | Käyttö |
|---|---|---|---|
| 1 | h20 | 1B | ← GPIO11 |
| 2 | h21 | 2B | ← GPIO12 |
| 3 | h22 | 3B | ← GPIO13 |
| 4 | h23 | 4B | ← GPIO21 |
| 5–8 | h24–h27 | 5B–8B | ei käytössä |
| 9 | h28 | GND | → T-Displayn GND (esim. b3) |
| 10 | b28 | COM | → +V tai auki (LEDeille ei pakollinen) |
| 15 | b23 | 4C | → LED4 katodi |
| 16 | b22 | 3C | → LED3 katodi |
| 17 | b21 | 2C | → LED2 katodi |
| 18 | b20 | 1C | → LED1 katodi |

## Alkuperäinen suunnitelma (korvattu yllä olevalla)

| Mistä | Mihin |
|---|---|
| i7 (GPIO11) | h20 (ULN 1B) |
| i6 (GPIO12) | h21 (ULN 2B) |
| i5 (GPIO13) | h22 (ULN 3B) |
| b6 (GPIO21) | h23 (ULN 4B) |
| i11 (GPIO1) | Nappi 1, toinen jalka GND:hen |
| i10 (GPIO2) | Nappi 2, toinen jalka GND:hen |
| i9 (GPIO3) | Nappi 3, toinen jalka GND:hen |
| i8 (GPIO10) | Nappi 4, toinen jalka GND:hen |
| h28 (ULN GND) | b3 (T-Displayn GND) |
| b20–b23 (ULN 1C–4C) | LED1–LED4 katodit |
| +V → 220 Ω → LED-anodi | kunkin LEDin anodi |

Napit ovat active LOW sisäisellä pull-upilla, joten ulkoisia vastuksia ei tarvita.
LEDit syttyvät, kun GPIO on HIGH (ULN johtaa).

## Nykyiset hyppylangat (tehty levylle)

Protolevyn ryhmät: sarakkeessa a–e ovat yhdessä ja f–j yhdessä. Siksi esim. j1 on samassa solmussa kuin i1 (5V).

| Lanka | Solmu | Tarkoitus |
|---|---|---|
| j1 ↔ +1 | i1 = T-Displayn **5V** | 5V → + kisko |
| j2 ↔ −1 | i2 = T-Displayn **GND** | GND → − kisko *(kirjoitit +1, oletan −1:ksi, muuten 5V olisi oikosulussa GND:n kanssa)* |
| j11 ↔ i20 | i11 = **GPIO1** ↔ h20 = **ULN 1B** | GPIO1 ohjaa LED1:tä (nykyinen `main.cpp`: `LED_PIN 1`) |
| +28 ↔ a28 | b28 = **ULN 10 (COM)** | COM → + kisko |
| −28 ↔ j28 | h28 = **ULN 9 (GND)** | ULN GND → − kisko *(kirjoitit a28, oletan j28:ksi, koska a28 on ULN:n COM-pinnin ryhmässä)* |

### Nappi 1 + LED 1 (valmis)

| Lanka / osa | Solmu | Tarkoitus |
|---|---|---|
| vastus 220 Ω, a19 ↔ a20 | a20 on samassa ryhmässä kuin b20 = **ULN 1C** | LED-katodi → vastus → ULN 1C |
| e19 → lamppu | e19 on samassa ryhmässä kuin a19 | LED− (katodi) |
| +4 → lamppu | + kisko | LED+ (anodi) |
| −3 → nappi | − kisko (GND) | napin toinen jalka |
| a10 → nappi | a10 on samassa ryhmässä kuin b10 = **GPIO43** | napin signaali (`BTN_PIN 43`) |

Virtapiiri: + kisko (5V) → LED → e19 → 220 Ω → ULN 1C → ULN sisällä GND → − kisko.
ULN 1B (h20) ohjataan GPIO1:llä (j11 ↔ i20).

Huom: `src/main.cpp` ohjaa vielä yhtä LEDiä (GPIO1) ja nappia GPIO43:lla. `CLAUDE.md`:n pinnitaulukko
(LEDit 11, 12, 13, 21 ja napit 1, 2, 3, 10) on suunniteltu lopullinen kytkentä.

## Avoimet kohdat

- **ULN:n pinninumerointi:** DIP-18:ssa pinni 10 on vastapäätä pinniä 9, eli b28. Pinni 16 olisi b22.
  Tarkista moduulin oma numerointi.
- **LEDien +V:** 5V-pinni (i1) toimii vain USB-virralla. Akulla käytettäessä sitä ei syötetä.
  3V3 antaa ULN:n kanssa himmeän valon (~1,4 mA / 220 Ω).
- **Vastus:** 220 Ω on protolevyllä LEDin katodipuolella (a19–a20), ei anodipuolella.
- **Kiskot:** Levyllä on yksi + ja yksi − verkko. Kuvassa ne on piirretty ylös ja alas vain selkeyden vuoksi.
