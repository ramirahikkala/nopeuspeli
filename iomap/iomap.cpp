// IO-kartoitus: selvitä mikä GPIO ohjaa mitäkin lamppua ja mikä GPIO on minkäkin napin.
//
// Moodit (BOOT-nappi GPIO0 vaihtaa):
//   OUT : yksi GPIO kerrallaan HIGH. Katso mikä lamppu syttyy ja kirjaa "GPIO n -> lamppu x".
//         GPIO14 siirtyy seuraavaan GPIO:hon.
//   SWEEP: sama automaattisesti, uusi GPIO 2,5 s välein. GPIO14 = tauko/jatka.
//   Järjestys BOOT:lla: OUT -> SWEEP -> IN -> OUT.
//   IN  : näyttää kaikkien ehdokaspinnien tilan. Paina nappia ja katso mikä ruutu muuttuu vihreäksi.
//
// Ehdokkaat OUT: 1, 2, 3, 10, 11, 12, 13   (T-Displayn P2-piikkirivi)
// Ehdokkaat IN : 16, 17, 18, 21, 43, 44, 11, 12, 13
// IN-moodissa GPIO 1, 2, 3, 10 pidetään LOW:ssa, ettei ULN-tulo kellu ja LED hehku himmeästi.
//
// Aja: pio run -e iomap -t upload && pio device monitor -e iomap

#include "Arduino.h"
#include "TFT_eSPI.h"
#include "pin_config.h"

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

static const uint8_t OUT_PINS[] = {1, 2, 3, 10, 11, 12, 13};
static const uint8_t IN_PINS[]  = {16, 17, 18, 21, 43, 44, 11, 12, 13};
static const uint8_t NOUT = sizeof(OUT_PINS);
static const uint8_t NIN  = sizeof(IN_PINS);

static const uint16_t DARK = 0x2104;
static const uint16_t GREY = 0x8410;

static bool inMode = false;
static bool sweep = false;            // OUT-moodin automaattinen kierto
static bool paused = false;
static unsigned long lastSweep = 0;
static const unsigned long SWEEP_MS = 2500;
static uint8_t outIdx = 0;
static bool pressed[sizeof(IN_PINS)];
static bool drawnPressed[sizeof(IN_PINS)];
static int lastPressed = -1;

// ---------- pinnien asetus ----------

static void enterOut() {
    inMode = false;
    for (uint8_t i = 0; i < NIN; i++) pinMode(IN_PINS[i], INPUT_PULLUP);
    for (uint8_t i = 0; i < NOUT; i++) {
        pinMode(OUT_PINS[i], OUTPUT);
        digitalWrite(OUT_PINS[i], LOW);
    }
    digitalWrite(OUT_PINS[outIdx], HIGH);
}

static void enterIn() {
    inMode = true;
    for (uint8_t i = 0; i < NOUT; i++) {
        pinMode(OUT_PINS[i], OUTPUT);
        digitalWrite(OUT_PINS[i], LOW);
    }
    for (uint8_t i = 0; i < NIN; i++) pinMode(IN_PINS[i], INPUT_PULLUP);
    lastPressed = -1;
}

// ---------- piirto ----------

