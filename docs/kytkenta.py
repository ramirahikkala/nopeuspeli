"""Piirtää protolevyn kytkentäkaavion (docs/kytkenta.png).

Ajo: python3 docs/kytkenta.py   (vaatii matplotlib)

Yläkuva paneelin suunnasta: sarake 1 vasemmalla, rivi a takareunassa.
Mitat ovat kotelon STEP-mallista (electronics_reference_v3_17.step).
TEHTY = yhtenäinen paksu viiva + täytetty piste.  TEKEMÄTTÄ = katkoviiva + tyhjä piste.
"""
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, FancyBboxPatch, Rectangle

PITCH = 2.54
ROW_Y = {r: y for r, y in zip("abcdefghij", [0, 1, 2, 3, 4, 7, 8, 9, 10, 11])}

# --- T-Display S3 -piikkirivit (sarake 1 = USB-C-pää, sarake 12 = 3V-pää) ---
P2 = ["3V", "GPIO1", "GPIO2", "GPIO3", "GPIO10", "GPIO11", "GPIO12", "GPIO13", "NC", "NC", "GND", "5V"]
P1 = ["GND", "GND", "GPIO43", "GPIO44", "GPIO18", "GPIO17", "GPIO21", "GPIO16", "NC", "GND", "GND", "3V"]
TD = {}
for i, name in enumerate(P2):
    TD[("i", 12 - i)] = name
for i, name in enumerate(P1):
    TD[("b", 12 - i)] = name

# --- ULN2803 (DIP-18: 1 = h20, 9 = h28, 10 = b28, 18 = b20) ---
ULN = {}
for p in range(1, 10):
    ULN[p] = ("h", 19 + p)
for p in range(10, 19):
    ULN[p] = ("b", 38 - p)
ULN_NAME = {1: "1B", 2: "2B", 3: "3B", 4: "4B", 5: "5B", 6: "6B", 7: "7B", 8: "8B", 9: "GND", 10: "COM",
            11: "8C", 12: "7C", 13: "6C", 14: "5C", 15: "4C", 16: "3C", 17: "2C", 18: "1C"}

# --- Painikkeet (nappi + lamppu) ---
# ULN-kanavat 1, 3, 5, 7: jokaisella vastuksella on vapaa naapurisarake (19, 21, 23, 25)
PAINIKE = [
    dict(n=1, color="#ef6c00", done=True, led_col=11, uln_col=20, res=19, minus_col=19, btn=("a", 10)),
    dict(n=2, color="#00897b", done=False, led_col=10, uln_col=22, res=21, minus_col=21, btn=("a", 8)),
    dict(n=3, color="#8e24aa", done=False, led_col=9, uln_col=24, res=23, minus_col=23, btn=("a", 7)),
    dict(n=4, color="#d81b60", done=False, led_col=8, uln_col=26, res=25, minus_col=25, btn=("a", 6)),
]

RAILS = {"yla+": (-2.4, "#c62828", "+"), "yla-": (-3.3, "#222", "−"),
         "ala+": (14.6, "#c62828", "+"), "ala-": (15.5, "#222", "−")}


def xy(row, col):
    return col, ROW_Y[row]


fig, ax = plt.subplots(figsize=(18, 17))
ax.set_aspect("equal")
ax.invert_yaxis()
ax.axis("off")

# --- levy, T-Display, ULN ---
left = 1 - 3.9 / PITCH
ax.add_patch(Rectangle((left, -5.33), 81 / PITCH, 55 / PITCH, fc="#e6efdc", ec="#6b7d5c", lw=1.5, zorder=0))
td_r = left + 40.68 / PITCH
ax.add_patch(Rectangle((td_r - 60.78 / PITCH, 0.48), 60.78 / PITCH, 25.5 / PITCH, fc="#7fbf8f", ec="#2f6f3f", lw=1.5,
                       alpha=0.45, zorder=1))
ax.text(td_r - 60.78 / PITCH + 0.4, 5.5, "T-Display S3\n(näyttö ylös,\nUSB-C vasemmalla)", fontsize=10,
        va="center", color="#1f4f2f")
uln_l = left + 50.28 / PITCH
ax.add_patch(Rectangle((uln_l, 0.48), 24 / PITCH, 23 / PITCH, fc="#8e7ca8", ec="#4a3a66", lw=1.5, alpha=0.4, zorder=1))
ax.text(24.0, 6.0, "ULN2803", fontsize=11, ha="center", va="center", color="#2a1f40", weight="bold")

