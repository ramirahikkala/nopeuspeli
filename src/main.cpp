// Nopeuspeli: nopeutuvalla tahdilla syttyy ja sammuu satunnainen lamppu. Lamput menevät
// omaa tahtiaan eivätkä odota pelaajaa. Pelaajan pitää painaa nappeja samassa järjestyksessä
// kuin lamput syttyivät (lamppu n <-> nappi n). Pelaaja saa jäädä muutaman lampun jälkeen.
//
// Säännöt ovat tiedostossa include/game_core.h ja nappien suodatus tiedostossa
// include/button_filter.h. Molemmat on testattu koneella (tools/host_tests).
//
// Jokainen syttynyt lamppu menee jonoon. Painallus täsmää jonon vanhimpaan riippumatta siitä,
// palaako lamppu vielä:
//   - oikea nappi        -> poistuu jonosta, piste
//   - väärä nappi        -> peli loppuu
//   - nappi kun jono on tyhjä -> peli loppuu (liian aikaisin)
//   - jonossa yli MAX_LAG painamatonta -> peli loppuu (liian jäljessä)
// Tahti nopeutuu lineaarisesti: START_MS ensimmäisellä lampulla, MIN_MS lampulla FULL_SPEED_LAMP ja siitä eteenpäin.

#include "Arduino.h"
#include "TFT_eSPI.h"
#include "pin_config.h"
#include "io_pins.h"
#include "button_filter.h"
#include "game_core.h"

typedef struct { uint8_t cmd; uint8_t data[14]; uint8_t len; } lcd_cmd_t;
lcd_cmd_t lcd_st7789v[] = {
    {0x11,{0},0|0x80},{0x3A,{0X05},1},{0xB2,{0X0B,0X0B,0X00,0X33,0X33},5},
    {0xB7,{0X75},1},{0xBB,{0X28},1},{0xC0,{0X2C},1},{0xC2,{0X01},1},
    {0xC3,{0X1F},1},{0xC6,{0X13},1},{0xD0,{0XA7},1},{0xD0,{0XA4,0XA1},2},
    {0xD6,{0XA1},1},
    {0xE0,{0XF0,0X05,0X0A,0X06,0X06,0X03,0X2B,0X32,0X43,0X36,0X11,0X10,0X2B,0X32},14},
    {0xE1,{0XF0,0X08,0X0C,0X0B,0X09,0X24,0X2B,0X22,0X43,0X38,0X15,0X16,0X2F,0X37},14},
};

TFT_eSPI tft = TFT_eSPI();

// ---------- asetukset ----------

static const uint16_t START_MS = 1000;        // ensimmäinen tahti (uusi lamppu joka START_MS)
static const uint16_t MIN_MS = 350;           // nopein tahti
static const uint16_t FULL_SPEED_LAMP = 150;  // tällä lampulla saavutetaan MIN_MS (lineaarinen nopeutus)
static const uint8_t  ON_PCT = 60;            // lamppu palaa tämän % tahdista, loput on tauko
static const uint8_t  JITTER_PCT = 20;        // satunnainen heitto: tahti vaihtelee +-JITTER_PCT % perusnopeudesta
static const uint8_t  MAX_LAG = 4;            // montako lamppua saa olla painamatta

static const uint16_t GREY = 0x8410;

// ---------- tila ----------

enum State { IDLE, PLAYING, GAME_OVER };

static State state = IDLE;
static GameCore core(MAX_LAG);
static ButtonFilter filter;   // oletukset: ks. button_filter.h (15/25/100/40 ms)

static uint16_t best = 0;
static uint8_t lastTarget = 0;            // edellinen sytytetty lamppu (ei toistoa peräkkäin)
static uint16_t interval = START_MS;      // nykyisen lampun tahti (ms)
static uint16_t lampNo = 0;                  // montako lamppua on sytytetty tässä pelissä
static bool lampLit = false;
static unsigned long lampOffAt = 0;
static unsigned long nextLightAt = 0;
static unsigned long lockUntil = 0;
static unsigned long lastChase = 0;
static uint8_t chasePos = 0;
static unsigned long gameStartAt = 0;        // lokin aikaleimat ovat ms pelin alusta
static unsigned long lastLampEventAt = 0;    // viimeisin lampun syttyminen tai sammuminen

