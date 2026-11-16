"""Writes Source/Helix/UI/Text/Unicode/Implementation/UnicodeTables.inl from the Unicode Character Database files in
ThirdParty/Unicode: per code point a record of its bidi class, line break class, East Asian width, general category,
joining type, script and Extended_Pictographic, found through a two stage table (blocks of 128 code points, equal
blocks shared), and the sorted lists of bracket pairs and mirrored characters. Run it after updating the files:
python Tools/GenerateUnicode.py
"""

import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UCD = os.path.join(ROOT, "ThirdParty", "Unicode")
OUT = os.path.join(ROOT, "Source", "Helix", "UI", "Text", "Unicode", "Implementation", "UnicodeTables.inl")
COUNT = 0x110000
BLOCK = 128

# The orders of the enums in Unicode.h.
BIDI = ["L", "R", "AL", "EN", "ES", "ET", "AN", "CS", "NSM", "BN", "B", "S", "WS", "ON", "LRE", "LRO", "RLE", "RLO", "PDF",
        "LRI", "RLI", "FSI", "PDI"]
LINE_BREAK = ["BK", "CR", "LF", "CM", "NL", "SG", "WJ", "ZW", "GL", "SP", "ZWJ", "B2", "BA", "BB", "HY", "CB", "CL", "CP",
              "EX", "IN", "NS", "OP", "QU", "IS", "NU", "PO", "PR", "SY", "AI", "AK", "AL", "AP", "AS", "CJ", "EB", "EM",
              "H2", "H3", "HL", "HH", "ID", "JL", "JV", "JT", "RI", "SA", "VF", "VI", "XX"]
WIDTH = ["N", "A", "H", "W", "F", "Na"]
CATEGORY = ["Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Mc", "Me", "Nd", "Nl", "No", "Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po",
            "Sm", "Sc", "Sk", "So", "Zs", "Zl", "Zp", "Cc", "Cf", "Cs", "Co", "Cn"]
JOINING = ["U", "R", "L", "D", "C", "T"]


def lines(name):
    with open(os.path.join(UCD, name), encoding="utf-8") as f:
        for line in f:
            yield line.rstrip("\n")


def version():
    for line in lines("LineBreak.txt"):
        m = re.match(r"# LineBreak-(\d+\.\d+\.\d+)\.txt", line)
        if m:
            return m.group(1)
    return "?"


def span(text):
    a, _, b = text.strip().partition("..")
    return int(a, 16), int(b or a, 16)


def read(name, values, aliases=None, field=1):
    """A property of every code point: the @missing defaults in order, then the listed ranges."""
    out = [None] * COUNT
    for line in lines(name):
        m = re.match(r"# @missing: ([0-9A-F.]+); *([^#;]+)", line)
        if m:
            a, b = span(m.group(1))
            v = m.group(2).strip()
            v = aliases.get(v, v) if aliases else v
            out[a:b + 1] = [values.index(v)] * (b - a + 1)
            continue
        line = line.split("#")[0].strip()
        if not line:
            continue
        parts = [p.strip() for p in line.split(";")]
        a, b = span(parts[0])
        v = parts[field]
        v = aliases.get(v, v) if aliases else v
        out[a:b + 1] = [values.index(v)] * (b - a + 1)
    return out


def aliases(prop):
    """Long names to short ones of a property in PropertyValueAliases.txt, and the short names in order."""
    long_to_short, shorts = {}, []
    for line in lines("PropertyValueAliases.txt"):
        line = line.split("#")[0].strip()
        parts = [p.strip() for p in line.split(";")]
        if len(parts) >= 3 and parts[0] == prop:
            shorts.append(parts[1])
            for p in parts[2:]:
                long_to_short[p] = parts[1]
    return long_to_short, shorts


