// Koneella ajettavat testit nappisuodattimelle ja pelin jonolle.
//
//   g++ -std=c++11 -Wall -Wextra -Iinclude tools/host_tests/test_logic.cpp -o /tmp/test_logic && /tmp/test_logic

#include <cstdio>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>
#include "button_filter.h"
#include "game_core.h"

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { fails++; \
    std::printf("FAIL (rivi %d): %s\n", __LINE__, msg); } } while (0)

// ---------- signaalien rakennus ----------

typedef std::function<bool(int)> Signal;   // ms -> painettu?
typedef std::vector<std::pair<int,int> > Events;   // (aika, nappi)

// Suodattimen diagnostiikkalaskurit viimeisestä run()-kutsusta.
static int g_rejSame = 0, g_sim = 0, g_glitch = 0;

static bool chatter(int t, int salt) {     // deterministinen "satunnainen" bitti per ms
    uint32_t x = (uint32_t)t * 2654435761u ^ (uint32_t)salt * 40503u;
    x ^= x >> 13; x *= 1274126177u; x ^= x >> 16;
    return x & 1;
}

// Painallus [a, b) jossa kontaktit sätkivät bounce ms alussa ja lopussa.
static Signal press(int a, int b, int bounce, int salt = 1) {
    return [=](int t) {
        if (t < a || t >= b + bounce) return false;
        if (t < a + bounce) return chatter(t, salt);
        if (t < b) return true;
        return chatter(t, salt + 7);
    };
}

static Signal anyOf(std::vector<Signal> s) {
    return [=](int t) { for (size_t i = 0; i < s.size(); i++) if (s[i](t)) return true; return false; };
}

// Ajaa suodatinta 1 ms askelin. Palauttaa (aika, nappi) -tapahtumat.
static Events run(std::vector<Signal> sigs, int durationMs, ButtonFilter *fp = 0) {
    ButtonFilter local;
    ButtonFilter &f = fp ? *fp : local;
    bool now[ButtonFilter::MAX_BUTTONS] = {false};
    for (size_t i = 0; i < sigs.size(); i++) now[i] = sigs[i](0);
    f.begin((uint8_t)sigs.size(), now, 0);
    Events ev;
    g_rejSame = g_sim = g_glitch = 0;
    for (int t = 1; t <= durationMs; t++) {
        for (size_t i = 0; i < sigs.size(); i++) now[i] = sigs[i](t);
        uint8_t m = f.update(now, (uint32_t)t);
        g_rejSame += __builtin_popcount(f.rejectedSame);
        g_sim += f.simultaneous ? 1 : 0;
        g_glitch += __builtin_popcount(f.glitch);
        for (size_t i = 0; i < sigs.size(); i++)
            if (m & (1u << i)) ev.push_back(std::make_pair(t, (int)i));
    }
    return ev;
}

// ---------- nappisuodatin ----------

