#pragma once
#include <stdint.h>

// Oikosulkuanalyysi pinniryhmälle. Ei riippuvuuksia Arduinoon (testattavissa koneella,
// ks. tools/host_tests/test_shortscan.cpp). Laitetaso annetaan ShortScanHal:n kautta.
//
// Menetelmä:
//  1) Kaikki pinnit tuloiksi sisäisellä pull-downilla. Jos pinni lukee HIGH, sitä vetää ylös jokin
//     ulkoinen: kova jännite (3V3/5V) tai ulkoinen pull-up-vastus (esim. I2C-väylän pull-upit)
//     -> stuckHigh. Näitä kahta ei voi erottaa toisistaan tällä menetelmällä.
//  2) Yksi pinni kerrallaan ajetaan heikosti HIGH:iin ja luetaan kaikki muut pinnit.
//     - Ajettu pinni lukee itse LOW -> se on kiinni GND:ssä (tai painettu nappi) -> stuckLow.
//     - Muu pinni lukee HIGH -> se on sähköisesti kiinni ajetussa pinnissä -> pair.
//     (ULN-tulot lukevat pull-downilla LOW, joten ne eivät aiheuta vääriä hälytyksiä, ja
//     heikko ajo on turvallinen myös painetun napin kohdalla.)
//  3) Ylösvedetyt pinnit (stuckHigh) eivät näy vaiheessa 2 toistensa pareina, koska ne lukevat
//     HIGH joka tapauksessa. Siksi jokainen ylösvedetty pinni ajetaan heikosti LOW:iin ja luetaan
//     muut ylösvedetyt: pinni, joka seuraa LOW:iin, on oikosulussa ajetun kanssa. Kova jännite
//     voittaa heikon ajon, joten silloin paria ei voi todeta (vain stuckHigh).
//
// Painettua nappia ei voi erottaa oikosulusta GND:hen, joten testin aikana ei saa painaa nappeja.

class ShortScanHal {
public:
    virtual ~ShortScanHal() {}
    virtual void pulldown(uint8_t i) = 0;   // pinni i tuloksi, sisäinen pull-down, normaali ajovoima
    virtual void driveHigh(uint8_t i) = 0;  // pinni i ulostuloksi HIGH, heikoin ajovoima, itseluku sallittu
    virtual void driveLow(uint8_t i) = 0;   // pinni i ulostuloksi LOW, heikoin ajovoima
    virtual bool read(uint8_t i) = 0;       // pinnin i taso
    virtual void settle() = 0;              // odota tasojen asettumista
};

struct ShortScan {
    enum { MAXP = 16 };

    uint8_t n;
    bool pair[MAXP][MAXP];   // pair[i][j]: pinnit i ja j ovat sähköisesti yhdessä (symmetrinen)
    bool stuckHigh[MAXP];    // kiinni jännitteessä
    bool stuckLow[MAXP];     // kiinni GND:ssä (tai painettu nappi)

    ShortScan() : n(0) { clear(); }

    void clear() {
        for (uint8_t i = 0; i < MAXP; i++) {
            stuckHigh[i] = stuckLow[i] = false;
            for (uint8_t j = 0; j < MAXP; j++) pair[i][j] = false;
        }
    }

    void run(ShortScanHal &h, uint8_t count) {
        n = count > (uint8_t)MAXP ? (uint8_t)MAXP : count;
        clear();
        for (uint8_t i = 0; i < n; i++) h.pulldown(i);
        h.settle();
        for (uint8_t i = 0; i < n; i++) stuckHigh[i] = h.read(i);

        for (uint8_t i = 0; i < n; i++) {
            h.driveHigh(i);
            h.settle();
            stuckLow[i] = !h.read(i);
            if (!stuckLow[i]) {
                for (uint8_t j = 0; j < n; j++) {
                    if (j == i || stuckHigh[j]) continue;
                    if (h.read(j)) pair[i][j] = pair[j][i] = true;
                }
            }
            h.pulldown(i);
            h.settle();
        }

        // Vaihe 3: ylösvedetyt pinnit pareittain ajamalla LOW:iin
        for (uint8_t i = 0; i < n; i++) {
            if (!stuckHigh[i]) continue;
            h.driveLow(i);
            h.settle();
            for (uint8_t j = 0; j < n; j++) {
                if (j == i || !stuckHigh[j]) continue;
                if (!h.read(j)) pair[i][j] = pair[j][i] = true;
            }
            h.pulldown(i);
            h.settle();
        }
    }

    bool anyProblem() const {
        for (uint8_t i = 0; i < n; i++) {
            if (stuckHigh[i] || stuckLow[i]) return true;
            for (uint8_t j = 0; j < n; j++) if (pair[i][j]) return true;
        }
        return false;
    }

    // Jakaa pinnit yhteenkuuluviin ryhmiin. label[i] = ryhmän pienin pinni-indeksi.
    // Ryhmä on oikosulku, jos siinä on vähintään kaksi jäsentä.
    void groups(uint8_t *label) const {
        for (uint8_t i = 0; i < n; i++) label[i] = i;
        for (uint8_t round = 0; round < n; round++)
            for (uint8_t i = 0; i < n; i++)
                for (uint8_t j = 0; j < n; j++)
                    if (pair[i][j] && label[j] < label[i]) label[i] = label[j];
    }
};
