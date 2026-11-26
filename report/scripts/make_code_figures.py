"""Renders the code listings used in the report's Implementation section.

Each listing is an exact excerpt of a project source file (real line numbers). Lines
where a PDC concept is applied are highlighted. Output: report/figures/code/*.png

Run:  D:\\Python\\python.exe report\\scripts\\make_code_figures.py
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from pygments.lexers import CppLexer
from pygments.token import Comment, Keyword, Name, Number, Operator, String, Token

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "report" / "figures" / "code"
OUT.mkdir(parents=True, exist_ok=True)

FONTS = Path("C:/Windows/Fonts")
S = 2  # supersampling factor
CODE = ImageFont.truetype(str(FONTS / "consola.ttf"), 15 * S)
CODE_I = ImageFont.truetype(str(FONTS / "consolai.ttf"), 15 * S)
HEAD = ImageFont.truetype(str(FONTS / "segoeuib.ttf"), 14 * S)
HEAD2 = ImageFont.truetype(str(FONTS / "segoeui.ttf"), 13 * S)

BG, BORDER, HEAD_BG = "#ffffff", "#d0d7de", "#f6f8fa"
GUTTER_FG, HL_BG, HL_BAR = "#8c959f", "#eaf2fd", "#257de0"
COLORS = [  # (token type, colour, italic)
    (Comment.Preproc, "#8250df", False), (Comment.PreprocFile, "#0a3069", False),
    (Comment, "#6e7781", True), (String, "#0a3069", False), (Number, "#0550ae", False),
    (Keyword.Type, "#cf222e", False), (Keyword, "#cf222e", False),
    (Name.Builtin, "#953800", False), (Operator, "#24292f", False),
]
MPI_OMP_NAMES = ("MPI_", "omp_")  # highlight MPI / OpenMP API names in a distinct colour


def style(ttype, value):
    if any(value.startswith(p) for p in MPI_OMP_NAMES):
        return "#0550ae", False
    for t, c, it in COLORS:
        if ttype in t:
            return c, it
    return "#24292f", False


def tokenize_lines(path):
    """Token list per line of the whole file (so multi-line comments are lexed correctly)."""
    text = path.read_text(encoding="utf-8")
    lines, cur = [], []
    for ttype, value in CppLexer(stripnl=False, ensurenl=True).get_tokens(text):
        parts = value.split("\n")
        for k, part in enumerate(parts):
            if part:
                cur.append((ttype, part))
            if k < len(parts) - 1:
                lines.append(cur)
                cur = []
    if cur:
        lines.append(cur)
    return lines


def render(name, rel_path, ranges, highlight, title):
    src = ROOT / rel_path
    toks = tokenize_lines(src)
    rows = []  # (line number or None for a gap, tokens)
    for idx, (a, b) in enumerate(ranges):
        if idx > 0:
            rows.append((None, []))
        for n in range(a, b + 1):
            rows.append((n, toks[n - 1]))

    # Remove the common indentation of the excerpt so it doesn't waste width.
    def indent(tl):
        text = "".join(v for _, v in tl)
        return len(text) - len(text.lstrip(" ")) if text.strip() else 999
    dedent = min(indent(t) for n, t in rows if n is not None)
    dedent = 0 if dedent == 999 else dedent

    def line_text(tl):
        return "".join(v for _, v in tl)[dedent:]

    cw = CODE.getlength("M")
    lh = int(15 * S * 1.42)
    max_chars = max(len(line_text(t)) for n, t in rows if n is not None)
    gutter = int(cw * (len(str(ranges[-1][1])) + 2.5))
    pad = 16 * S
    head_h = 34 * S
    width = int(gutter + pad + max_chars * cw + pad)
    width = max(width, int(HEAD.getlength(title) + HEAD2.getlength(rel_path) + 80 * S))
    height = head_h + pad // 2 + lh * len(rows) + pad // 2

    img = Image.new("RGB", (width, height), BG)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, width - 1, head_h], fill=HEAD_BG)
    d.line([0, head_h, width, head_h], fill=BORDER, width=S)
    d.text((pad, head_h / 2), title, font=HEAD, fill="#1f2328", anchor="lm")
    d.text((width - pad, head_h / 2), rel_path.replace("\\", "/"), font=HEAD2, fill="#59636e", anchor="rm")

    y = head_h + pad // 2
    for n, tl in rows:
        if n is None:
            cx, r = gutter - cw * 2, 2 * S  # vertical ellipsis drawn as three dots
            for k in (-1, 0, 1):
                cy = y + lh / 2 + k * 7 * S
                d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=GUTTER_FG)
            d.line([gutter + pad / 2, y + lh / 2, width - pad, y + lh / 2], fill="#eaeef2", width=S)
            y += lh
            continue
        if n in highlight:
            d.rectangle([0, y, width, y + lh - 1], fill=HL_BG)
            d.rectangle([0, y, 3 * S, y + lh - 1], fill=HL_BAR)
        d.text((gutter - cw * 1.2, y + lh / 2), str(n), font=CODE, fill=GUTTER_FG, anchor="rm")
        x = gutter + pad / 2
        skip = dedent
        for ttype, value in tl:
            if skip:
                cut = min(skip, len(value) - len(value.lstrip(" ")))
                value, skip = value[cut:], skip - cut
                if not value:
                    continue
                skip = 0
            color, italic = style(ttype, value)
            f = CODE_I if italic else CODE
            d.text((x, y + lh / 2), value, font=f, fill=color, anchor="lm")
            x += f.getlength(value)
        y += lh

    framed = Image.new("RGB", (width + 2 * S, height + 2 * S), BORDER)
    framed.paste(img, (S, S))
    framed.save(OUT / f"{name}.png", dpi=(300, 300))
    print(f"wrote {name}.png  ({max_chars} chars wide)")


# ---- The listings ------------------------------------------------------------------------
render("listing1_convolution", "src/common/convolution.cpp", [(29, 58)], set(),
       "Shared convolution kernel (used unchanged by every version)")
render("listing2_omp_rows", "src/openmp/main.cpp", [(38, 52)], {49, 50, 51, 52},
       "OpenMP rows mode: threads share the rows of one image")
render("listing3_omp_images", "src/openmp/main.cpp", [(83, 101), (108, 119)], {86, 87, 110, 111, 115, 116},
       "OpenMP images mode: reduction, critical section and atomic update")
render("listing4_mpi_batch", "src/mpi_batch/main.cpp", [(38, 42), (55, 57), (83, 84), (90, 91), (101, 102), (135, 138)],
       {39, 56, 57, 78, 83, 91, 101, 137},
       "MPI batch + hybrid: broadcast, block partition, OpenMP inside, reduce")
render("listing5_partition", "src/common/partition.hpp", [(14, 22)], {19, 20},
       "Block partitioning used by both MPI programs")
render("listing6_mpi_strips", "src/mpi_strips/main.cpp", [(133, 154), (164, 168), (174, 176)],
       {143, 144, 148, 149, 150, 152, 153, 164, 175, 176},
       "MPI strips: Scatterv, halo exchange with Sendrecv, Gatherv")