static void testFilter() {
    // 1. siisti painallus -> tasan yksi
    CHECK(run({press(100, 300, 0)}, 600).size() == 1, "siisti painallus = 1 tapahtuma");

    // 2. ei painallusta -> ei tapahtumia
    CHECK(run({press(0, 0, 0)}, 300).empty(), "ei painallusta = 0 tapahtumaa");

    // 3. pompotus alussa ja lopussa (12 ms) -> yksi
    for (int salt = 1; salt <= 50; salt++)
        CHECK(run({press(100, 300, 12, salt)}, 600).size() == 1, "pompotus 12 ms: tasan yksi");

    // 4. raju sätkintä (20 ms) alussa ja lopussa -> yksi
    for (int salt = 1; salt <= 50; salt++)
        CHECK(run({press(100, 400, 20, salt)}, 800).size() == 1, "sätkintä 20 ms: tasan yksi");

    // 5. lyhyt piikki -> ei mitään
    CHECK(run({press(100, 105, 0)}, 400).empty(), "5 ms piikki ei ole painallus");
    CHECK(run({press(100, 114, 0)}, 400).empty(), "14 ms piikki ei ole painallus");

    // 6. toistuva piikkijono (häiriö) -> ei mitään
    {
        Signal noise = [](int t) { return (t % 20) < 6; };   // 6 ms piikki 20 ms välein
        CHECK(run({noise}, 1000).empty(), "toistuvat 6 ms piikit eivät laukaise");
    }

    // 7. kaksi painallusta samalla napilla, selvä väli -> kaksi
    CHECK(run({anyOf({press(20, 60, 0), press(180, 220, 0)})}, 600).size() == 2,
          "kaksi selvästi erillistä painallusta = 2");

    // 8. epäinhimillisen nopea toisto (jakso 50 ms) -> yksi
    CHECK(run({anyOf({press(10, 30, 0), press(60, 80, 0)})}, 400).size() == 1,
          "50 ms jaksolla toistuva painallus = 1");

    // 9. 40 sätkivää painallusta 200 ms välein -> tasan 40
    {
        std::vector<Signal> s;
        for (int k = 0; k < 40; k++) s.push_back(press(100 + k * 200, 150 + k * 200, 10, k + 1));
        CHECK(run({anyOf(s)}, 100 + 40 * 200 + 300).size() == 40,
              "40 sätkivää painallusta -> tasan 40 tapahtumaa");
    }

    // 10. kaksi nappia yhtä aikaa -> ei hyväksytä kumpaakaan, raportoidaan yhtäaikaisena
    for (int salt = 1; salt <= 20; salt++) {
        Events ev = run({press(100, 300, 5, salt), press(100, 300, 5, salt + 100)}, 600);
        CHECK(ev.empty(), "kaksi nappia yhtä aikaa: ei hyväksyttyjä");
        CHECK(g_sim == 1, "yhtäaikaisuus raportoidaan kerran");
    }

    // 11. kaksi eri nappia 100 ms välein -> kaksi, oikeassa järjestyksessä
    {
        Events ev = run({press(100, 250, 0), press(200, 350, 0)}, 600);
        CHECK(ev.size() == 2, "kaksi eri nappia 100 ms välein = 2");
        if (ev.size() == 2) CHECK(ev[0].second == 0 && ev[1].second == 1, "järjestys säilyy");
    }

    // 12. alussa painettuna oleva nappi ei laukea ennen vapautusta + uutta painallusta
    CHECK(run({anyOf({press(-1000, 200, 0), press(400, 500, 0)})}, 700).size() == 1,
          "alussa painettuna: vain uusi painallus lasketaan");

    // 13. pitkä pito -> yksi tapahtuma
    CHECK(run({press(100, 5000, 0)}, 5500).size() == 1, "pitkä pito = 1");

    // 14. aikalaskurin ylivuoto (uint32) ei riko
    {
        ButtonFilter f;
        bool p[1] = {false};
        uint32_t t0 = 0xFFFFFFF0u;
        f.begin(1, p, t0);
        int events = 0;
        for (uint32_t k = 0; k < 400; k++) {
            p[0] = (k >= 50 && k < 200);
            if (f.update(p, t0 + k)) events++;
        }
        CHECK(events == 1, "uint32-ylivuoto: yksi tapahtuma");
    }

    // 15. diagnostiikka: liian nopea toisto samasta napista raportoidaan hylättynä
    {
        Events ev = run({anyOf({press(10, 40, 0), press(70, 100, 0)})}, 400);
        CHECK(ev.size() == 1, "60 ms jaksolla toinen painallus hylätään");
        CHECK(g_rejSame == 1 && g_sim == 0, "hylkäys raportoidaan (sama nappi)");
    }

    // 16. diagnostiikka: piikit raportoidaan, tavallinen pompotus ei
    {
        Events ev = run({press(100, 108, 0)}, 400);
        CHECK(ev.empty() && g_glitch == 1, "8 ms piikki raportoidaan");
        ev = run({press(100, 102, 0)}, 400);
        CHECK(ev.empty() && g_glitch == 0, "2 ms pompotus ei ole raportoitava piikki");
        ev = run({press(100, 300, 0)}, 600);
        CHECK(ev.size() == 1 && g_glitch == 0 && g_rejSame == 0 && g_sim == 0,
              "siisti painallus ei tuota diagnostiikkaa");
    }

    // 17. pressOnset kertoo raa'an signaalin alkuhetken, ei vahvistushetkeä
    {
        ButtonFilter f;
        Events ev = run({press(100, 300, 0), press(104, 300, 0)}, 600, &f);
        CHECK(ev.empty() && g_sim == 1, "104 ms vs 100 ms alku = yhtäaikainen");
        CHECK(f.pressOnset[0] == 100 && f.pressOnset[1] == 104, "alkuhetket 100 ja 104 ms");
    }
}

// ---------- päällekkäin pidetyt napit (rollover) ----------