# --- reikäruudukko ---
for r, y in ROW_Y.items():
    ax.text(-1.2, y, r, ha="center", va="center", fontsize=9, color="#555")
    for c in range(1, 31):
        ax.plot(c, y, "o", ms=3.2, color="#b9c4ad", zorder=2)
for c in range(1, 31):
    ax.text(c, -0.95, str(c), ha="center", va="center", fontsize=8, color="#555")

# --- kiskot (yksi + ja yksi − verkko; ylä/ala piirretty erikseen vain selkeyden vuoksi) ---
for key, (ry, rc, lab) in RAILS.items():
    ax.plot([1, 30], [ry, ry], "-", color=rc, lw=1, alpha=0.35, zorder=2)
    for c in range(1, 31):
        ax.plot(c, ry, "o", ms=2.6, color=rc, alpha=0.5, zorder=2)
    ax.text(0.2, ry, lab, color=rc, ha="center", va="center", fontsize=11, weight="bold")

# --- T-Displayn ja ULN:n pinnit ---
for (r, c), name in TD.items():
    x, y = xy(r, c)
    col = {"GND": "#222", "5V": "#c62828", "3V": "#c62828", "NC": "#999"}.get(name, "#1565c0")
    ax.plot(x, y, "o", ms=7, color=col, zorder=5)
    ax.text(x, y + (-0.55 if r == "b" else 0.62), name.replace("GPIO", ""), ha="center", va="center", fontsize=8,
            color=col, zorder=6, weight="bold")
for p, (r, c) in ULN.items():
    x, y = xy(r, c)
    ax.plot(x, y, "s", ms=6.5, color="#4a3a66", zorder=5)
    ax.text(x, y + (-0.75 if r == "b" else 0.85), f"{p}\n{ULN_NAME[p]}", ha="center", va="center", fontsize=7.5,
            color="#2a1f40", zorder=6, linespacing=1.0)

LW = 4.0


def dot(pt, color, done, ms=8):
    ax.plot(*pt, "o", ms=ms, mfc=color if done else "white", mec=color, mew=2, zorder=9)


def hole_label(pt, text, color, dy=0.0, dx=0.0):
    ax.text(pt[0] + dx, pt[1] + dy, text, fontsize=7.5, color=color, ha="center", va="center", zorder=10,
            weight="bold", bbox=dict(boxstyle="round,pad=0.12", fc="white", ec=color, lw=0.8, alpha=0.95))


def line(pts, color, done, lw=LW, z=7):
    xs, ys = zip(*pts)
    ax.plot(xs, ys, linestyle="-" if done else (0, (3.2, 2.2)), color=color, lw=lw, zorder=z, solid_capstyle="round",
            dash_capstyle="butt")


def wire(a, b, color, done=True):
    line([a, b], color, done)
    dot(a, color, done)
    dot(b, color, done)


def resistor(c0, color, done):
    y = 0
    line([(c0, y), (c0 + 0.2, y)], color, done, lw=3)
    ax.add_patch(Rectangle((c0 + 0.15, y - 0.25), 0.7, 0.5, fc="#e8c872" if done else "white", ec=color, lw=2,
                           ls="-" if done else "--", zorder=8))
    line([(c0 + 0.8, y), (c0 + 1, y)], color, done, lw=3)
    dot((c0, y), color, done, 6)
    dot((c0 + 1, y), color, done, 6)


# --- painikeyksiköt levyn yläpuolella ---
UW, X0, YT, YB = 6.3, 5.8, -11.4, -7.6
BUS_GND, BUS_PLUS = -13.5, -12.45


def unit_geo(k):
    x0 = X0 + UW * (k - 1)
    return dict(x0=x0, gnd=(x0 + 0.9, YT), plus=(x0 + 4.4, YT), sig=(x0 + 1.9, YB), minus=(x0 + 5.2, YB))


