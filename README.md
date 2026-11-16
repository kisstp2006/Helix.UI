# Helix.UI

The user interface of the Helix engine: a native C++ core (elements, layout, styles, markup) and the module that puts
it in the engine (assets, views drawn over the game's cameras in both render pipelines).

## Contents

| Path | Content |
|---|---|
| `Source/Helix/UI/Elements/` | The element tree: elements by stable handles in dense arrays, names, classes, pseudo states, dirty flags |
| `Source/Helix/UI/Layout/` | The flexbox layout: direction, wrap, grow, shrink, basis, alignment, absolute position, min and max sizes, margins, paddings, borders, percentages, aspect ratio; incremental, measure callbacks, subtrees on Helix.Jobs |
| `Source/Helix/UI/Styles/` | HSS style sheets: the parser, selectors matched right to left with an ancestor Bloom filter, specificity, the cascade, inheritance, `var()` variables, transitions |
| `Source/Helix/UI/Markup/` | HXML markup: an XML parser, element types from a registry, style sheet references, inline styles, templates and their instances with attribute overrides |
| `Source/Helix.UI.Engine/` | The engine module HelixUI: `UITemplate` (.hxml) and `StyleSheetAsset` (.hss) assets, the `UIView` component and its drawer |
| `Shaders/` | The UI's C# shaders (`Hidden/UI/Flat`) |
| `Tests/Core/` | Tests of the core |
| `Tests/Engine/` | The assets and views in the engine, drawn on every device through a test pipeline (golden images in `Tests/Golden/`) |
| `Benchmarks/` | The element store and the layout of 10 000 elements, full and incremental |

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

GPL-3.0, see `LICENSE`.
