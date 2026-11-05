"""Piirtää protolevyn kytkentäkaavion (docs/kytkenta.png).

Ajo: python3 docs/kytkenta.py   (vaatii matplotlib)

Koordinaatit: sarake 1 vasemmalla, rivi a ylhäällä (= takareuna), yläkuva paneelin
suunnasta. Mitat ovat kotelon STEP-mallista (electronics_reference_v3_17.step).
"""
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

PITCH = 2.54
ROW_Y = {r: y for r, y in zip("abcdefghij", [0, 1, 2, 3, 4, 7, 8, 9, 10, 11])}

# --- T-Display S3 -piikkirivit (sarake 1 = USB-C-pää, sarake 12 = 3V-pää) ---
# P1 = rivi b, P2 = rivi i (katsottuna paneelin puolelta, näyttö ylöspäin)
P2 = ["3V", "GPIO1", "GPIO2", "GPIO3", "GPIO10", "GPIO11", "GPIO12", "GPIO13", "NC", "NC", "GND", "5V"]
P1 = ["GND", "GND", "GPIO43", "GPIO44", "GPIO18", "GPIO17", "GPIO21", "GPIO16", "NC", "GND", "GND", "3V"]
TD = {}  # (rivi, sarake) -> nimi
for i, name in enumerate(P2):
    TD[("i", 12 - i)] = name
for i, name in enumerate(P1):
    TD[("b", 12 - i)] = name

# --- ULN2803 (DIP-18-numerointi: 1 = h20, 9 = h28, 10 = b28, 18 = b20) ---
ULN = {}
for p in range(1, 10):
    ULN[p] = ("h", 19 + p)
for p in range(10, 19):
    ULN[p] = ("b", 38 - p)
ULN_NAME = {1: "1B", 2: "2B", 3: "3B", 4: "4B", 5: "5B", 6: "6B", 7: "7B", 8: "8B", 9: "GND", 10: "COM",
            11: "8C", 12: "7C", 13: "6C", 14: "5C", 15: "4C", 16: "3C", 17: "2C", 18: "1C"}


def xy(row, col):
    return col, ROW_Y[row]


def find_td(name):
    return next(k for k, v in TD.items() if v == name)


fig, ax = plt.subplots(figsize=(17, 11))
ax.set_aspect("equal")
ax.invert_yaxis()
ax.axis("off")

# protolevy 81 x 55
left = 1 - 3.9 / PITCH
ax.add_patch(Rectangle((left, -5.33), 81 / PITCH, 55 / PITCH, fc="#e6efdc", ec="#6b7d5c", lw=1.5, zorder=0))
# T-Display PCB 60,78 x 25,5 (vasen pää ulottuu levyn yli)
td_r = left + 40.68 / PITCH
ax.add_patch(Rectangle((td_r - 60.78 / PITCH, 0.48), 60.78 / PITCH, 25.5 / PITCH, fc="#7fbf8f", ec="#2f6f3f", lw=1.5,
                       alpha=0.55, zorder=1))
ax.text(td_r - 60.78 / PITCH + 0.4, 5.5, "T-Display S3\n(näyttö ylös, USB-C vasemmalla)", fontsize=10, va="center",
        color="#1f4f2f")
# ULN-moduuli 24 x 23
uln_l = left + 50.28 / PITCH
ax.add_patch(Rectangle((uln_l, 0.48), 24 / PITCH, 23 / PITCH, fc="#8e7ca8", ec="#4a3a66", lw=1.5, alpha=0.5, zorder=1))
ax.text(24.0, 5.0, "ULN2803", fontsize=11, ha="center", va="center", color="#2a1f40", weight="bold")

# reikäruudukko
for r, y in ROW_Y.items():
    ax.text(-1.2, y, r, ha="center", va="center", fontsize=9, color="#555")
    for c in range(1, 31):
        ax.plot(c, y, "o", ms=3.2, color="#b9c4ad", zorder=2)
for c in range(1, 31):
    ax.text(c, -0.9, str(c), ha="center", va="center", fontsize=8, color="#555")

# pinnit
for (r, c), name in TD.items():
    x, y = xy(r, c)
    col = {"GND": "#222", "5V": "#c62828", "3V": "#c62828", "NC": "#999"}.get(name, "#1565c0")
    ax.plot(x, y, "o", ms=7, color=col, zorder=5)
    dy = -0.55 if r == "b" else 0.62
    ax.text(x, y + dy, name.replace("GPIO", ""), ha="center", va="center", fontsize=8, color=col, zorder=6,
            rotation=0, weight="bold")
for p, (r, c) in ULN.items():
    x, y = xy(r, c)
    ax.plot(x, y, "s", ms=6.5, color="#4a3a66", zorder=5)
    dy = -0.55 if r == "b" else 0.62
    ax.text(x, y + (-0.75 if r == "b" else 0.85), f"{p}\n{ULN_NAME[p]}", ha="center", va="center", fontsize=7.5, color="#2a1f40", zorder=6, linespacing=1.0)


