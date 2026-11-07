// Koneella ajettavat testit oikosulkuanalyysille.
//
//   g++ -std=c++11 -Wall -Wextra -Iinclude tools/host_tests/test_shortscan.cpp -o /tmp/test_shortscan && /tmp/test_shortscan

#include <cstdio>
#include <cstdint>
#include <vector>
#include "short_scan.h"

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { fails++; \
    std::printf("FAIL (rivi %d): %s\n", __LINE__, msg); } } while (0)

// Simuloitu pinniverkko: pinnit samassa verkossa ovat sähköisesti yhdessä.
// Verkko voi olla kiinni jännitteessä (vcc, kova), GND:ssä (gnd, kova) tai ulkoisessa
// pull-upissa (pullup, vastus). Kova jännite voittaa heikon ajon. Muuten: ajettu HIGH -> HIGH,
// ajettu LOW -> LOW, ilman ajoa pullup -> HIGH, muuten sisäinen pull-down -> LOW
// (kuten ULN-tulo ja avoin nappi).
struct FakeBoard : ShortScanHal {
    int net[ShortScan::MAXP];
    bool vcc[ShortScan::MAXP], gnd[ShortScan::MAXP], pullup[ShortScan::MAXP];  // verkon id:llä
    bool driven[ShortScan::MAXP], drivenLow[ShortScan::MAXP];
    int pulldownCalls, driveCalls;

    FakeBoard() : pulldownCalls(0), driveCalls(0) {
        for (int i = 0; i < ShortScan::MAXP; i++) {
            net[i] = i; vcc[i] = gnd[i] = pullup[i] = driven[i] = drivenLow[i] = false;
        }
    }
    void join(int a, int b) {                 // yhdistä pinnit a ja b samaan verkkoon
        int from = net[b], to = net[a];
        for (int i = 0; i < ShortScan::MAXP; i++) if (net[i] == from) net[i] = to;
        vcc[to] = vcc[to] || vcc[from]; gnd[to] = gnd[to] || gnd[from];
        pullup[to] = pullup[to] || pullup[from];
    }
    void pulldown(uint8_t i) { driven[i] = drivenLow[i] = false; pulldownCalls++; }
    void driveHigh(uint8_t i) { driven[i] = true; drivenLow[i] = false; driveCalls++; }
    void driveLow(uint8_t i) { drivenLow[i] = true; driven[i] = false; }
    void settle() {}
    bool read(uint8_t i) {
        int id = net[i];
        if (gnd[id]) return false;
        if (vcc[id]) return true;
        for (int k = 0; k < ShortScan::MAXP; k++) if (net[k] == id && driven[k]) return true;
        for (int k = 0; k < ShortScan::MAXP; k++) if (net[k] == id && drivenLow[k]) return false;
        return pullup[id];
    }
};

static int pairCount(const ShortScan &s) {
    int c = 0;
    for (int i = 0; i < s.n; i++) for (int j = i + 1; j < s.n; j++) if (s.pair[i][j]) c++;
    return c;
}

