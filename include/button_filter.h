#pragma once
#include <stdint.h>

// Nappisuodatin: palauttaa jokaisesta fyysisestä painalluksesta enintään yhden tapahtuman.
// Ei riippuvuuksia Arduinoon, joten sen voi testata koneella (tools/host_tests).
//
// Jokainen nappi on itsenäinen. Edellinen nappi saa olla vielä alhaalla, kun seuraava painetaan
// (päällekkäinen pito, "rollover"), ja seuraava painallus lasketaan normaalisti.
//
//  1) Pompotus ja piikit: painallus hyväksytään vasta, kun raaka signaali on ollut painettu
//     pressMs yhtäjaksoisesti. Vapautus hyväksytään vasta, kun signaali on ollut vapaana
//     releaseMs yhtäjaksoisesti. Pompotus ei voi tuottaa uutta painallusta, koska sen LOW-jaksot
//     ovat lyhyempiä kuin pressMs.
//  2) Saman napin uusi painallus aiemmin kuin repressMs edellisestä hyväksytystä hylätään
//     (ihminen ei paina samaa nappia yli ~14 kertaa sekunnissa).
//  3) Yhtäaikaisuus: jos kahden eri napin raaka signaali alkoi alle simulMs sisällä toisistaan,
//     ne ovat yhtäaikaiset. Silloin kumpaakaan ei hyväksytä, ja `simultaneous` kertoo ne.
//     Päätös tehdään napin todellisesta alkuhetkestä (pressOnset), ei vahvistushetkestä eikä
//     napin indeksistä. Jos toinen on vasta alkamassa (LOW mutta ei vielä vahvistunut), ensimmäisen
//     hyväksyntää viivytetään kunnes tiedetään, vahvistuuko myös toinen vai oliko se piikki.
//
// Hylätty tai yhtäaikainen painallus kuluttaa silti painalluksen: nappi pitää vapauttaa ennen
// seuraavaa. update() voi palauttaa useita bittejä, jos eri napit vahvistuvat samalla kutsulla;
// käsittele ne pressOnset-järjestyksessä.

class ButtonFilter {
public:
    enum { MAX_BUTTONS = 8, GLITCH_MIN_MS = 3 };

    uint16_t pressMs;     // vakaa painallus vähintään näin kauan
    uint16_t releaseMs;   // vakaa vapautus vähintään näin kauan
    uint16_t repressMs;   // saman napin kahden hyväksytyn painalluksen vähimmäisväli
    uint16_t simulMs;     // eri nappien alkuhetkien ero, jota lyhyempi on "yhtäaikainen" (<= pressMs)

    // Diagnostiikka. Nollataan jokaisen update()-kutsun alussa, bitti i = nappi i.
    uint8_t rejectedSame;  // hylätty: sama nappi liian pian edellisestä (repressMs)
    uint8_t simultaneous;  // yhtäaikaisiksi tunnistetut napit (ei hyväksytty)
    uint8_t glitch;        // liian lyhyt painallus (GLITCH_MIN_MS .. pressMs), ei tapahtumaa
    uint16_t glitchLen[MAX_BUTTONS];  // piikin kesto ms
    // Hetki (kellon arvo), jolloin napin raaka signaali meni painetuksi. Päivittyy kun painallus
    // vahvistuu (hyväksyttynä, hylättynä tai yhtäaikaisena).
    uint32_t pressOnset[MAX_BUTTONS];

    ButtonFilter()
        : pressMs(15), releaseMs(8), repressMs(70), simulMs(12),
          rejectedSame(0), simultaneous(0), glitch(0), n(0) {
        for (uint8_t i = 0; i < MAX_BUTTONS; i++) { glitchLen[i] = 0; pressOnset[i] = 0; }
    }

    // pressedNow[i] == true, kun nappi i on painettuna (raaka luku).
    // Painettuna alussa oleva nappi ei tuota tapahtumaa ennen vapautusta ja uutta painallusta.
    void begin(uint8_t count, const bool *pressedNow, uint32_t now) {
        n = count > (uint8_t)MAX_BUTTONS ? (uint8_t)MAX_BUTTONS : count;
        if (simulMs > pressMs) simulMs = pressMs;
        for (uint8_t i = 0; i < n; i++) {
            raw[i] = stable[i] = pressedNow[i];
            since[i] = now;
            hasAcc[i] = false;
            lastAcc[i] = 0;
        }
    }

    // Kutsu usein (vähintään ~1 ms välein). Palauttaa bittimaskin hyväksytyistä painalluksista.
    uint8_t update(const bool *pressedNow, uint32_t now) {
        rejectedSame = simultaneous = glitch = 0;
        uint8_t elig = 0;                       // vahvistumassa tällä kutsulla

        for (uint8_t i = 0; i < n; i++) {
            if (raw[i] != pressedNow[i]) {
                if (raw[i] && !stable[i]) {     // oli painettuna mutta ei ehtinyt vahvistua
                    uint32_t len = now - since[i];
                    if (len >= GLITCH_MIN_MS) {
                        glitch |= (uint8_t)(1u << i);
                        glitchLen[i] = (uint16_t)len;
                    }
                }
                raw[i] = pressedNow[i];
                since[i] = now;
            }
            uint32_t steady = now - since[i];
            if (stable[i]) {
                if (!raw[i] && steady >= releaseMs) stable[i] = false;
            } else if (raw[i] && steady >= pressMs) {
                elig |= (uint8_t)(1u << i);
            }
        }

        uint8_t mask = 0, done = 0;
        for (uint8_t i = 0; i < n; i++) {
            uint8_t bi = (uint8_t)(1u << i);
            if (!(elig & bi) || (done & bi)) continue;

            // Ketkä napit alkoivat lähes samaan aikaan kuin i?
            uint8_t group = bi;
            bool pending = false;
            for (uint8_t j = 0; j < n; j++) {
                if (j == i || !raw[j] || absDiff(since[j], since[i]) >= simulMs) continue;
                if (elig & (1u << j)) group |= (uint8_t)(1u << j);
                else if (!stable[j]) pending = true;   // alkanut, ei vielä vahvistunut
            }
            if (pending) continue;              // odota selviääkö toisen napin kohtalo

            for (uint8_t j = 0; j < n; j++) {
                if (group & (1u << j)) {
                    stable[j] = true;
                    pressOnset[j] = since[j];
                    done |= (uint8_t)(1u << j);
                }
            }
            if (group & (group - 1)) {          // useampi kuin yksi bitti
                simultaneous |= group;
                continue;
            }
            bool okSame = !hasAcc[i] || (now - lastAcc[i]) >= repressMs;
            if (okSame) {
                mask |= bi;
                lastAcc[i] = now;
                hasAcc[i] = true;
            } else {
                rejectedSame |= bi;
            }
        }
        return mask;
    }

private:
    static uint32_t absDiff(uint32_t a, uint32_t b) {
        int32_t d = (int32_t)(a - b);
        return (uint32_t)(d < 0 ? -d : d);
    }

    uint8_t n;
    bool raw[MAX_BUTTONS];
    bool stable[MAX_BUTTONS];
    bool hasAcc[MAX_BUTTONS];
    uint32_t since[MAX_BUTTONS];
    uint32_t lastAcc[MAX_BUTTONS];
};