static void drawHeader(const char *title) {
    tft.fillRect(0, 0, 320, 26, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(title, 6, 3, 4);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawRightString("BOOT: moodi", 314, 8, 2);
}

static void drawOut() {
    tft.fillScreen(TFT_BLACK);
    drawHeader(sweep ? "OUT: sweep" : "OUT: ohjaus");

    tft.setTextColor(GREY, TFT_BLACK);
    tft.drawCentreString("GPIO on HIGH:", 160, 34, 2);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", OUT_PINS[outIdx]);
    tft.drawCentreString(buf, 160, 54, 7);

    // ehdokasrivi
    for (uint8_t i = 0; i < NOUT; i++) {
        int x = 6 + i * 44;
        bool cur = (i == outIdx);
        tft.fillRoundRect(x, 118, 40, 22, 4, cur ? TFT_YELLOW : DARK);
        tft.setTextColor(cur ? TFT_BLACK : GREY, cur ? TFT_YELLOW : DARK);
        snprintf(buf, sizeof(buf), "%u", OUT_PINS[i]);
        tft.drawCentreString(buf, x + 20, 122, 2);
    }
    tft.setTextColor(GREY, TFT_BLACK);
    tft.drawCentreString(sweep ? (paused ? "GPIO14: jatka (TAUOLLA)" : "GPIO14: tauko")
                               : "GPIO14: seuraava GPIO", 160, 150, 2);
}

static void drawCell(uint8_t i) {
    int x = 4 + (i % 3) * 106;
    int y = 30 + (i / 3) * 40;
    uint16_t fill = pressed[i] ? TFT_GREEN : DARK;
    uint16_t fg = pressed[i] ? TFT_BLACK : GREY;
    tft.fillRoundRect(x, y, 102, 36, 5, fill);
    tft.setTextColor(fg, fill);
    char buf[16];
    snprintf(buf, sizeof(buf), "GPIO%u", IN_PINS[i]);
    tft.drawCentreString(buf, x + 51, y + 3, 2);
    tft.drawCentreString(pressed[i] ? "LOW (painettu)" : "HIGH", x + 51, y + 19, 2);
}

static void drawLast() {
    tft.fillRect(0, 150, 320, 20, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    char buf[32];
    if (lastPressed < 0) snprintf(buf, sizeof(buf), "Paina nappia...");
    else snprintf(buf, sizeof(buf), "Viimeksi: GPIO%u", IN_PINS[lastPressed]);
    tft.drawCentreString(buf, 160, 152, 2);
}

static void drawIn() {
    tft.fillScreen(TFT_BLACK);
    drawHeader("IN: napit");
    for (uint8_t i = 0; i < NIN; i++) {
        pressed[i] = (digitalRead(IN_PINS[i]) == LOW);
        drawnPressed[i] = pressed[i];
        drawCell(i);
    }
    drawLast();
}

// ---------- ohjelma ----------

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
    tft.setRotation(1);
    tft.setSwapBytes(true);

    ledcSetup(0, 5000, 8);
    ledcAttachPin(PIN_LCD_BL, 0);
    ledcWrite(0, 200);

    pinMode(PIN_BUTTON_1, INPUT_PULLUP);  // GPIO0, BOOT
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);  // GPIO14

    sweep = true;              // käynnistyy suoraan sweepiin
    lastSweep = millis();
    enterOut();
    drawOut();
    Serial.println("IO-kartoitus kaynnissa. SWEEP-moodi.");
    Serial.printf("OUT GPIO%u = HIGH\n", OUT_PINS[outIdx]);
}

void loop() {
    static bool prevBoot = true, prevNext = true;
    bool boot = digitalRead(PIN_BUTTON_1);
    bool next = digitalRead(PIN_BUTTON_2);

    // BOOT: OUT (käsin) -> SWEEP (automaattinen) -> IN -> OUT ...
    if (prevBoot && !boot) {
        if (inMode) {
            sweep = false; enterOut(); drawOut(); Serial.println("Moodi: OUT (kasin)");
        } else if (sweep) {
            sweep = false; enterIn(); drawIn(); Serial.println("Moodi: IN");
        } else {
            sweep = true; paused = false; lastSweep = millis();
            enterOut(); drawOut(); Serial.println("Moodi: SWEEP");
            Serial.printf("OUT GPIO%u = HIGH\n", OUT_PINS[outIdx]);
        }
        delay(30);
    }

    // GPIO14: seuraava pinni (OUT) tai tauko/jatka (SWEEP)
    if (prevNext && !next && !inMode) {
        if (sweep) {
            paused = !paused;
            lastSweep = millis();
            drawOut();
            Serial.println(paused ? "Sweep: tauko" : "Sweep: jatkuu");
        } else {
            digitalWrite(OUT_PINS[outIdx], LOW);
            outIdx = (outIdx + 1) % NOUT;
            digitalWrite(OUT_PINS[outIdx], HIGH);
            drawOut();
            Serial.printf("OUT GPIO%u = HIGH\n", OUT_PINS[outIdx]);
        }
        delay(30);
    }
    prevBoot = boot;
    prevNext = next;

    // SWEEP: siirry seuraavaan pinniin SWEEP_MS välein
    if (sweep && !paused && !inMode && millis() - lastSweep >= SWEEP_MS) {
        lastSweep = millis();
        digitalWrite(OUT_PINS[outIdx], LOW);
        outIdx = (outIdx + 1) % NOUT;
        digitalWrite(OUT_PINS[outIdx], HIGH);
        drawOut();
        Serial.printf("OUT GPIO%u = HIGH\n", OUT_PINS[outIdx]);
    }

    if (inMode) {
        bool changed = false;
        for (uint8_t i = 0; i < NIN; i++) {
            pressed[i] = (digitalRead(IN_PINS[i]) == LOW);
            if (pressed[i] != drawnPressed[i]) {
                drawnPressed[i] = pressed[i];
                drawCell(i);
                Serial.printf("IN GPIO%u %s\n", IN_PINS[i], pressed[i] ? "LOW (painettu)" : "HIGH");
                if (pressed[i]) { lastPressed = i; changed = true; }
            }
        }
        if (changed) drawLast();
    }
}