static void testScan() {
    // 1. ei oikosulkuja
    {
        FakeBoard b; ShortScan s; s.run(b, 13);
        CHECK(!s.anyProblem(), "puhdas levy: ei ongelmia");
        CHECK(b.pulldownCalls > 0 && b.driveCalls == 13, "jokainen pinni ajettiin kerran");
    }

    // 2. kaksi pinniä oikosulussa (esim. GPIO17 ja GPIO18 = indeksit 2 ja 3)
    {
        FakeBoard b; b.join(2, 3); ShortScan s; s.run(b, 13);
        CHECK(s.pair[2][3] && s.pair[3][2], "pari 2-3 löytyy symmetrisesti");
        CHECK(pairCount(s) == 1, "vain yksi pari");
        bool others = false;
        for (int i = 0; i < 13; i++) if (s.stuckHigh[i] || s.stuckLow[i]) others = true;
        CHECK(!others, "ei GND/jännite-ilmoituksia");
        uint8_t label[ShortScan::MAXP]; s.groups(label);
        CHECK(label[2] == 2 && label[3] == 2 && label[0] == 0, "ryhmät oikein");
    }

    // 3. ketju kolmesta pinnistä
    {
        FakeBoard b; b.join(1, 2); b.join(2, 4); ShortScan s; s.run(b, 13);
        CHECK(pairCount(s) == 3, "kolmen ketju = kolme paria");
        uint8_t label[ShortScan::MAXP]; s.groups(label);
        CHECK(label[1] == 1 && label[2] == 1 && label[4] == 1, "yksi ryhmä");
    }

    // 4. kaksi erillistä oikosulkua
    {
        FakeBoard b; b.join(0, 5); b.join(7, 8); ShortScan s; s.run(b, 13);
        CHECK(pairCount(s) == 2 && s.pair[0][5] && s.pair[7][8], "kaksi erillistä paria");
        CHECK(!s.pair[0][7] && !s.pair[5][8], "ei ristiin");
        uint8_t label[ShortScan::MAXP]; s.groups(label);
        CHECK(label[5] == 0 && label[8] == 7, "kaksi ryhmää");
    }

    // 5. pinni kiinni GND:ssä (esim. painettu nappi tai oikosulku)
    {
        FakeBoard b; b.gnd[6] = true; ShortScan s; s.run(b, 13);
        CHECK(s.stuckLow[6], "GND-oikosulku havaitaan");
        CHECK(!s.stuckHigh[6] && pairCount(s) == 0, "ei vääriä pareja");
        int lows = 0; for (int i = 0; i < 13; i++) lows += s.stuckLow[i];
        CHECK(lows == 1, "vain yksi pinni GND:ssä");
    }

    // 6. pinni kiinni jännitteessä
    {
        FakeBoard b; b.vcc[9] = true; ShortScan s; s.run(b, 13);
        CHECK(s.stuckHigh[9], "jänniteoikosulku havaitaan");
        CHECK(pairCount(s) == 0, "ei vääriä pareja jännitteen takia");
        int highs = 0; for (int i = 0; i < 13; i++) highs += s.stuckHigh[i];
        CHECK(highs == 1, "vain yksi pinni jännitteessä");
    }

    // 7. kaksi pinniä yhdessä ja GND:ssä: molemmat stuckLow, ei paria (ei voi erottaa)
    {
        FakeBoard b; b.join(2, 3); b.gnd[b.net[2]] = true; ShortScan s; s.run(b, 13);
        CHECK(s.stuckLow[2] && s.stuckLow[3], "molemmat GND:ssä");
    }

    // 8. kaksi pinniä yhdessä ja jännitteessä: molemmat stuckHigh, ei vääriä pareja muille
    {
        FakeBoard b; b.join(4, 5); b.vcc[b.net[4]] = true; ShortScan s; s.run(b, 13);
        CHECK(s.stuckHigh[4] && s.stuckHigh[5], "molemmat jännitteessä");
        CHECK(pairCount(s) == 0, "ei pareja");
    }

    // 9. satunnaiset ryhmitykset: löydetyt parit = täsmälleen sama verkko
    {
        uint32_t seed = 4242;
        bool ok = true;
        for (int trial = 0; trial < 300 && ok; trial++) {
            FakeBoard b;
            int joins = trial % 6;
            std::vector<std::pair<int,int> > js;
            for (int k = 0; k < joins; k++) {
                seed = seed * 1664525u + 1013904223u; int a = (seed >> 8) % 13;
                seed = seed * 1664525u + 1013904223u; int c = (seed >> 8) % 13;
                if (a != c) { b.join(a, c); js.push_back(std::make_pair(a, c)); }
            }
            ShortScan s; s.run(b, 13);
            for (int i = 0; i < 13 && ok; i++)
                for (int j = 0; j < 13 && ok; j++)
                    if (i != j) ok = (s.pair[i][j] == (b.net[i] == b.net[j]));
            for (int i = 0; i < 13 && ok; i++) ok = !s.stuckHigh[i] && !s.stuckLow[i];
        }
        CHECK(ok, "300 satunnaista verkkoa: parit täsmäävät verkkoihin");
    }

    // 11. kaksi erillistä ulkoista pull-upia (kuten I2C SCL/SDA): ylösvedetty, ei paria
    {
        FakeBoard b; b.pullup[2] = true; b.pullup[3] = true; ShortScan s; s.run(b, 13);
        CHECK(s.stuckHigh[2] && s.stuckHigh[3], "pull-upit näkyvät ylösvedettyinä");
        CHECK(pairCount(s) == 0, "erilliset pull-upit eivät ole pari");
        CHECK(!s.stuckLow[2] && !s.stuckLow[3], "ei GND-ilmoituksia");
    }

    // 12. kaksi pull-upia JA oikosulku niiden välillä: pari löytyy, vaikka molemmat lukevat HIGH
    {
        FakeBoard b; b.join(2, 3); b.pullup[b.net[2]] = true; ShortScan s; s.run(b, 13);
        CHECK(s.stuckHigh[2] && s.stuckHigh[3], "molemmat ylösvedettyjä");
        CHECK(s.pair[2][3] && s.pair[3][2], "oikosulku löytyy ylösvedettyjen välillä");
        CHECK(pairCount(s) == 1, "vain yksi pari");
    }

    // 13. kolme pull-upia, vain kaksi niistä oikosulussa
    {
        FakeBoard b; b.join(2, 3); b.pullup[b.net[2]] = true; b.pullup[b.net[4]] = true;
        ShortScan s; s.run(b, 13);
        CHECK(s.pair[2][3] && !s.pair[2][4] && !s.pair[3][4], "vain oikea pari");
    }

    // 14. ylösvedetty pinni oikosulussa tavalliseen pinniin (ei pull-upia): pari löytyy
    {
        FakeBoard b; b.join(2, 5); b.pullup[b.net[2]] = true; ShortScan s; s.run(b, 13);
        CHECK(s.stuckHigh[2] && s.stuckHigh[5], "pinni 5 näkyy ylösvedettynä pinnin 2 kautta");
        CHECK(s.pair[2][5], "pari 2-5 löytyy");
    }

    // 10. enimmäiskoko: yli MAXP pinniä ei kaada
    {
        FakeBoard b; ShortScan s; s.run(b, 40);
        CHECK(s.n == ShortScan::MAXP, "n rajataan MAXP:iin");
    }
}

int main() {
    testScan();
    std::printf("%d tarkistusta, %d epäonnistui\n", checks, fails);
    return fails ? 1 : 0;
}
