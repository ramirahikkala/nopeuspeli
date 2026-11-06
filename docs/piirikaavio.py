"""Piirikaavio (schematic). Aja repon juuresta: python3 docs/piirikaavio.py"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon, Circle

fig, ax = plt.subplots(figsize=(14, 9))
ax.set_xlim(0, 100); ax.set_ylim(-12, 68); ax.set_aspect("equal"); ax.axis("off")
K = "black"
def ln(xs, ys, c=K, lw=1.8): ax.plot(xs, ys, color=c, lw=lw, solid_capstyle="round")
def dot(x, y): ax.add_patch(Circle((x, y), 0.45, color=K, zorder=5))
def txt(x, y, s, **k): ax.text(x, y, s, ha=k.pop("ha", "center"), va=k.pop("va", "center"), fontsize=k.pop("fs", 10), **k)

def box(x0, y0, x1, y1, title):
    ax.add_patch(Rectangle((x0, y0), x1-x0, y1-y0, fill=True, fc="#f4f4f4", ec=K, lw=2))
    txt((x0+x1)/2, y1+1.5, title, fs=12, weight="bold")

def resistor(x0, x1, y):
    n = 6; w = (x1-x0-2)/n
    pts = [(x0, y), (x0+1, y)]
    for i in range(n):
        pts.append((x0+1+w*(i+.5), y+(1.2 if i % 2 == 0 else -1.2)))
    pts += [(x1-1, y), (x1, y)]
    ln([p[0] for p in pts], [p[1] for p in pts])

def led(x0, x1, y):
    """katodi vasemmalla (x0), anodi oikealla (x1); virta oikealta vasemmalle"""
    xm = (x0+x1)/2
    ln([x0, xm-1.5], [y, y]); ln([xm+1.5, x1], [y, y])
    ax.add_patch(Polygon([[xm+1.5, y+1.8], [xm+1.5, y-1.8], [xm-1.5, y]], closed=True, fc="#ffe066", ec=K, lw=1.8))
    ln([xm-1.5, xm-1.5], [y-1.8, y+1.8])
    for dx in (0, 1.6):
        ax.annotate("", xy=(xm+1.5+dx, y+3.6), xytext=(xm+dx-0.3, y+2.2), arrowprops=dict(arrowstyle="->", color=K, lw=1.2))

def button(x0, x1, y):
    xm = (x0+x1)/2
    ln([x0, xm-2], [y, y]); ln([xm+2, x1], [y, y])
    dot(xm-2, y); dot(xm+2, y)
    ln([xm-2, xm+1.5], [y, y+1.8])           # kosketin
    ln([xm-1, xm-1], [y+2.8, y+3.8]); ln([xm-3, xm+1], [y+3.8, y+3.8])  # painike

def gnd(x, y):
    ln([x, x], [y, y-1]); 
    for i, w in enumerate((2.2, 1.4, .6)):
        ln([x-w, x+w], [y-1-i*.7, y-1-i*.7])

# --- T-Display ---
box(3, -4, 20, 62, "T-Display S3")
led_rows = [(54, "GPIO1"), (46, "GPIO2"), (38, "GPIO3"), (30, "GPIO10")]
btn_rows = [(15, "GPIO43"), (10, "GPIO18"), (5, "GPIO17"), (0, "GPIO21")]
for y, n in led_rows + btn_rows:
    txt(19, y, n, ha="right", fs=10)
    ln([20, 24], [y, y]); 
txt(19, 60, "5V", ha="right"); ln([20, 24], [60, 60])
txt(19, -2.5, "GND", ha="right"); ln([20, 24], [-2.5, -2.5])

# --- ULN2803 ---
box(30, 24, 46, 58, ""); txt(36, 56.5, "ULN2803", fs=12, weight="bold")
for i, (y, _) in enumerate(led_rows):
    ln([24, 30], [y, y])
    txt(31, y, f"{2*i+1}B", ha="left", fs=9)
    txt(45, y, f"{2*i+1}C", ha="right", fs=9)
    ln([46, 50], [y, y])
    resistor(50, 58, y); txt(54, y+3, "220 Ω", fs=9)
    led(58, 72, y); txt(65, y-3.0, f"LED{i+1}", fs=9)
    ln([72, 76], [y, y]); dot(76, y)
# kytkentä: lähdöt eivät kuljeta virtaa tulojen läpi: input-vedot jo piirretty 24->30
# käyttämättömät tulot 2B,4B,6B,8B maahan
for i, y in enumerate((50, 42, 34, 26)):
    ln([30, 27], [y, y]); txt(31, y, f"{2*i+2}B", ha="left", fs=8, color="gray")
    ln([27, 27], [y, y-1.2]); gnd(27, y-1.2)
ln([33, 33], [24, 21.5]); txt(35, 22.5, "GND", fs=9, ha="left"); gnd(33, 21.5)
ln([43, 43], [58, 64]); txt(44, 60.5, "COM", fs=9, ha="left")

# +5V kisko
ln([76, 76], [30, 64], lw=3, c="#c00000")
ln([20, 76], [64, 64], c="#c00000", lw=3)
ln([24, 24], [60, 64], c="#c00000"); dot(24, 64)
dot(43, 64); txt(80, 64, "+5V (USB)", fs=11, weight="bold", color="#c00000", ha="left")

# --- napit ---
for i, (y, _) in enumerate(btn_rows):
    ln([24, 50], [y, y]) if False else None
    button(24, 62, y); txt(34, y+1.5, f"S{i+1}", fs=9)
    ln([62, 76], [y, y]); dot(76, y)
ln([76, 76], [0, 15], lw=3, c="#0044aa")
ln([76, 76], [0, -3], lw=3, c="#0044aa"); gnd(76, -3)
txt(80, 8, "GND", fs=11, weight="bold", color="#0044aa", ha="left")
# GND T-Display -> maahan
ln([24, 24], [-2.5, -5]); gnd(24, -5)
txt(50, -10, "Napit: active LOW (sisäinen pull-up), LEDit: active HIGH (GPIO HIGH → ULN johtaa → LED palaa). "
    "Käyttämättömät ULN-tulot 2B, 4B, 6B, 8B maahan.", fs=9)
txt(50, 67, "Nopeuspeli – piirikaavio", fs=15, weight="bold")
# 5V-pinnistä vedetty johto
fig.savefig("docs/piirikaavio.png", dpi=130, bbox_inches="tight", facecolor="white")
