// Oikosulkutesti: löytää pinnit, jotka ovat sähköisesti kiinni toisissaan, GND:ssä tai jännitteessä.
//
// Tarkoitus: selvittää esim. "nappi 2 ja nappi 3 triggaavat molemmat" (GPIO17 ja GPIO18).
// Testi ajetaan KERRAN käynnistyksen yhteydessä ja tulos tulostetaan konsoliin (sarjamonitori)
// heti kun monitori on auki (enintään 10 s odotus, ei blokkaa näyttöä).
// Testin jälkeen näytölle tulee syötteiden live-näkymä: napit ja muut vapaat pinnit, painettu = vihreä.
// Sillä testaat jokaisen napin painalluksen erikseen. Muutokset kirjataan myös konsoliin.
// Uusi oikosulkutesti: paina BOOT-nappia (GPIO0) tai resetoi laite.
//
// ÄLÄ PAINA NAPPEJA testin aikana: painettua nappia ei voi erottaa oikosulusta GND:hen.
// Testi ajaa yhden pinnin kerrallaan heikosti HIGH:iin (~5 mA, ei vahingoita painettua nappia
// tai oikosulkua). LEDien ULN-tulot voivat välähtää ~1 ms kerrallaan, mikä ei näy silmällä.
//
// Testattavat pinnit (T-Displayn piikkirivit) ja reikä levyllä:
//   P1 (rivi b): GPIO16 b5, GPIO21 b6, GPIO17 b7, GPIO18 b8, GPIO44 b9, GPIO43 b10
//   P2 (rivi i): GPIO13 i5, GPIO12 i6, GPIO11 i7, GPIO10 i8, GPIO3 i9, GPIO2 i10, GPIO1 i11
//
// Aja: pio run -e shorttest -t upload && pio device monitor -e shorttest

#include "Arduino.h"
#include "TFT_eSPI.h"
#include "driver/gpio.h"
#include "pin_config.h"
#include "short_scan.h"
#include "io_pins.h"

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

static const uint8_t PINS[]  = {16,   21,   17,   18,   44,   43,   13,   12,   11,   10,   3,    2,    1};
static const char *HOLES[]   = {"b5", "b6", "b7", "b8", "b9", "b10", "i5", "i6", "i7", "i8", "i9", "i10", "i11"};
static const uint8_t NP = sizeof(PINS);

static const uint16_t GREY = 0x8410;
static const uint16_t ORANGE = 0xFD20;

// ---------- laitetaso ----------

class ArduinoHal : public ShortScanHal {
public:
    void pulldown(uint8_t i) {
        gpio_set_drive_capability((gpio_num_t)PINS[i], GPIO_DRIVE_CAP_2);   // oletus
        pinMode(PINS[i], INPUT_PULLDOWN);
    }
    void driveHigh(uint8_t i) {
        pinMode(PINS[i], OUTPUT);
        gpio_set_direction((gpio_num_t)PINS[i], GPIO_MODE_INPUT_OUTPUT);    // itseluku
        gpio_set_drive_capability((gpio_num_t)PINS[i], GPIO_DRIVE_CAP_0);   // heikoin (~5 mA)
        digitalWrite(PINS[i], HIGH);
    }
    void driveLow(uint8_t i) {
        pinMode(PINS[i], OUTPUT);
        gpio_set_direction((gpio_num_t)PINS[i], GPIO_MODE_INPUT_OUTPUT);
        gpio_set_drive_capability((gpio_num_t)PINS[i], GPIO_DRIVE_CAP_0);   // heikoin (~5 mA)
        digitalWrite(PINS[i], LOW);
    }
    bool read(uint8_t i) { return digitalRead(PINS[i]) != 0; }
    void settle() { delayMicroseconds(800); }
};

static ArduinoHal hal;
static ShortScan scan;

// ---------- tulosrivit (näyttö) ----------

struct Line { char text[48]; uint16_t color; };
static const uint8_t MAX_LINES = 24;
static Line lines[MAX_LINES];
static uint8_t nLines = 0;

static void addLine(uint16_t color, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void addLine(uint16_t color, const char *fmt, ...) {
    if (nLines >= MAX_LINES) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lines[nLines].text, sizeof(lines[nLines].text), fmt, ap);
    va_end(ap);
    lines[nLines].color = color;
    nLines++;
}

static uint8_t nProblem = 0;     // ongelmarivit (oikosulut, GND); loput rivit ovat pelkkiä huomioita