def main():
    bidi_alias, _ = aliases("bc")
    bidi = read("DerivedBidiClass.txt", BIDI, bidi_alias)
    line_break = read("LineBreak.txt", LINE_BREAK)
    width = read("EastAsianWidth.txt", WIDTH)
    category = read("DerivedGeneralCategory.txt", CATEGORY)
    script_alias, scripts = aliases("sc")
    # Common first, so an unlisted code point (Unknown) is distinct from it.
    scripts = sorted(set(scripts), key=lambda s: (s != "Zyyy", s != "Zinh", s != "Zzzz", s))
    script = [scripts.index("Zzzz")] * COUNT
    for line in lines("Scripts.txt"):
        line = line.split("#")[0].strip()
        if line:
            r, name = [p.strip() for p in line.split(";")]
            a, b = span(r)
            script[a:b + 1] = [scripts.index(script_alias.get(name, name))] * (b - a + 1)
    # Joining types: listed ones, else T for Mn, Me and Cf, else U.
    joining = [JOINING.index("T") if CATEGORY[c] in ("Mn", "Me", "Cf") else JOINING.index("U") for c in category]
    for line in lines("ArabicShaping.txt"):
        line = line.split("#")[0].strip()
        if line:
            parts = [p.strip() for p in line.split(";")]
            joining[int(parts[0], 16)] = JOINING.index(parts[2])
    pictographic = [0] * COUNT
    for line in lines("emoji-data.txt"):
        line = line.split("#")[0].strip()
        if line and line.split(";")[1].strip() == "Extended_Pictographic":
            a, b = span(line.split(";")[0])
            pictographic[a:b + 1] = [1] * (b - a + 1)

    records, index = [], {}
    per_cp = []
    for c in range(COUNT):
        r = (bidi[c], line_break[c], width[c], category[c], joining[c], script[c], pictographic[c])
        if r not in index:
            index[r] = len(records)
            records.append(r)
        per_cp.append(index[r])
    blocks, block_index, stage1 = [], {}, []
    for start in range(0, COUNT, BLOCK):
        b = tuple(per_cp[start:start + BLOCK])
        if b not in block_index:
            block_index[b] = len(blocks)
            blocks.append(b)
        stage1.append(block_index[b])

    brackets = []
    for line in lines("BidiBrackets.txt"):
        line = line.split("#")[0].strip()
        if line:
            a, pair, kind = [p.strip() for p in line.split(";")]
            brackets.append((int(a, 16), int(pair, 16), 0 if kind == "o" else 1))
    mirrors = []
    for line in lines("BidiMirroring.txt"):
        line = line.split("#")[0].strip()
        if line:
            a, b = [p.strip() for p in line.split(";")]
            mirrors.append((int(a, 16), int(b, 16)))

    out = [f"// Generated by Tools/GenerateUnicode.py from the Unicode Character Database {version()}.",
           "// Copyright 1991-2026 Unicode, Inc. Under the Unicode License v3, see ThirdParty/Unicode/LICENSE.txt.",
           "// clang-format off", ""]
    out.append(f"constexpr u32 kUnicodeBlock = {BLOCK};")
    out.append(f"constexpr const char* kUnicodeVersion = \"{version()}\";")
    out.append(f"constexpr u32 kScriptCount = {len(scripts)};")
    out.append("constexpr char kScriptCodes[][5] = {" + ", ".join(f'"{s}"' for s in scripts) + "};")
    out.append("constexpr CharProperties kRecords[] = {")
    for r in records:
        out.append(f"    {{BidiClass({r[0]}), LineBreakClass({r[1]}), EastAsianWidth({r[2]}), GeneralCategory({r[3]}), "
                   f"JoiningType({r[4]}), {r[5]}, {r[6]}}},")
    out.append("};")
    out.append(f"constexpr u16 kStage1[{len(stage1)}] = {{")
    for i in range(0, len(stage1), 32):
        out.append("    " + ", ".join(str(v) for v in stage1[i:i + 32]) + ",")
    out.append("};")
    out.append(f"constexpr u16 kStage2[{len(blocks) * BLOCK}] = {{")
    for b in blocks:
        for i in range(0, BLOCK, 32):
            out.append("    " + ", ".join(str(v) for v in b[i:i + 32]) + ",")
    out.append("};")
    out.append(f"constexpr BracketEntry kBrackets[{len(brackets)}] = {{")
    out += [f"    {{0x{a:X}, 0x{p:X}, BracketType({k + 1})}}," for a, p, k in brackets]
    out.append("};")
    out.append(f"constexpr MirrorEntry kMirrors[{len(mirrors)}] = {{")
    out += [f"    {{0x{a:X}, 0x{b:X}}}," for a, b in mirrors]
    out.append("};")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print(f"{len(records)} records, {len(blocks)} blocks, {len(scripts)} scripts, {len(brackets)} brackets, {len(mirrors)} mirrors")


if __name__ == "__main__":
    main()