static unsigned long gameT() { return millis() - gameStartAt; }

// ---------- LEDit ----------

static void setLeds(uint8_t mask) {
    for (uint8_t i = 0; i < NUM_PADS; i++)
        digitalWrite(LED_PINS[i], ((mask >> i) & 1) ? HIGH : LOW);
}

// ---------- piirto ----------

static void drawIdle() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("NOPEUSPELI", 160, 50, 4);
    tft.setTextColor(GREY, TFT_BLACK);
    tft.drawCentreString("Paina mita tahansa nappia", 160, 96, 2);
    if (best > 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Ennatys: %u", best);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawCentreString(buf, 160, 124, 2);
    }
}

// Piirtää numeron isolla 7-segmenttifontilla vaakasuunnassa keskelle ja pystysuunnassa
// alueen [top, top + areaH) keskelle. Koko valitaan suurimmaksi (enintään maxSize), joka mahtuu.
// Fontti 7: 48 px korkea, numero n. 32 px leveä (kerrotaan koolla).
static void drawBigNumber(unsigned n, int top, int areaH, int maxSize, uint16_t color) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%u", n);
    int digits = (int)strlen(buf);
    int size = maxSize;
    while (size > 1 && (digits * 32 * size > 316 || 48 * size > areaH)) size--;
    tft.setTextSize(size);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawCentreString(buf, 160, top + (areaH - 48 * size) / 2, 7);
    tft.setTextSize(1);
}

// Pelin aikana näytöllä on vain pistemäärä, mahdollisimman isona.
static void drawPlay() {
    tft.fillScreen(TFT_BLACK);
    drawBigNumber(core.score(), 0, 170, 2, TFT_WHITE);
}

static const char *reasonText(GameCore::Result r) {
    switch (r) {
        case GameCore::WRONG:  return "Vaara nappi";
        case GameCore::EARLY:  return "Liian aikaisin";
        case GameCore::SIMUL:  return "Kaksi nappia yhtaikaa";
        default:               return "Liian jaljessa";
    }
}

// Loppunäyttö: "PELI LOPPUI", pistemäärä isona ja ennätys. Syy näkyy vain sarjalokissa.
static void drawGameOver() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("PELI LOPPUI", 160, 6, 4);
    drawBigNumber(core.score(), 38, 100, 2, TFT_GREEN);
    char buf[24];
    snprintf(buf, sizeof(buf), "Ennatys: %u", best);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawCentreString(buf, 160, 142, 4);
}

// ---------- napit ----------

// Palauttaa bittimaskin hyväksytyistä painalluksista (enintään yksi bitti), ks. button_filter.h.
static uint8_t pollButtons() {
    bool raw[NUM_PADS];
    for (uint8_t i = 0; i < NUM_PADS; i++) raw[i] = (digitalRead(BTN_PINS[i]) == LOW);
    return filter.update(raw, millis());
}

// Nielee painallukset, jotka tapahtuivat blokkaavan viiveen aikana.
static void flushButtons() {
    unsigned long t = millis();
    while (millis() - t < 40) pollButtons();
}

// ---------- lokit ----------

// Jono muodossa "2 4 1" (vanhin ensin, 1-pohjaiset numerot) tai "-" kun tyhjä.
static void queueStr(char *out, size_t n) {
    size_t p = 0;
    out[0] = 0;
    for (uint8_t i = 0; i < core.pending() && p + 4 < n; i++)
        p += snprintf(out + p, n - p, "%s%u", i ? " " : "", core.at(i) + 1);
    if (core.pending() == 0) snprintf(out, n, "-");
}

