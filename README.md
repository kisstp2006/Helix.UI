# Helix.UI

The user interface of the Helix engine: a native C++ core (elements, layout, styles, markup, text) and the module that
puts it in the engine (assets, fonts, views drawn over the screen after the game's cameras, with any render pipeline
or none).

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
| `Source/Helix/UI/Events/` | Events: types from a registry with the reference's names and propagation, the dispatcher (trickle down, bubble up, stops, a queue for events sent while dispatching), picking, pointers and the mouse, click counts, capture, hover, focus and the focus ring, keys, navigation, the input method's composition, drag and drop |
| `Source/Helix/UI/Rendering/` | The geometry of a panel: rounded boxes with per side borders, images (scale modes, 9-slice), glyphs, painted paths, clips, batches of four textures; the `Painter` (paths of lines, arcs and Bezier curves, filled or stroked with joins, caps and gradients, antialiased) |
| `Source/Helix.UI.Engine/` | The engine module HelixUI: `UITemplate` (.hxml), `StyleSheetAsset` (.hss) and `FontAsset` (.ttf, .otf, .ttc) assets, the `UIView` component, its drawer in the engine's screen overlay, the image atlas |
| `Shaders/` | The UI's C# shader (`Hidden/UI/Default`: rounded boxes and borders from their distance, images, distance field, coverage and color glyphs, rounded clips, four texture slots) |
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

## Drawing

```css
.card { border-radius: 14px; border-left-width: 3px; border-left-color: #4f9dff; overflow: hidden; }
.photo { background-image: url("../Images/Photo.png"); -helix-background-scale-mode: scale-and-crop;
         -helix-background-image-tint-color: #ffffffc0; }
.button { background-image: url("../Images/Frame.png"); -helix-slice-left: 16; -helix-slice-top: 16;
          -helix-slice-right: 16; -helix-slice-bottom: 16; -helix-slice-scale: 1; }
```

Boxes are quads whose pixels compute the distance of the rounded box, so corners and borders are antialiased at any
size; a border of four colors joins on the corners' diagonals. Images stretch, crop or fit, or are cut in nine parts,
inside the rounded corners; readable textures of up to 256 pixels are copied into shared atlas pages, others draw
from their own texture. `overflow: hidden` clips the descendants to the rounded padding box. Element types can paint
their content with vector paths: `RegisterElementType(name, attributes, paint)` gives a type a function that draws
with a `Painter` in the element's content box. Layout rectangles sit on the pixel grid. A panel draws with one shader
and as few draw calls as its textures need (four per call).

## Events

```cpp
EventSystem& events = view->panel()->events();
events.RegisterCallback(button, Events().click, [](void* user, EventSystem& events, UIEvent& e) {
    // e.target, e.currentTarget, e.position, e.clickCount ...
}, nullptr);
```

```xml
<Element name="field" focusable="true" tabindex="1" picking-mode="Position" enabled="true"/>
```

Each panel turns the input into the reference's events and order: a pointer's events go to the top-most pickable
element under it (or its capture), from the root down to it and back up; the mouse gets its Mouse events after its
Pointer events; a move sends Out, Leave, Over and Enter after it; a press and release on one element is a Click.
Keys go to the focused element (or the root); Tab, the arrows, Enter and Escape send navigation events after them,
whose default moves the focus around the ring (positive tab indices first, then tree order) or to the nearest element
that way. :hover, :focus and :disabled follow the state; :active is a control's. Element types take part through
`RegisterElementType(name, attributes, paint, attach)`: `attach` gives each element of the type its callbacks once a
panel is built. A focused text element asks for the input method with `SetTextInput` and gets `CompositionEvent`
while it composes.

A `UIView` component names its markup asset (`m_Source`); it lays the page out in the screen's size and draws it in
the engine's screen overlay after the cameras, with any render pipeline or none. The views read the engine's input
each frame without taking it away (a game keeps its input; `UIView::Pick` tells it whether a point is over a view). With `m_WatchFiles` the markup and
its style sheets are read again while the game runs and the page is built again when they change.

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
