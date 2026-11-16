# Helix.UI

The user interface of the Helix engine: a native C++ core (elements, layout, styles, markup, text) and the module that
puts it in the engine (assets, fonts, views drawn over the game's cameras in both render pipelines).

## Contents

| Path | Content |
|---|---|
| `Source/Helix/UI/Elements/` | The element tree: elements by stable handles in dense arrays, names, classes, pseudo states, dirty flags |
| `Source/Helix/UI/Layout/` | The flexbox layout: direction, wrap, grow, shrink, basis, alignment, absolute position, min and max sizes, margins, paddings, borders, percentages, aspect ratio; incremental, measure callbacks, subtrees on Helix.Jobs |
| `Source/Helix/UI/Styles/` | HSS style sheets: the parser, selectors matched right to left with an ancestor Bloom filter, specificity, the cascade, inheritance, `var()` variables, transitions |
| `Source/Helix/UI/Markup/` | HXML markup: an XML parser, element types from a registry, style sheet references, inline styles, templates and their instances with attribute overrides |
| `Source/Helix/UI/Text/Unicode/` | Character properties from the Unicode Character Database (two stage tables written by `Tools/GenerateUnicode.py`) |
| `Source/Helix/UI/Text/Bidi/` | The bidirectional algorithm (UAX 9): levels, isolates, brackets, per line reordering |
| `Source/Helix/UI/Text/LineBreak/` | Line breaking (UAX 14) with CJK strictness |
| `Source/Helix/UI/Text/Shaping/` | The shaper: runs by script, level and font (fallback chains), OpenType GSUB and GPOS, Arabic joining forms |
| `Source/Helix/UI/Text/Fonts/` | Glyph atlases (SDF, MSDF, coverage, color glyphs), face metrics, the fonts of the machine |
| `Source/Helix/UI/Text/RichText/` | Rich text tags parsed into styles per character |
| `Source/Helix/UI/Text/Layout/` | Text layout: lines, alignment, overflow, auto size, and the text info (characters, words, lines, links, pages) |
| `Source/Helix.UI.Engine/` | The engine module HelixUI: `UITemplate` (.hxml), `StyleSheetAsset` (.hss) and `FontAsset` (.ttf, .otf, .ttc) assets, the `UIView` component and its drawer |
| `Shaders/` | The UI's C# shader (`Hidden/UI/Default`: flat quads, distance field, coverage and color glyphs) |
| `Tests/Core/` | Tests of the core, with every case of the Unicode bidi and line break tests |
| `Tests/Engine/` | The assets and views in the engine, drawn on every device through a test pipeline (golden images in `Tests/Golden/`) |
| `Benchmarks/` | The element store, the layout of 10 000 elements (full and incremental), a page of text |
| `ThirdParty/Unicode/` | Files of the Unicode Character Database, see `THIRD-PARTY.txt` |

## Markup and styles

```xml
<HXML>
  <Style src="Page.hss"/>
  <Template name="Card" src="Card.hxml"/>
  <Element name="content" class="row">
    <Instance template="Card" class="warm"/>
  </Element>
</HXML>
```

```css
#content { flex-direction: row; flex-wrap: wrap; --accent: #4f9dff; }
.warm .picture { background-color: var(--accent); aspect-ratio: 2; }
```

## Text

```xml
<Label class="body">Plain, &lt;b&gt;bold&lt;/b&gt; or <![CDATA[<color=#4f9dff>tagged</color>]]> text</Label>
```

```css
.body { -helix-font-definition: url("Fonts/Body.ttf"); -helix-font-family: "Segoe UI", "Microsoft YaHei";
        font-size: 16px; -helix-font-style: bold; -helix-text-align: upper-left; white-space: normal;
        text-overflow: ellipsis; letter-spacing: 1px; }
```

Text is shaped with the fonts' OpenType tables (ligatures, kerning, marks, cursive attachment, Arabic forms), split by
script, direction and font (a font's fallbacks, then the families of the style, then the platform's default families
of the machine), ordered by the Unicode bidirectional algorithm, broken into lines by the Unicode line breaking rules,
and drawn from glyph atlases filled as glyphs are first used. Rich text takes the tags b, i, u, s, color, alpha, size,
font, align, mark, sub, sup, line-height, cspace, mspace, indent, margin, voffset, space, pos, nobr, noparse, br,
lowercase, uppercase, smallcaps, link, style, sprite, gradient, width, rotate and page.

A `UIView` component names its markup asset (`m_Source`); it lays the page out in the screen's size and draws it after
each game camera. With `m_WatchFiles` the markup and its style sheets are read again while the game runs and the page
is built again when they change.

## Build

```
git clone https://github.com/kisstp2006/Helix.UI
git -C Helix.UI submodule update --init
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

The .NET 11 SDK is needed for the shaders (`HELIX_DOTNET_ROOT` or `DOTNET_ROOT`).

## License

GPL-3.0, see `LICENSE`. The Unicode data files are under the Unicode License v3, see `THIRD-PARTY.txt`.