// Kirjaa suodattimen hylkäämät painallukset ja piikit. Ne eivät tuota peliin tapahtumaa,
// mutta pelaaja voi kokea ne painalluksina.
static void logFilterEvents() {
    for (uint8_t i = 0; i < NUM_PADS; i++) {
        uint8_t b = 1 << i;
        if (filter.rejectedSame & b)
            Serial.printf("[%lu ms] HYLATTY nappi %u (GPIO%u): sama nappi alle %u ms edellisesta"
                          " | raaka LOW alkoi %ld ms\n",
                          gameT(), i + 1, BTN_PINS[i], filter.repressMs,
                          (long)(filter.pressOnset[i] - gameStartAt));
        if (filter.simultaneous & b)
            Serial.printf("[%lu ms] YHTAAIKAINEN nappi %u (GPIO%u) | raaka LOW alkoi %ld ms\n",
                          gameT(), i + 1, BTN_PINS[i],
                          (long)(filter.pressOnset[i] - gameStartAt));
        if (filter.glitch & b)
            Serial.printf("[%lu ms] PIIKKI nappi %u (GPIO%u): %u ms, ei painallus\n",
                          gameT(), i + 1, BTN_PINS[i], filter.glitchLen[i]);
    }
}

// ---------- pelilogiikka ----------

static void gameOver(GameCore::Result r) {
    setLeds(0);
    for (uint8_t i = 0; i < 3; i++) {
        setLeds(0x0F); delay(150);
        setLeds(0);    delay(150);
    }
    if (core.score() > best) best = core.score();
    state = GAME_OVER;
    drawGameOver();
    flushButtons();
    lockUntil = millis() + 800;
    Serial.printf("Peli loppui (%s), oikein: %u, ennatys: %u\n", reasonText(r), core.score(), best);
}

// Sytyttää uuden satunnaisen lampun ja lisää sen jonoon.
// Lineaarinen tahti: START_MS lampulla 1, MIN_MS lampulla FULL_SPEED_LAMP ja siitä eteenpäin.
// Lasketaan lampun numerosta, ettei pyöristys kerry.
static uint16_t paceForLamp(uint16_t lamp) {
    if (lamp >= FULL_SPEED_LAMP) return MIN_MS;
    return (uint16_t)(START_MS - (uint32_t)(START_MS - MIN_MS) * (lamp - 1) / (FULL_SPEED_LAMP - 1));
}

static void lightNext(unsigned long now) {
    lampNo++;
    // Heitto lasketaan perusnopeudesta joka lampulle erikseen, joten se ei kerry eikä muuta nopeutuskäyrää.
    uint16_t base = paceForLamp(lampNo);
    interval = (uint16_t)((uint32_t)base * (100 + random(-(int)JITTER_PCT, (int)JITTER_PCT + 1)) / 100);
    // Satunnainen, mutta ei sama lamppu kahdesti peräkkäin: arvotaan muiden joukosta.
    uint8_t target = random(NUM_PADS);
    if (lampNo > 1 && target == lastTarget) target = (target + 1 + random(NUM_PADS - 1)) % NUM_PADS;
    lastTarget = target;
    GameCore::Result r = core.lampLit(target);
    char qs[40];
    queueStr(qs, sizeof(qs));
    Serial.printf("[%lu ms] LAMPPU %u PAALLE (GPIO%u) | tahti %u ms | jono [%s]\n",
                  gameT(), target + 1, LED_PINS[target], interval, qs);
    lastLampEventAt = now;

    if (r != GameCore::RUNNING) { gameOver(r); return; }

    setLeds(1 << target);
    lampLit = true;
    lampOffAt = now + (unsigned long)interval * ON_PCT / 100;
    nextLightAt = now + interval;
}