for P in PAINIKE:
    k, col, done = P["n"], P["color"], P["done"]
    g = unit_geo(k)
    x0 = g["x0"]
    ax.add_patch(FancyBboxPatch((x0 + 0.05, YT), UW - 0.3, YB - YT, boxstyle="round,pad=0.05",
                                fc="#fffbea" if done else "#f5f5f5", ec=col, lw=2.5, ls="-" if done else "--",
                                zorder=3))
    cx = x0 + UW / 2 - 0.1
    ax.text(cx, -10.95, f"PAINIKE {k}", ha="center", va="center", fontsize=10, weight="bold", color=col, zorder=4)
    ax.text(cx, -10.35, "✔ TEHTY" if done else "☐ TEKEMÄTTÄ", ha="center", va="center", fontsize=7.5,
            color="#2e7d32" if done else "#b71c1c", weight="bold", zorder=4)
    ax.add_patch(Circle((x0 + 1.4, -8.95), 0.78, fc="#dfe9fb", ec="#1565c0", lw=2, zorder=4))
    ax.text(x0 + 1.4, -8.95, "NAPPI", ha="center", va="center", fontsize=6.5, color="#1565c0", weight="bold",
            zorder=5)
    ax.add_patch(Circle((x0 + 4.65, -8.95), 0.78, fc="#ffe082", ec="#c62828", lw=2, zorder=4))
    ax.text(x0 + 4.65, -8.95, "LAMPPU", ha="center", va="center", fontsize=6, color="#c62828", weight="bold",
            zorder=5)
    ax.plot([g["gnd"][0], x0 + 1.4], [YT, -9.73], color="#222", lw=2, zorder=4)
    ax.plot([g["sig"][0], x0 + 1.4], [YB, -8.17], color="#1565c0", lw=2, zorder=4)
    ax.plot([g["plus"][0], x0 + 4.65], [YT, -9.73], color="#c62828", lw=2, zorder=4)
    ax.plot([g["minus"][0], x0 + 4.65], [YB, -8.17], color="#4a3a66", lw=2, zorder=4)
    for key, c in (("gnd", "#222"), ("plus", "#c62828"), ("sig", "#1565c0"), ("minus", "#4a3a66")):
        ax.plot(*g[key], "o", ms=6, color=c, zorder=6)
    ax.text(g["sig"][0], YB + 0.45, "signaali", ha="center", va="top", fontsize=6.5, color="#1565c0", zorder=6)
    ax.text(g["minus"][0], YB + 0.45, "LED−", ha="center", va="top", fontsize=6.5, color="#4a3a66", zorder=6)

# --- haaroittava kaapeli: levylle vain 2 johtoa (+4, −3), jaetaan 4 painikkeelle ---
line([(3, BUS_GND), (3, RAILS["yla-"][0])], "#222", True)
dot((3, RAILS["yla-"][0]), "#222", True)
line([(4, BUS_PLUS), (4, RAILS["yla+"][0])], "#c62828", True)
dot((4, RAILS["yla+"][0]), "#c62828", True)
hole_label((3, RAILS["yla-"][0]), "−3", "#222", dy=-0.85, dx=-0.55)
hole_label((4, RAILS["yla+"][0]), "+4", "#c62828", dy=0.85, dx=0.55)
for P in PAINIKE:
    k, done = P["n"], P["done"]
    g = unit_geo(k)
    prev = unit_geo(k - 1) if k > 1 else None
    sx = 3 if prev is None else prev["gnd"][0]
    line([(sx, BUS_GND), (g["gnd"][0], BUS_GND), g["gnd"]], "#222", done)
    sx = 4 if prev is None else prev["plus"][0]
    line([(sx, BUS_PLUS), (g["plus"][0], BUS_PLUS), g["plus"]], "#c62828", done)
ax.text(1.0, -14.7, "HAAROITTAVA KAAPELI: levylle vain 2 johtoa (+4, −3) – jaetaan 4 napille/lampulle ketjuna",
        fontsize=10, weight="bold", color="#555")
ax.text(31.2, BUS_GND - 0.45, "GND (napit)", fontsize=8.5, color="#222", va="center", ha="right", weight="bold")
ax.text(31.2, BUS_PLUS - 0.45, "+V (lamput)", fontsize=8.5, color="#c62828", va="center", ha="right", weight="bold")

