// IO-testi: näyttää nappien tilat ja ohjaa LEDejä, jotta kytkennän voi tarkistaa silmällä.
//
// Näytöllä 4 saraketta (yksi per painike/lamppu):
//   - ympyrä = LEDin ohjaus (keltainen = GPIO HIGH = lamppu pitäisi palaa)
//   - laatikko = napin tila (vihreä = painettu)
//
// Moodin vaihto: T-Displayn BOOT-nappi (GPIO0). Moodit:
//   CHASE  : yksi lamppu kerrallaan kiertää 1 → 2 → 3 → 4
//   MANUAL : lamppu palaa kun sen oman napin pitää alhaalla
//   ALL    : kaikki lamput vilkkuvat yhtä aikaa
//   OFF    : kaikki pois
// Toinen sisäinen nappi (GPIO14) vaihtaa CHASE/ALL-nopeutta.
//
// Sarjaloki (aikaleimat mikrosekunnin tarkkuudella, ms pohjalla, sama kello kaikissa riveissä):
//   RAW        jokainen napin GPIO-reuna keskeytyksestä, myös pompotus ja piikit, ja aika edelliseen
//              reunaan (mikä tahansa nappi) ja saman napin edelliseen reunaan
//   LED        jokainen LED-ulostulon muutos hetkellä, jolloin se kirjoitetaan
//   SUODATETTU pelin nappisuodattimen päätös (hyväksytty, hylätty, yhtäaikainen, piikki)
//              Huom: näytön piirto hidastaa tätä silmukkaa, joten sen ajoitus on karkeampi kuin RAW:n.
// Diagnostiikka "painan nappia 3 ja nappi 2 reagoi myös": katso RAW-rivit. Jos molempien reunat ovat
// samassa kohdassa, johtimet/kontaktit kytkeytyvät. Jos reuna seuraa LED-riviä, kyse on LEDien
// kytkennän aiheuttamasta häiriöstä. Kokeile OFF-moodissa (BOOT x3), jolloin LEDit eivät kytkeydy.
//
// Aja: pio run -e iotest -t upload && pio device monitor -e iotest

#include "Arduino.h"
#include "TFT_eSPI.h"
#include "pin_config.h"
#include "io_pins.h"
#include "button_filter.h"

typedef struct { uint8_t cmd; uint8_t data[14]; uint8_t len; } lcd_cmd_t;
static lcd_cmd_t lcd_st7789v[] = {
    {0x11,{0},0|0x80},{0x3A,{0X05},1},{0xB2,{0X0B,0X0B,0X00,0X33,0X33},5},
    {0xB7,{0X75},1},{0xBB,{0X28},1},{0xC0,{0X2C},1},{0xC2,{0X01},1},
    {0xC3,{0X1F},1},{0xC6,{0X13},1},{0xD0,{0XA7},1},{0xD0,{0XA4,0XA1},2},
    {0xD6,{0XA1},1},
    {0xE0,{0XF0,0X05,0X0A,0X06,0X06,0X03,0X2B,0X32,0X43,0X36,0X11,0X10,0X2B,0X32},14},
    {0xE1,{0XF0,0X08,0X0C,0X0B,0X09,0X24,0X2B,0X22,0X43,0X38,0X15,0X16,0X2F,0X37},14},
};

TFT_eSPI tft = TFT_eSPI();

static const uint8_t N = NUM_PADS;  // LED_PINS ja BTN_PINS: include/io_pins.h

enum Mode { CHASE, MANUAL, ALL, OFF, MODE_COUNT };
static const char *MODE_NAMES[MODE_COUNT] = {"CHASE", "MANUAL", "ALL", "OFF"};

static const uint16_t STEP_MS[] = {1000, 500, 250, 120};
static uint8_t stepIdx = 1;

static Mode mode = CHASE;
static bool ledState[N];       // mitä ohjataan ulos
static bool btnState[N];       // true = painettu
static bool drawnLed[N];
static bool drawnBtn[N];
static bool drawnValid = false;
static unsigned long lastStep = 0;
static uint8_t chasePos = 0;
static bool allOn = false;

// ---------- aikaleimat ja reunaloki ----------

// Aikaleima "[12345.678 ms]". micros() ylivuotaa n. 71 min välein.
#define TS_FMT "[%lu.%03lu ms]"
#define TS_ARGS(us) ((unsigned long)((us) / 1000UL)), ((unsigned long)((us) % 1000UL))

struct Edge { uint32_t us; uint8_t idx; uint8_t level; };
static const uint8_t RING = 128;                 // kahden potenssi
static volatile Edge ring[RING];
static volatile uint8_t ringHead = 0;
static volatile uint16_t ringDrops = 0;
static uint8_t ringTail = 0;

// Keskeytys: tallentaa vain ajan, napin ja luetun tason. Tulostus tehdään silmukassa.
static void IRAM_ATTR onEdge(void *arg) {
    uint8_t idx = (uint8_t)(uintptr_t)arg;
    uint8_t h = ringHead;
    uint8_t next = (h + 1) & (RING - 1);
    if (next == ringTail) { ringDrops++; return; }
    ring[h].us = micros();
    ring[h].idx = idx;
    ring[h].level = digitalRead(BTN_PINS[idx]);
    ringHead = next;
}