static void startGame() {
    core.reset();
    interval = START_MS;
    lampNo = 0;
    lampLit = false;
    setLeds(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawCentreString("VALMIS", 160, 70, 4);
    setLeds(0x0F);
    delay(350);
    setLeds(0);
    delay(450);
    flushButtons();
    state = PLAYING;
    gameStartAt = lastLampEventAt = millis();
    Serial.println("=== Peli alkoi ===");
    drawPlay();
    nextLightAt = millis() + 400;                // ensimmäinen lamppu hetken päästä
}

// Käsittelee yhden hyväksytyn painalluksen. Palauttaa false, jos peli päättyi.
static bool handlePress(uint8_t idx, unsigned long now) {
    char qs[40], expected[24];
    queueStr(qs, sizeof(qs));                      // jono ennen painallusta
    if (core.pending())
        snprintf(expected, sizeof(expected), "nappi %u (GPIO%u)",
                 core.at(0) + 1, BTN_PINS[core.at(0)]);
    else
        snprintf(expected, sizeof(expected), "ei mitaan (jono tyhja)");

    GameCore::Result r = core.press(idx);
    Serial.printf("[%lu ms] PAINETTU nappi %u (GPIO%u) | odotettiin %s | jono [%s] | %s"
                  " | lamppu muuttui %lu ms sitten | raaka LOW alkoi %ld ms\n",
                  gameT(), idx + 1, BTN_PINS[idx], expected, qs,
                  r == GameCore::RUNNING ? "OIKEIN" : reasonText(r),
                  now - lastLampEventAt,
                  (long)(filter.pressOnset[idx] - gameStartAt));
    if (r != GameCore::RUNNING) { gameOver(r); return false; }
    drawPlay();
    return true;
}

// ---------- Arduino ----------

void setup() {
    Serial.begin(115200);

    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    tft.begin();
    for (uint8_t i = 0; i < sizeof(lcd_st7789v)/sizeof(lcd_cmd_t); i++) {
        tft.writecommand(lcd_st7789v[i].cmd);
        for (int j = 0; j < (lcd_st7789v[i].len & 0x7f); j++)
            tft.writedata(lcd_st7789v[i].data[j]);
        if (lcd_st7789v[i].len & 0x80) delay(120);
    }
    tft.setRotation(1);
    tft.setSwapBytes(true);

    ledcSetup(0, 5000, 8);
    ledcAttachPin(PIN_LCD_BL, 0);
    ledcWrite(0, 200);

    bool raw[NUM_PADS];
    for (uint8_t i = 0; i < NUM_PADS; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
        pinMode(BTN_PINS[i], INPUT_PULLUP);
    }
    delay(5);                                      // pull-upit asettumaan
    for (uint8_t i = 0; i < NUM_PADS; i++) raw[i] = (digitalRead(BTN_PINS[i]) == LOW);
    filter.begin(NUM_PADS, raw, millis());

    randomSeed(esp_random());
    state = IDLE;
    drawIdle();
}

void loop() {
    unsigned long now = millis();
    uint8_t pressed = pollButtons();
    logFilterEvents();

    switch (state) {
        case IDLE:
            if (now - lastChase >= 350) {          // houkutteleva kiertovalo
                lastChase = now;
                setLeds(1 << chasePos);
                chasePos = (chasePos + 1) % NUM_PADS;
            }
            if (pressed) startGame();
            break;

        case PLAYING:
            // 1) painallukset jonon vanhinta vastaan (valon tila ei vaikuta).
            //    Yhtäaikaiset napit päättävät pelin. Päällekkäin pidetty (edellinen vielä alhaalla)
            //    on sallittu ja lasketaan normaalisti.
            if (filter.simultaneous) {
                Serial.printf("[%lu ms] KAKSI NAPPIA YHTA AIKAA -> peli loppuu\n", gameT());
                gameOver(GameCore::SIMUL);
                break;
            }
            while (pressed) {
                uint8_t idx = 0xFF;                        // aikaisimmin alkanut ensin
                for (uint8_t i = 0; i < NUM_PADS; i++)
                    if (((pressed >> i) & 1) &&
                        (idx == 0xFF || (int32_t)(filter.pressOnset[i] - filter.pressOnset[idx]) < 0))
                        idx = i;
                pressed &= ~(1 << idx);
                if (!handlePress(idx, now)) break;
            }
            if (state != PLAYING) break;

            // 2) lamput kulkevat omaa tahtiaan
            if (lampLit && now >= lampOffAt) {
                lampLit = false;
                setLeds(0);
                lastLampEventAt = now;
                Serial.printf("[%lu ms] lamppu POIS\n", gameT());
            }
            if (now >= nextLightAt) lightNext(now);
            break;

        case GAME_OVER:
            if (pressed && now >= lockUntil) startGame();
            break;
    }
}