static void testRollover() {
    // A pidetään alhaalla ja B painetaan 30 ms myöhemmin: molemmat lasketaan.
    {
        Events ev = run({press(100, 400, 0), press(130, 400, 0)}, 700);
        CHECK(ev.size() == 2, "B painettu 30 ms A:n jälkeen, A vielä alhaalla: kaksi");
        if (ev.size() == 2) CHECK(ev[0].second == 0 && ev[1].second == 1, "järjestys A, B");
        CHECK(g_sim == 0 && g_rejSame == 0, "ei hylkäyksiä");
    }
    // Käänteinen indeksi: korkeampi nappi ensin, matalampi perään
    {
        Events ev = run({press(130, 400, 0), press(100, 400, 0)}, 700);
        CHECK(ev.size() == 2, "kaksi tapahtumaa");
        if (ev.size() == 2) CHECK(ev[0].second == 1 && ev[1].second == 0,
                                  "alkuhetki ratkaisee, ei indeksi (nappi 2 ennen nappia 1)");
    }
    // Alkuhetkien ero juuri yli rajan (13 ms) on vielä kaksi erillistä painallusta
    {
        Events ev = run({press(100, 400, 0), press(113, 400, 0)}, 700);
        CHECK(ev.size() == 2 && g_sim == 0, "13 ms ero = kaksi painallusta");
    }
    // Alle rajan (11 ms) on yhtäaikainen
    {
        Events ev = run({press(100, 400, 0), press(111, 400, 0)}, 700);
        CHECK(ev.empty() && g_sim == 1, "11 ms ero = yhtäaikainen");
    }
    // A vapautetaan vasta B:n painamisen jälkeen
    {
        Events ev = run({press(100, 200, 0), press(150, 300, 0)}, 600);
        CHECK(ev.size() == 2, "A vapautuu B:n painamisen jälkeen: kaksi");
    }
    // Toinen nappi on vasta alkamassa mutta oli piikki: ensimmäinen hyväksytään viiveellä
    {
        Events ev = run({press(100, 400, 0), press(105, 118, 0)}, 700);
        CHECK(ev.size() == 1 && ev[0].second == 0 && g_sim == 0,
              "13 ms piikki kakkosnapissa ei estä ykkösnapin painallusta");
        CHECK(g_glitch == 1, "piikki raportoitu");
    }
    // Saman napin nopea tuplapainallus kun edellinen "näyttää vielä painetulta":
    // väli vapautuksessa 12 ms (> releaseMs), jakso 100 ms (> repressMs)
    {
        Events ev = run({anyOf({press(100, 150, 0), press(200, 250, 0)})}, 600);
        CHECK(ev.size() == 2, "saman napin tuplapainallus 100 ms jaksolla: kaksi");
    }

    // Satunnainen rollover: 40 painallusta, napit pidetään päällekkäin, alkuhetkien väli
    // vähintään 40 ms, sätkintä 8 ms. Kaikkien pitää tulla läpi täsmälleen oikeassa järjestyksessä.
    uint32_t s = 987654321u;
    struct P { int btn, start, end; };
    for (int trial = 0; trial < 30; trial++) {
        std::vector<P> ps;
        int lastRel[4] = {-1000, -1000, -1000, -1000};
        int t = 100;
        for (int k = 0; k < 40; k++) {
            s = s * 1664525u + 1013904223u; t += 40 + (int)((s >> 8) % 121);
            s = s * 1664525u + 1013904223u; int b = (int)((s >> 8) % 4);
            int start = t > lastRel[b] + 40 ? t : lastRel[b] + 40;
            t = start;
            s = s * 1664525u + 1013904223u; int end = start + 50 + (int)((s >> 8) % 201);
            lastRel[b] = end + 10;
            P p = {b, start, end};
            ps.push_back(p);
        }
        std::vector<Signal> sigs;
        for (int b = 0; b < 4; b++) {
            std::vector<Signal> mine;
            for (size_t k = 0; k < ps.size(); k++)
                if (ps[k].btn == b) mine.push_back(press(ps[k].start, ps[k].end, 8, (int)k + 1));
            if (mine.empty()) mine.push_back(press(0, 0, 0));
            sigs.push_back(anyOf(mine));
        }
        Events ev = run(sigs, ps.back().end + 400);
        bool same = ev.size() == ps.size();
        for (size_t k = 0; same && k < ps.size(); k++) same = (ev[k].second == ps[k].btn);
        CHECK(same, "satunnainen rollover: kaikki painallukset oikeassa järjestyksessä");
        CHECK(g_sim == 0 && g_rejSame == 0, "satunnainen rollover: ei hylkäyksiä");
        if (!same) {
            std::printf("  odotettiin %zu tapahtumaa, saatiin %zu (trial %d)\n", ps.size(), ev.size(), trial);
            break;
        }
    }
}

// ---------- pelin jono ----------