static uint32_t lastEdgeUs = 0;
static int8_t lastEdgeIdx = -1;
static uint32_t lastEdgeUsPin[N];
static bool haveEdgePin[N];

static void drainEdges() {
    while (ringTail != ringHead) {
        uint32_t us = ring[ringTail].us;
        uint8_t idx = ring[ringTail].idx;
        uint8_t level = ring[ringTail].level;
        ringTail = (ringTail + 1) & (RING - 1);

        char since[48], sincePin[32];
        if (lastEdgeIdx >= 0)
            snprintf(since, sizeof(since), "+%lu us edellisesta (nappi %u)",
                     (unsigned long)(us - lastEdgeUs), lastEdgeIdx + 1);
        else
            snprintf(since, sizeof(since), "ensimmainen reuna");
        if (haveEdgePin[idx])
            snprintf(sincePin, sizeof(sincePin), "+%lu us samasta", (unsigned long)(us - lastEdgeUsPin[idx]));
        else
            snprintf(sincePin, sizeof(sincePin), "ensimmainen samasta");

        Serial.printf(TS_FMT " RAW nappi %u (GPIO%u) %-4s | %s | %s\n", TS_ARGS(us),
                      idx + 1, BTN_PINS[idx], level ? "HIGH" : "LOW", since, sincePin);
        lastEdgeUs = us;
        lastEdgeIdx = idx;
        lastEdgeUsPin[idx] = us;
        haveEdgePin[idx] = true;
    }
    if (ringDrops) {
        Serial.printf("VAROITUS: %u reunaa menetetty (rengas täynnä)\n", (unsigned)ringDrops);
        ringDrops = 0;
    }
}

static ButtonFilter filter;

// Kirjaa pelin suodattimen päätökset.
static void logFilter(uint8_t accepted) {
    uint32_t us = micros();
    for (uint8_t i = 0; i < N; i++) {
        uint8_t b = 1 << i;
        if (accepted & b)
            Serial.printf(TS_FMT " SUODATETTU nappi %u (GPIO%u): HYVAKSYTTY | raaka LOW alkoi %lu ms\n",
                          TS_ARGS(us), i + 1, BTN_PINS[i], (unsigned long)filter.pressOnset[i]);
        if (filter.rejectedSame & b)
            Serial.printf(TS_FMT " SUODATETTU nappi %u (GPIO%u): HYLATTY, sama nappi alle %u ms edellisesta\n",
                          TS_ARGS(us), i + 1, BTN_PINS[i], filter.repressMs);
        if (filter.simultaneous & b)
            Serial.printf(TS_FMT " SUODATETTU nappi %u (GPIO%u): YHTAAIKAINEN | raaka LOW alkoi %lu ms\n",
                          TS_ARGS(us), i + 1, BTN_PINS[i], (unsigned long)filter.pressOnset[i]);
        if (filter.glitch & b)
            Serial.printf(TS_FMT " SUODATETTU nappi %u (GPIO%u): PIIKKI %u ms, ei painallus\n",
                          TS_ARGS(us), i + 1, BTN_PINS[i], filter.glitchLen[i]);
    }
}

static const int COL_W = 80;
static const int LAMP_Y = 72, LAMP_R = 22;
static const int BTN_Y = 128, BTN_H = 26;

