#pragma once
#include <stdint.h>

// Pelin sääntöydin ilman Arduinoa (testattavissa koneella, ks. tools/host_tests).
//
// Jokainen syttynyt lamppu menee jonon perälle. Painallus täsmää jonon vanhimpaan, riippumatta
// siitä palaako lamppu vielä. Vain järjestys merkitsee.
//   press(pad)    : oikea -> piste, väärä -> WRONG, jono tyhjä -> EARLY
//   lampLit(pad)  : jonossa yli maxLag painamatonta -> BEHIND

class GameCore {
public:
    // SIMUL: kaksi nappia yhtä aikaa (päättää main.cpp suodattimen perusteella, ei press())
    enum Result { RUNNING, WRONG, EARLY, BEHIND, SIMUL };
    enum { QSIZE = 16 };

    explicit GameCore(uint8_t lag = 4) : maxLag(lag) { reset(); }

    void reset() { head = 0; count = 0; correct = 0; }

    Result lampLit(uint8_t pad) {
        if (count >= QSIZE) return BEHIND;
        q[(head + count) % QSIZE] = pad;
        count++;
        return count > maxLag ? BEHIND : RUNNING;
    }

    Result press(uint8_t pad) {
        if (count == 0) return EARLY;
        if (q[head] != pad) return WRONG;
        head = (uint8_t)((head + 1) % QSIZE);
        count--;
        correct++;
        return RUNNING;
    }

    // i. painamaton lamppu (0 = vanhin, jota seuraavaksi odotetaan)
    uint8_t at(uint8_t i) const { return q[(head + i) % QSIZE]; }

    uint16_t score() const { return correct; }
    uint8_t pending() const { return count; }
    uint8_t lag() const { return maxLag; }

private:
    uint8_t maxLag;
    uint8_t q[QSIZE];
    uint8_t head, count;
    uint16_t correct;
};
