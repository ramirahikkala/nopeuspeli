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
for n in range(1, 5):
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

ax.text(-8.7, 17.4, "Yhtenäinen = tehty hyppylanka:\n"
        "  j1 ↔ +1 (5V)   j2 ↔ −1 (GND)   j11 ↔ i20 (GPIO1 → ULN 1B)\n"
        "  a28 ↔ +28 (ULN COM)   j28 ↔ −28 (ULN GND)\n"
        "Katkoviiva = suunniteltu (CLAUDE.md): GPIO11/12/13/21 → ULN 1B–4B (oranssi),\n"
        "  napit N1–N4 = GPIO1/2/3/10, LED-katodit L1–L4 = ULN 1C–4C (b20–b23)\n"
        "Kiskojen sijainti piirretty arviolla.",
        fontsize=8.5, va="top", family="monospace")
ax.set_title("Nopeuspeli – protolevyn kytkentä (yläkuva paneelin suunnasta, rivi a takareunassa)", fontsize=13,
             pad=20)
ax.set_xlim(-9, 32)
ax.set_ylim(21.6, -5.7)
fig.savefig("docs/kytkenta.png", dpi=130, bbox_inches="tight")
print("ok")