# --- painikkeiden johdot levylle ---
for P in PAINIKE:
    k, col, done = P["n"], P["color"], P["done"]
    g = unit_geo(k)
    tgt = xy(*P["btn"])  # nappi -> a-reikä
    wire(g["sig"], tgt, col, done)
    hole_label(tgt, f"{P['btn'][0]}{P['btn'][1]}", col, dy=-0.62)
    tgt = xy("e", P["minus_col"])  # LED-miinus -> e-reikä
    wire(g["minus"], tgt, col, done)
    hole_label(tgt, f"e{P['minus_col']}", col, dy=0.62)
    resistor(P["res"], col, done)  # 220 Ω a-rivillä: LED−-solmu <-> ULN-kanava
    # GPIO -> ULN-sisääntulo (kaari rivin j alapuolella; sisäkkäiset kaaret eivät risteä)
    a = xy("j", P["led_col"])
    b = xy("i", P["uln_col"])
    ybot = a[1] + [0.9, 1.5, 2.1, 2.7][k - 1] + 0.3
    line([a, (a[0], ybot), (b[0], ybot), b], col, done)
    dot(a, col, done)
    dot(b, col, done)
    hole_label(a, f"j{P['led_col']}", col, dy=0.65)
    hole_label(b, f"i{P['uln_col']}", col, dy=0.65)

# --- muut tehdyt hyppylangat (kiskot) ---
wire(xy("j", 1), (1, RAILS["ala+"][0]), "#c62828")     # j1  <-> +1  (5V)
wire(xy("j", 2), (2, RAILS["ala-"][0]), "#222")        # j2  <-> -1  (GND)
wire(xy("a", 28), (28, RAILS["yla+"][0]), "#c62828")   # a28 <-> +28 (ULN COM)
wire(xy("j", 28), (28, RAILS["ala-"][0]), "#222")      # j28 <-> -28 (ULN GND)
ax.text(1.5, 13.75, "j1 → +1 (5V)", fontsize=8, color="#c62828", ha="left", va="center", weight="bold")
ax.text(2.5, 16.0, "j2 → −1 (GND)", fontsize=8, color="#222", ha="left", va="center", weight="bold")
ax.text(28.5, 12.2, "j28 → −28\n(ULN GND)", fontsize=8, color="#222", ha="left", va="center", weight="bold")
ax.text(27.6, -1.55, "a28 → +28 (ULN COM)", fontsize=8, color="#c62828", ha="right", va="center", weight="bold")

# --- tehty / tekemättä -selite ---
done_txt = (
    "✔ TEHTY (yhtenäinen viiva, täytetty piste)\n"
    "  ✔ j1 ↔ +1          5V → + kisko\n"
    "  ✔ j2 ↔ −1          GND → − kisko\n"
    "  ✔ a28 ↔ +28        ULN COM → +\n"
    "  ✔ j28 ↔ −28        ULN GND → −\n"
    "  ✔ j11 ↔ i20        GPIO1 → ULN 1B\n"
    "  ✔ vastus a19–a20   ULN 1C ↔ LED−-solmu\n"
    "  ✔ Painike 1: +4 → LED+, −3 → nappi GND,\n"
    "               a10 → nappi (GPIO43), e19 → LED−"
)
todo_txt = (
    "☐ TEKEMÄTTÄ (katkoviiva, tyhjä piste)\n"
    "  ☐ Painike 2: j10 ↔ i22 (GPIO2 → ULN 3B), vastus a21–a22,\n"
    "               e21 → LED−, a8 → nappi (GPIO18)\n"
    "  ☐ Painike 3: j9 ↔ i24 (GPIO3 → ULN 5B), vastus a23–a24,\n"
    "               e23 → LED−, a7 → nappi (GPIO17)\n"
    "  ☐ Painike 4: j8 ↔ i26 (GPIO10 → ULN 7B), vastus a25–a26,\n"
    "               e25 → LED−, a6 → nappi (GPIO21)\n"
    "  ☐ haaroittava kaapeli: +V ja GND 1 → 2 → 3 → 4 (kuvan ylälaita)\n"
    "  Vastukset 220 Ω. Käyttämättömät ULN-tulot 2B, 4B, 6B, 8B → GND (suositus)."
)
ax.text(-8.7, 18.4, done_txt, fontsize=9.5, va="top", family="monospace", color="#1b5e20")
ax.text(11.0, 18.4, todo_txt, fontsize=9.5, va="top", family="monospace", color="#b71c1c")
ax.set_title("Nopeuspeli – protolevyn kytkentä: tehty ja tekemättä", fontsize=14, pad=18)
ax.set_xlim(-9, 32)
ax.set_ylim(28.5, -15.4)
fig.savefig("docs/kytkenta.png", dpi=120, bbox_inches="tight")
print("ok")