static void testCore() {
    typedef GameCore G;

    // oikea järjestys, lamput ehtivät sammua välissä (jono ei riipu valosta)
    {
        G g(4);
        CHECK(g.lampLit(0) == G::RUNNING, "lamppu 1 jonoon");
        CHECK(g.lampLit(2) == G::RUNNING, "lamppu 3 jonoon");
        CHECK(g.lampLit(1) == G::RUNNING, "lamppu 2 jonoon");
        CHECK(g.pending() == 3, "kolme jonossa");
        CHECK(g.at(0) == 0 && g.at(1) == 2 && g.at(2) == 1, "at() palauttaa jonon järjestyksessä");
        CHECK(g.press(0) == G::RUNNING, "1. painallus oikein");
        CHECK(g.press(2) == G::RUNNING, "2. painallus oikein");
        CHECK(g.press(1) == G::RUNNING, "3. painallus oikein");
        CHECK(g.score() == 3 && g.pending() == 0, "pisteet 3, jono tyhjä");
    }

    // väärä järjestys
    {
        G g(4);
        g.lampLit(0); g.lampLit(1);
        CHECK(g.press(1) == G::WRONG, "väärä järjestys (2 ennen 1) = WRONG");
        CHECK(g.score() == 0, "ei pisteitä väärästä");
    }

    // painallus ilman jonoa
    {
        G g(4);
        CHECK(g.press(0) == G::EARLY, "painallus tyhjällä jonolla = EARLY");
        g.lampLit(3);
        CHECK(g.press(3) == G::RUNNING, "oikea painallus");
        CHECK(g.press(3) == G::EARLY, "ylimääräinen painallus = EARLY");
    }

    // sama lamppu peräkkäin
    {
        G g(4);
        g.lampLit(2); g.lampLit(2); g.lampLit(2);
        CHECK(g.press(2) == G::RUNNING && g.press(2) == G::RUNNING && g.press(2) == G::RUNNING,
              "kolme samaa peräkkäin");
        CHECK(g.press(2) == G::EARLY, "neljäs painallus ilman lamppua = EARLY");
    }

    // jäljessä: maxLag 4 sallii 4 painamatonta, viides päättää pelin
    {
        G g(4);
        CHECK(g.lampLit(0) == G::RUNNING, "1/4");
        CHECK(g.lampLit(1) == G::RUNNING, "2/4");
        CHECK(g.lampLit(2) == G::RUNNING, "3/4");
        CHECK(g.lampLit(3) == G::RUNNING, "4/4 sallittu");
        CHECK(g.lampLit(0) == G::BEHIND, "5. painamaton = BEHIND");
    }

    // jäljessä, mutta kiinni ottaminen vapauttaa tilaa
    {
        G g(4);
        for (int i = 0; i < 4; i++) g.lampLit(i);
        CHECK(g.press(0) == G::RUNNING, "kuitataan yksi");
        CHECK(g.lampLit(1) == G::RUNNING, "tilaa taas yhdelle");
        CHECK(g.pending() == 4, "jonossa edelleen 4");
    }

    // sekoitettu: lamppu syttyy painalluksen välissä
    {
        G g(4);
        g.lampLit(0); g.lampLit(1);
        CHECK(g.press(0) == G::RUNNING, "a");
        g.lampLit(2);
        CHECK(g.press(1) == G::RUNNING, "b");
        CHECK(g.press(2) == G::RUNNING, "c");
        CHECK(g.score() == 3, "kolme pistettä");
    }

    // rengaspuskurin ympärimeno viitetoteutusta vastaan
    {
        G g(4);
        std::vector<int> shadow;
        uint32_t s = 12345;
        bool ok = true;
        for (int step = 0; step < 5000 && ok; step++) {
            s = s * 1664525u + 1013904223u;
            bool light = (s >> 16) & 1;
            uint8_t pad = (s >> 20) & 3;
            if (light) {
                if (shadow.size() < 4) {
                    ok = ok && g.lampLit(pad) == G::RUNNING;
                    shadow.push_back(pad);
                }
            } else if (!shadow.empty()) {
                ok = ok && g.press((uint8_t)shadow.front()) == G::RUNNING;
                shadow.erase(shadow.begin());
            }
            ok = ok && g.pending() == shadow.size();
        }
        CHECK(ok, "rengaspuskuri vastaa viitetoteutusta 5000 askeleen ajan");
    }

    // täysi jono ei ylivuoda muistiin
    {
        G g(200);
        G::Result r = G::RUNNING;
        for (int i = 0; i < 100 && r == G::RUNNING; i++) r = g.lampLit(0);
        CHECK(r == G::BEHIND && g.pending() <= G::QSIZE, "jono ei ylitä QSIZE:a");
    }
}

int main() {
    testFilter();
    testRollover();
    testCore();
    std::printf("%d tarkistusta, %d epäonnistui\n", checks, fails);
    return fails ? 1 : 0;
}
