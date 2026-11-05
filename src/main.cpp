#include "Arduino.h"
#include "TFT_eSPI.h"
#include "pin_config.h"

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

#define LED_PIN  1
#define BTN_PIN  43

void showMsg(const char *msg, uint16_t color) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawCentreString(msg, 160, 65, 4);
}

void showResult(unsigned long ms) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString("Reaktioaika:", 160, 20, 4);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu ms", ms);
    tft.setTextSize(2);
    tft.drawCentreString(buf, 160, 60, 7);
    tft.setTextSize(1);
    tft.setTextColor(0x4A49, TFT_BLACK);
    tft.drawCentreString("paina uudelleen", 160, 148, 2);
}

void setup() {
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    tft.begin();
    for (uint8_t i = 0; i < sizeof(lcd_st7789v)/sizeof(lcd_cmd_t); i++) {
        tft.writecommand(lcd_st7789v[i].cmd);
        for (int j = 0; j < (lcd_st7789v[i].len & 0x7f); j++)
            tft.writedata(lcd_st7789v[i].data[j]);
        if (lcd_st7789v[i].len & 0x80) delay(120);
    }
    tft.setRotation(3);
    tft.setSwapBytes(true);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    pinMode(BTN_PIN, INPUT_PULLUP);

    ledcSetup(0, 5000, 8);
    ledcAttachPin(PIN_LCD_BL, 0);
    ledcWrite(0, 200);

    randomSeed(esp_random());
    showMsg("Odota...", TFT_WHITE);
}

enum State { WAITING, LIT, RESULT };
State state = WAITING;
unsigned long litAt = 0;

void loop() {
    switch (state) {
        case WAITING: {
            unsigned long wait = random(1000, 4000);
            unsigned long start = millis();
            while (millis() - start < wait) {
                if (digitalRead(BTN_PIN) == LOW) {
                    showMsg("VARASLAHTÖ!", TFT_RED);
                    state = RESULT;
                    delay(300);
                    return;
                }
            }
            digitalWrite(LED_PIN, HIGH);
            showMsg("NYT!", TFT_YELLOW);
            litAt = millis();
            state = LIT;
            break;
        }

        case LIT:
            if (digitalRead(BTN_PIN) == LOW) {
                unsigned long ms = millis() - litAt;
                digitalWrite(LED_PIN, LOW);
                showResult(ms);
                state = RESULT;
                delay(300);
            }
            break;

        case RESULT:
            if (digitalRead(BTN_PIN) == LOW) {
                showMsg("Odota...", TFT_WHITE);
                state = WAITING;
                delay(300);
            }
            break;
    }
}