# --- kiskot (reunakiskot, sijainti arvio: + ja − sekä ylhäällä että alhaalla) ---
RAILS = {"yla+": (-2.4, "#c62828", "+"), "yla-": (-3.3, "#222", "−"),
         "ala+": (12.9, "#c62828", "+"), "ala-": (13.8, "#222", "−")}
for key, (ry, rc, lab) in RAILS.items():
    ax.plot([1, 30], [ry, ry], "-", color=rc, lw=1, alpha=0.35, zorder=2)
    for c in range(1, 31):
        ax.plot(c, ry, "o", ms=2.6, color=rc, alpha=0.5, zorder=2)
    ax.text(0.2, ry, lab, color=rc, ha="center", va="center", fontsize=10, weight="bold")

# --- SUUNNITELTU (katkoviiva): GPIO -> ULN-sisääntulo, napit, LED-katodit ---
plan = [("GPIO11", 1), ("GPIO12", 2), ("GPIO13", 3), ("GPIO21", 4)]
for (g, pn) in plan:
    (r1, c1), (r2, c2) = find_td(g), ULN[pn]
    x1, y1 = xy(r1, c1)
    x2, y2 = xy(r2, c2)
    ax.plot([x1, x2], [y1, y2], "--", color="#ef6c00", lw=1.3, alpha=0.55, zorder=3)
for n, g in enumerate(["GPIO1", "GPIO2", "GPIO3", "GPIO10"], 1):
    x, y = xy(*find_td(g))
    ax.plot([x, x], [y, 11.9], "--", color="#1565c0", lw=1.3, alpha=0.6, zorder=3)
    ax.text(x, 12.05, f"N{n}", ha="center", va="top", fontsize=7.5, color="#1565c0")
for n in range(2, 5):
    x, y = xy(*ULN[19 - n])
    ax.plot([x, x], [y, -1.6], "-", color="#4a3a66", lw=1.5, zorder=3)
    ax.text(x, -1.75, f"L{n}", ha="center", va="bottom", fontsize=7.5, color="#4a3a66")

# --- TEHDYT hyppylangat (yhtenäinen viiva) ---
def wire(a, b, color, label=None):
    (x1, y1), (x2, y2) = a, b
    ax.plot([x1, x2], [y1, y2], "-", color=color, lw=3, zorder=7, solid_capstyle="round")
    ax.plot([x1, x2], [y1, y2], "o", ms=5, color=color, zorder=8)

wire(xy("j", 1), (1, RAILS["ala+"][0]), "#c62828")          # j1  <-> +1  (5V)
wire(xy("j", 2), (2, RAILS["ala-"][0]), "#222")             # j2  <-> -1  (GND)
wire(xy("j", 11), xy("i", 20), "#ef6c00")                   # j11 <-> i20 (GPIO1 -> ULN 1B)
wire(xy("a", 28), (28, RAILS["yla+"][0]), "#c62828")        # a28 <-> +28 (ULN COM)
wire(xy("j", 28), (28, RAILS["ala-"][0]), "#222")           # j28 <-> -28 (ULN GND)


# --- TEHTY: nappi 1 + LED 1 (kotelon painike: nappi vasemmalla, lamppu oikealla) ---
from matplotlib.patches import FancyBboxPatch, Circle, Polygon
Y0 = -7.4  # liittimien taso
ax.add_patch(FancyBboxPatch((1.9, -11.0), 19.4, 3.9, boxstyle="round,pad=0.1", fc="#fff8dc", ec="#b8860b", lw=1.5, zorder=3))
ax.text(11.6, -11.4, "Nappi 1 + LED 1 (sama kotelon painike)", ha="center", va="bottom", fontsize=10, weight="bold",
        color="#6b4e00", zorder=4)