static void buildLines() {
    nLines = 0;
    uint8_t label[ShortScan::MAXP];
    scan.groups(label);
    // 1) oikosulut: kaikki toisissaan kiinni olevat pinnit yhdellä rivillä
    for (uint8_t root = 0; root < NP; root++) {
        uint8_t members = 0;
        for (uint8_t i = 0; i < NP; i++) if (label[i] == root) members++;
        if (members < 2) continue;
        char buf[48];
        size_t p = 0;
        buf[0] = 0;
        for (uint8_t i = 0; i < NP; i++) {
            if (label[i] != root) continue;
            p += snprintf(buf + p, sizeof(buf) - p, "%sGPIO%u(%s)", p ? " = " : "", PINS[i], HOLES[i]);
            if (p >= sizeof(buf)) break;
        }
        addLine(TFT_RED, "%s", buf);
    }
    // 2) GND:hen kiinni
    for (uint8_t i = 0; i < NP; i++)
        if (scan.stuckLow[i]) addLine(ORANGE, "GPIO%u(%s): kiinni GND:ssa / painettu", PINS[i], HOLES[i]);
    nProblem = nLines;
    // 3) huomiot: ylösvedetyt pinnit (ulkoinen pull-up tai jännite, ei yksin vika)
    for (uint8_t i = 0; i < NP; i++)
        if (scan.stuckHigh[i]) addLine(TFT_YELLOW, "GPIO%u(%s): vedetty ylos", PINS[i], HOLES[i]);
}

// ---------- syötteiden live-näkymä (testin jälkeen) ----------

// Napit ensin, sitten muut vapaat pinnit (jos nappi on kytketty väärään pinniin, se näkyy täällä).
// ULN-tulot (GPIO1, 2, 3, 10) ovat LED-ohjauksia: ne pidetään LOW:ssa eikä niitä näytetä.
static const uint8_t LIVE_PINS[] = {21, 18, 17, 43, 16, 44, 13, 12, 11};
static const uint8_t NLIVE = sizeof(LIVE_PINS);
static bool livePressed[NLIVE];
static bool liveDrawn[NLIVE];
static bool liveValid = false;

static int buttonNumber(uint8_t pin) {
    for (uint8_t i = 0; i < NUM_PADS; i++) if (BTN_PINS[i] == pin) return i + 1;
    return 0;
}

static void drawLiveHeader() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("SYOTTEET", 6, 2, 4);
    tft.setTextColor(GREY, TFT_BLACK);
    tft.drawRightString("BOOT: oikosulkutesti", 314, 8, 2);

    char buf[64];
    if (nProblem > 0) {
        if (nProblem > 1) snprintf(buf, sizeof(buf), "%s (+%u)", lines[0].text, nProblem - 1);
        else              snprintf(buf, sizeof(buf), "%s", lines[0].text);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString(buf, 6, 30, 2);
    } else if (nLines > 0) {
        size_t p = snprintf(buf, sizeof(buf), "Ei oikosulkuja. Ylos:");
        for (uint8_t i = 0; i < NP && p < sizeof(buf); i++)
            if (scan.stuckHigh[i]) p += snprintf(buf + p, sizeof(buf) - p, " %u(%s)", PINS[i], HOLES[i]);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString(buf, 6, 30, 2);
    } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("Oikosulkutesti: ei oikosulkuja", 6, 30, 2);
    }
}

static void drawLiveCell(uint8_t i) {
    int x = 4 + (i % 3) * 106;
    int y = 50 + (i / 3) * 38;
    bool on = livePressed[i];
    uint16_t fill = on ? TFT_GREEN : 0x2104;
    uint16_t fg = on ? TFT_BLACK : GREY;
    tft.fillRoundRect(x, y, 102, 34, 5, fill);
    tft.setTextColor(fg, fill);
    char buf[24];
    int nb = buttonNumber(LIVE_PINS[i]);
    if (nb) snprintf(buf, sizeof(buf), "N%d GPIO%u", nb, LIVE_PINS[i]);
    else    snprintf(buf, sizeof(buf), "GPIO%u", LIVE_PINS[i]);
    tft.drawCentreString(buf, x + 51, y + 2, 2);
    tft.drawCentreString(on ? "PAINETTU (LOW)" : "HIGH", x + 51, y + 18, 2);
}

static void startLive() {
    // LED-ohjaukset pois, syötteet pull-upilla
    for (uint8_t i = 0; i < NP; i++) {
        bool live = false;
        for (uint8_t k = 0; k < NLIVE; k++) if (LIVE_PINS[k] == PINS[i]) live = true;
        if (!live) { pinMode(PINS[i], OUTPUT); digitalWrite(PINS[i], LOW); }
    }
    for (uint8_t k = 0; k < NLIVE; k++) pinMode(LIVE_PINS[k], INPUT_PULLUP);
    delay(5);                              // pull-upit asettumaan
    drawLiveHeader();
    liveValid = false;
}