static void drawHeader() {
    tft.fillRect(0, 0, 320, 24, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("IO-testi", 6, 4, 4);
    char buf[40];
    snprintf(buf, sizeof(buf), "%s  %ums", MODE_NAMES[mode], STEP_MS[stepIdx]);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawRightString(buf, 314, 6, 2);
}

static void drawStatic() {
    tft.fillScreen(TFT_BLACK);
    drawHeader();
    for (uint8_t i = 0; i < N; i++) {
        int cx = i * COL_W + COL_W / 2;
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        char buf[24];
        snprintf(buf, sizeof(buf), "%u", i + 1);
        tft.drawCentreString(buf, cx, 28, 4);
        tft.setTextColor(0x8410, TFT_BLACK);
        snprintf(buf, sizeof(buf), "LED GPIO%u", LED_PINS[i]);
        tft.drawCentreString(buf, cx, 104, 2);
        snprintf(buf, sizeof(buf), "BTN GPIO%u", BTN_PINS[i]);
        tft.drawCentreString(buf, cx, 158 - 4, 2);
        if (i > 0) tft.drawFastVLine(i * COL_W, 28, 142, 0x2104);
    }
    drawnValid = false;
}

static void drawColumn(uint8_t i) {
    int cx = i * COL_W + COL_W / 2;
    // lamppu
    if (ledState[i]) {
        tft.fillCircle(cx, LAMP_Y, LAMP_R, TFT_YELLOW);
        tft.drawCircle(cx, LAMP_Y, LAMP_R, TFT_WHITE);
    } else {
        tft.fillCircle(cx, LAMP_Y, LAMP_R, 0x2104);
        tft.drawCircle(cx, LAMP_Y, LAMP_R, 0x6B4D);
    }
    // nappi
    uint16_t fill = btnState[i] ? TFT_GREEN : 0x2104;
    uint16_t fg = btnState[i] ? TFT_BLACK : 0x8410;
    tft.fillRoundRect(cx - 32, BTN_Y - BTN_H - 2, 64, BTN_H, 4, fill);
    tft.setTextColor(fg, fill);
    tft.drawCentreString(btnState[i] ? "PAINETTU" : "auki", cx, BTN_Y - BTN_H + 4, 2);
}

static void applyOutputs() {
    static bool written[N];
    static bool first = true;
    for (uint8_t i = 0; i < N; i++) {
        if (first || written[i] != ledState[i]) {
            digitalWrite(LED_PINS[i], ledState[i] ? HIGH : LOW);
            written[i] = ledState[i];
            if (!first) {
                uint32_t us = micros();
                Serial.printf(TS_FMT " LED %u (GPIO%u) %s\n", TS_ARGS(us), i + 1, LED_PINS[i],
                              ledState[i] ? "ON" : "OFF");
            }
        }
    }
    first = false;
}

static void setMode(Mode m) {
    mode = m;
    chasePos = 0;
    allOn = false;
    lastStep = 0;
    for (uint8_t i = 0; i < N; i++) ledState[i] = false;
    drawHeader();
    Serial.printf("Moodi: %s\n", MODE_NAMES[mode]);
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    tft.begin();
    for (uint8_t i = 0; i < sizeof(lcd_st7789v) / sizeof(lcd_cmd_t); i++) {
        tft.writecommand(lcd_st7789v[i].cmd);
        for (int j = 0; j < (lcd_st7789v[i].len & 0x7f); j++)
            tft.writedata(lcd_st7789v[i].data[j]);
        if (lcd_st7789v[i].len & 0x80) delay(120);
    }
    tft.setRotation(1);  // 180° käännetty (aiemmin 3)
    tft.setSwapBytes(true);

    ledcSetup(0, 5000, 8);
    ledcAttachPin(PIN_LCD_BL, 0);
    ledcWrite(0, 200);

    for (uint8_t i = 0; i < N; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
        pinMode(BTN_PINS[i], INPUT_PULLUP);
    }
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);  // GPIO0, BOOT
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);  // GPIO14

    delay(5);                             // pull-upit asettumaan
    for (uint8_t i = 0; i < N; i++) btnState[i] = (digitalRead(BTN_PINS[i]) == LOW);
    filter.begin(N, btnState, millis());
    for (uint8_t i = 0; i < N; i++)
        attachInterruptArg(digitalPinToInterrupt(BTN_PINS[i]), onEdge, (void *)(uintptr_t)i, CHANGE);

    drawStatic();
    Serial.println("IO-testi kaynnissa. Loki: RAW = GPIO-reunat (us), LED = ulostulot, SUODATETTU = suodattimen paatos");
}

void loop() {
    unsigned long now = millis();

    // sisäiset napit (reunantunnistus)
    static bool prevBoot = true, prevSpeed = true;
    bool boot = digitalRead(PIN_BUTTON_1);
    bool speed = digitalRead(PIN_BUTTON_2);
    if (prevBoot && !boot) { setMode((Mode)((mode + 1) % MODE_COUNT)); delay(30); }
    if (prevSpeed && !speed) {
        stepIdx = (stepIdx + 1) % (sizeof(STEP_MS) / sizeof(STEP_MS[0]));
        drawHeader();
        Serial.printf("Askel: %u ms\n", STEP_MS[stepIdx]);
        delay(30);
    }
    prevBoot = boot;
    prevSpeed = speed;

    // lue napit
    for (uint8_t i = 0; i < N; i++) btnState[i] = (digitalRead(BTN_PINS[i]) == LOW);

    // kirjaa raa'at reunat (keskeytyksistä) ja suodattimen päätökset aikaleimoin
    drainEdges();
    logFilter(filter.update(btnState, millis()));

    // ohjaa ulostulot moodin mukaan
    switch (mode) {
        case CHASE:
            if (now - lastStep >= STEP_MS[stepIdx]) {
                lastStep = now;
                for (uint8_t i = 0; i < N; i++) ledState[i] = (i == chasePos);
                chasePos = (chasePos + 1) % N;
            }
            break;
        case MANUAL:
            for (uint8_t i = 0; i < N; i++) ledState[i] = btnState[i];
            break;
        case ALL:
            if (now - lastStep >= STEP_MS[stepIdx]) {
                lastStep = now;
                allOn = !allOn;
                for (uint8_t i = 0; i < N; i++) ledState[i] = allOn;
            }
            break;
        case OFF:
            for (uint8_t i = 0; i < N; i++) ledState[i] = false;
            break;
        default: break;
    }
    applyOutputs();

    // piirrä vain muuttuneet sarakkeet
    for (uint8_t i = 0; i < N; i++) {
        if (!drawnValid || ledState[i] != drawnLed[i] || btnState[i] != drawnBtn[i]) {
            drawColumn(i);
            drawnLed[i] = ledState[i];
            drawnBtn[i] = btnState[i];
        }
    }
    drawnValid = true;
}