# -- NAPPI (kytkin) --
ax.add_patch(FancyBboxPatch((2.2, -10.4), 6.4, 3.0, boxstyle="round,pad=0.05", fc="#dfe9fb", ec="#1565c0", lw=1.2, zorder=3))
ax.text(5.4, -10.05, "NAPPI (kytkin)", ha="center", va="center", fontsize=9, weight="bold", color="#1565c0", zorder=4)
ax.plot([3, 3, 4.3], [Y0, -8.3, -8.3], color="#222", lw=2, zorder=5)            # GND-puoli
ax.plot([4.3, 6.2], [-8.3, -9.2], color="#1565c0", lw=2.2, zorder=5)            # kytkimen vipu
ax.plot([4.3, 5.9], [-8.3, -8.3], color="#1565c0", lw=0, zorder=5)
ax.plot([5.9, 7, 7], [-8.3, -8.3, Y0], color="#1565c0", lw=2, zorder=5)         # signaali-puoli
ax.plot([4.3, 5.9], [-8.3, -8.3], "o", ms=5, color="#1565c0", zorder=6)
ax.text(3.0, -6.95, "GND", ha="center", va="top", fontsize=8, color="#222", zorder=6)
ax.text(7.0, -6.95, "signaali", ha="center", va="top", fontsize=8, color="#1565c0", zorder=6)
# -- LAMPPU (LED) --
ax.add_patch(FancyBboxPatch((9.7, -10.4), 11.2, 3.0, boxstyle="round,pad=0.05", fc="#ffe9e6", ec="#c62828", lw=1.2, zorder=3))
ax.text(15.3, -10.05, "LAMPPU (LED)", ha="center", va="center", fontsize=9, weight="bold", color="#c62828", zorder=4)
ax.plot([12, 12, 14.2], [Y0, -8.4, -8.4], color="#c62828", lw=2, zorder=5)         # anodi
ax.add_patch(Polygon([[14.2, -9.2], [14.2, -7.6], [16.0, -8.4]], closed=True, fc="#ffd54f", ec="#c62828", lw=1.8, zorder=5))
ax.plot([16.0, 16.0], [-9.2, -7.6], color="#c62828", lw=2.2, zorder=5)             # katodin palkki
ax.plot([16.0, 18, 18], [-8.4, -8.4, Y0], color="#4a3a66", lw=2, zorder=5)         # katodi
for dx in (0.0, 0.9):                                                              # valonsäteet
    ax.annotate("", xy=(15.9 + dx, -10.0 + 0.1), xytext=(14.9 + dx, -9.1), arrowprops=dict(arrowstyle="->", color="#f9a825", lw=1.6), zorder=5)
ax.text(12.0, -6.95, "+ (anodi)", ha="center", va="top", fontsize=8, color="#c62828", zorder=6)
ax.text(18.0, -6.95, "− (katodi)", ha="center", va="top", fontsize=8, color="#4a3a66", zorder=6)
def cable(tx, ty, target, color):
    ax.plot([tx, target[0]], [ty, target[1]], "-", color=color, lw=2.2, zorder=6, alpha=0.9)
    ax.plot(tx, ty, "o", ms=6, color=color, zorder=7)
    ax.plot(*target, "o", ms=5, color=color, zorder=8)
cable(3.0, Y0, (3, RAILS["yla-"][0]), "#222")        # -3  -> napin GND
cable(7.0, Y0, xy("a", 10), "#1565c0")               # a10 -> napin signaali (b10 = GPIO43)
cable(12.0, Y0, (4, RAILS["yla+"][0]), "#c62828")    # +4  -> LED anodi
cable(18.0, Y0, xy("e", 19), "#4a3a66")              # e19 -> LED katodi
# vastus a19-a20 (220 ohm): e19/a19 -solmu -> ULN 1C (b20)
rx0, rx1, ry = 19, 20, 0
ax.plot([rx0, rx0 + 0.2], [ry, ry], "-", color="#7a5c00", lw=2.5, zorder=8)
ax.add_patch(Rectangle((rx0 + 0.2, ry - 0.17), 0.6, 0.34, fc="#d9b96c", ec="#7a5c00", lw=1.2, zorder=8))
ax.plot([rx0 + 0.8, rx1], [ry, ry], "-", color="#7a5c00", lw=2.5, zorder=8)
ax.plot([rx0, rx1], [ry, ry], "o", ms=4.5, color="#7a5c00", zorder=9)
ax.text(18.6, ry - 0.0, "220 Ω", ha="right", va="center", fontsize=8, color="#7a5c00", weight="bold")

ax.text(-8.7, 17.4, "Yhtenäinen = tehty hyppylanka:\n"
        "  j1 ↔ +1 (5V)   j2 ↔ −1 (GND)   j11 ↔ i20 (GPIO1 → ULN 1B)\n"
        "  a28 ↔ +28 (ULN COM)   j28 ↔ −28 (ULN GND)\n"
        "  Nappi 1 + LED 1: +4 → LED+, −3 → GND, a10 → nappi (GPIO43), e19 → LED−\n"
        "  220 Ω a19–a20: LED− (e19) → ULN 1C (b20)\n"
        "Katkoviiva = suunniteltu (CLAUDE.md): GPIO11/12/13/21 → ULN 1B–4B (oranssi),\n"
        "  napit N1–N4 = GPIO1/2/3/10, LED-katodit L1–L4 = ULN 1C–4C (b20–b23)\n"
        "Kiskojen sijainti on arvio; ylä- ja alakiskot oletetaan samaksi verkoksi (+ ja −).",
        fontsize=8.5, va="top", family="monospace")
ax.set_title("Nopeuspeli – protolevyn kytkentä (yläkuva paneelin suunnasta, rivi a takareunassa)", fontsize=13,
             pad=20)
ax.set_xlim(-9, 32)
ax.set_ylim(23.2, -11.8)
fig.savefig("docs/kytkenta.png", dpi=130, bbox_inches="tight")
print("ok")
