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

# johdot: GPIO -> ULN-sisääntulo (LEDit)
led = [("GPIO11", 1), ("GPIO12", 2), ("GPIO13", 3), ("GPIO21", 4)]
colors = ["#ef6c00", "#f9a825", "#d84315", "#6d4c41"]
for (g, p), c in zip(led, colors):
    (r1, c1), (r2, c2) = find_td(g), ULN[p]
    x1, y1 = xy(r1, c1)
    x2, y2 = xy(r2, c2)
    ax.plot([x1, x1 + 0.0, x2], [y1, 6.2 + 0.25 * p, y2], color=c, lw=2, zorder=4, solid_capstyle="round")

# LED-katodit ULN-ulostuloista -> LED- (kaapeli koteloon)
for n in range(1, 5):
    p = 19 - n
    x, y = xy(*ULN[p])
    ax.annotate(f"LED{n}−", (x, y), (x, -3.4), ha="center", va="bottom", rotation=90, fontsize=9, color="#4a3a66",
                arrowprops=dict(arrowstyle="-", color="#4a3a66", lw=1.5))

# napit: GPIO -> kaapeli napille (toinen jalka GND)
for n, g in enumerate(["GPIO1", "GPIO2", "GPIO3", "GPIO10"], 1):
    x, y = xy(*find_td(g))
    ax.annotate(f"Nappi{n}", (x, y), (x, 13.0), ha="center", va="top", rotation=90, fontsize=9, color="#1565c0",
                arrowprops=dict(arrowstyle="-", color="#1565c0", lw=1.5))

# GND ULN:lle
gx, gy = xy(*ULN[9])
b3x, b3y = xy("b", 3)
ax.plot([gx, gx, b3x], [gy, 15.3, 15.3], color="#222", lw=1.8, zorder=3)
ax.plot([b3x, b3x], [15.3, b3y], color="#222", lw=1.8, zorder=3)
ax.text(14, 15.8, "GND: ULN pin 9 (h28) → T-Display GND (b3)", fontsize=8.5, color="#222", ha="center")

ax.text(20, -4.6, "LED+ (220 Ω sarjaan) → +V,  ULN pin 10 COM (b28) → +V tai jätä auki", fontsize=8.5,
        color="#4a3a66", ha="center")
ax.text(-8.7, 13.2, "Johdot (värit):\n  GPIO11 → ULN 1B (h20)\n  GPIO12 → ULN 2B (h21)\n  GPIO13 → ULN 3B (h22)\n  GPIO21 → ULN 4B (h23)",
        fontsize=8.5, va="top", family="monospace")
ax.set_title("Nopeuspeli – protolevyn kytkentä (yläkuva paneelin suunnasta, rivi a takareunassa)", fontsize=13,
             pad=20)
ax.set_xlim(-9, 32)
ax.set_ylim(16.6, -5.6)
fig.savefig("docs/kytkenta.png", dpi=130, bbox_inches="tight")
print("ok")