static void liveUpdate() {
    for (uint8_t i = 0; i < NLIVE; i++) {
        livePressed[i] = (digitalRead(LIVE_PINS[i]) == LOW);
        if (!liveValid || livePressed[i] != liveDrawn[i]) {
            if (liveValid) {
                uint32_t us = micros();
                int nb = buttonNumber(LIVE_PINS[i]);
                char name[16];
                if (nb) snprintf(name, sizeof(name), "nappi %d", nb);
                else    snprintf(name, sizeof(name), "-");
                Serial.printf("[%lu.%03lu ms] IN %s GPIO%u %s\n", (unsigned long)(us / 1000),
                              (unsigned long)(us % 1000), name, LIVE_PINS[i],
                              livePressed[i] ? "LOW (painettu)" : "HIGH");
            }
            drawLiveCell(i);
            liveDrawn[i] = livePressed[i];
        }
    }
    liveValid = true;
}

// ---------- konsoliraportti ----------

static void printReport() {
    Serial.println();
    Serial.println("=== OIKOSULKUTESTI: TULOS ===");
    Serial.print("Testatut pinnit:");
    for (uint8_t i = 0; i < NP; i++) Serial.printf(" GPIO%u(%s)", PINS[i], HOLES[i]);
    Serial.println();

    bool any = false;
    uint8_t nHigh = 0;

    // ryhmät (kaikki toisissaan kiinni olevat pinnit yhdellä rivillä)
    uint8_t label[ShortScan::MAXP];
    scan.groups(label);
    for (uint8_t root = 0; root < NP; root++) {
        uint8_t members = 0;
        for (uint8_t i = 0; i < NP; i++) if (label[i] == root) members++;
        if (members < 2) continue;
        any = true;
        Serial.print("OIKOSULKU (kiinni toisissaan):");
        for (uint8_t i = 0; i < NP; i++)
            if (label[i] == root) Serial.printf(" GPIO%u(%s)", PINS[i], HOLES[i]);
        Serial.println();
    }
    // yksittäiset parit
    for (uint8_t i = 0; i < NP; i++)
        for (uint8_t j = i + 1; j < NP; j++)
            if (scan.pair[i][j]) {
                Serial.printf("  pari: GPIO%u(%s) <-> GPIO%u(%s)\n", PINS[i], HOLES[i], PINS[j], HOLES[j]);
            }
    for (uint8_t i = 0; i < NP; i++) {
        if (scan.stuckHigh[i]) {
            nHigh++;
            Serial.printf("HUOM: vedetty ylos (ulkoinen pull-up tai jannite; ei yksin vika): GPIO%u(%s)\n",
                          PINS[i], HOLES[i]);
        }
        if (scan.stuckLow[i]) {
            any = true;
            Serial.printf("KIINNI GND:SSA (tai nappi painettu): GPIO%u(%s)\n", PINS[i], HOLES[i]);
        }
    }

    if (nHigh > 0)
        Serial.println("Vihje: pull-up-vastus ylosvetaa pinnin ilman etta se on oikosulku. pin_config.h:n mukaan\n"
                       "GPIO17 on I2C SCL ja GPIO18 I2C SDA, joten niilla voi olla pull-upit levyllä (ei varmistettu).\n"
                       "Oikosulku nakyy vain 'OIKOSULKU'-riveina.");
    if (any) Serial.println("TULOS: LOYTYI ONGELMIA (ks. yllä)");
    else     Serial.println("TULOS: EI OIKOSULKUJA");
    Serial.println("Naytto nayttaa nyt syotteiden tilat (IN-rivit tulevat tahan). Uusi testi: BOOT-nappi.");
}

static bool reportPending = false;
static unsigned long reportDeadline = 0;

// Ajaa oikosulkutestin kerran. Konsoliraportti tulostetaan loop():ssa heti kun sarjamonitori on
// auki (tai 10 s kuluttua), jotta näyttö ei blokkaannu odotuksen ajaksi.
static void runTest() {
    scan.run(hal, NP);
    buildLines();
    reportPending = true;
    reportDeadline = millis() + 10000;
    startLive();
}

// ---------- Arduino ----------

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

    pinMode(PIN_BUTTON_1, INPUT_PULLUP);   // GPIO0, BOOT

    runTest();                             // kerran käynnistyksessä
}

void loop() {
    if (reportPending && (Serial || millis() > reportDeadline)) {
        delay(300);                        // anna monitorin asettua
        printReport();
        reportPending = false;
    }

    static bool prevBoot = true;
    bool boot = digitalRead(PIN_BUTTON_1);
    if (prevBoot && !boot) {               // BOOT: uusi yksittäinen oikosulkutesti
        delay(30);
        runTest();
    }
    prevBoot = boot;

    liveUpdate();
    delay(5);
}
