# Helix.UI

The user interface of the Helix engine: a native C++ core (elements, layout, styles, markup, text) and the module that
puts it in the engine (assets, fonts, views drawn over the screen after the game's cameras, with any render pipeline
or none).

## Contents

| Path | Content |
|---|---|
| `Source/Helix/UI/Elements/` | The element tree: elements by stable handles in dense arrays, names, classes, pseudo states, dirty flags |
| `Source/Helix/UI/Layout/` | The flexbox layout: direction, wrap, grow, shrink, basis, alignment, absolute position, min and max sizes, margins, paddings, borders, percentages, aspect ratio; incremental, measure callbacks, subtrees on Helix.Jobs |
| `Source/Helix/UI/Styles/` | HSS style sheets: the parser, selectors matched right to left with an ancestor Bloom filter, specificity, the cascade, inheritance, `var()` variables (in shorthands too), transitions of every value of a property, `@import`, custom properties, the elements' transforms |
| `Source/Helix/UI/Markup/` | HXML markup: an XML parser, element types from a registry (those of script classes make their objects), style sheet references, inline styles, templates and their instances with attribute overrides and content containers |
| `Source/Helix/UI/Text/Unicode/` | Character properties from the Unicode Character Database (two stage tables written by `Tools/GenerateUnicode.py`) |
| `Source/Helix/UI/Text/Bidi/` | The bidirectional algorithm (UAX 9): levels, isolates, brackets, per line reordering |
| `Source/Helix/UI/Text/LineBreak/` | Line breaking (UAX 14) with CJK strictness |
| `Source/Helix/UI/Text/Shaping/` | The shaper: runs by script, level and font (fallback chains), OpenType GSUB and GPOS, Arabic joining forms |
| `Source/Helix/UI/Text/Fonts/` | Glyph atlases (SDF, MSDF, coverage, color glyphs), face metrics, the fonts of the machine |
| `Source/Helix/UI/Text/RichText/` | Rich text tags parsed into styles per character |
| `Source/Helix/UI/Text/Layout/` | Text layout: lines, alignment, overflow, auto size, and the text info (characters, words, lines, links, pages) |
| `Source/Helix/UI/World/` | The element world: one tree, layout, style engine and callback store per process; an element exists without a panel and keeps its callbacks and state when it moves between panels, which are roots in the world |
| `Source/Helix/UI/Events/` | Events: types from a registry with the reference's names and propagation, the dispatcher (trickle down, bubble up, stops, a queue for events sent while dispatching), picking through transforms, pointers and the mouse, click counts, capture, hover, focus and the focus ring, keys, navigation, the input method's composition, drag and drop |
| `Source/Helix/UI/Rendering/` | The geometry of a panel: rounded boxes with per side borders, background images (placed, sized and repeated as CSS backgrounds, 9-slice), glyphs, painted paths, clips that turn with their element, geometry through each element's transform, batches of four textures; the `Painter` (paths of lines, arcs and Bezier curves, filled or stroked with joins, caps and gradients, antialiased) |
| `Source/Helix.UI.Engine/` | The engine module HelixUI: `UITemplate` (.hxml), `StyleSheet` (.hss), `ThemeStyleSheet` (.tss) and `FontAsset` (.ttf, .otf, .ttc) assets, the `UIView` component and `UIPanelSettings`, the panels (shared by the views of one settings asset, scaled, sorted, drawn over the screen, into a render texture or in the world), `PanelInputConfiguration`, the image atlas, the cursor of the hovered element |
| `Source/Helix.UI.Engine/Helix/UI/Engine/Controls.h`, `Fields.h`, `Containers.h` | The controls as C++ classes (bound to C# and the C++ script API): `TextElement`, `Label`, `Button`, `RepeatButton`, `Image`, `Box`, `GroupBox`, `HelpBox`, `PopupWindow`, `ProgressBar`; the fields over `AbstractBaseField` (`Toggle`, `RadioButton`, `RadioButtonGroup`, `ToggleButtonGroup`, `Slider`, `SliderInt`, `MinMaxSlider`, `TextField`, the number fields, the vector, rect and bounds fields, `Hash128Field` (float fields show the float's own shortest text), `DropdownField`, `EnumField`, `MaskField`, `Mask64Field`); `Foldout`, `Scroller`, `ScrollView`, `TabView` and `Tab`, `TwoPaneSplitView` |
| `Themes/HelixDefault.tss` | Helix's default runtime theme (built into the module): the controls' looks by their `helix-` classes, its variables on the panel's root |
| `Source/Helix.UI.Engine/Helix/UI/Engine/Gui.h`, `GuiTypes.h` | The immediate GUI (namespace `HelixEngine`, one C++ implementation for C# and C++ scripts): `Gui`, `GuiLayout`, `GuiLayoutUtility`, `GuiUtility`, `GuiStyle` and `GuiStyleState`, `GuiSkin` (Helix's default skin, its images drawn by the UI), `GuiContent`, `RectOffset`, `GuiSettings`, `Event`, `TextEditor`; the passes of each `OnGui`, control ids, hot and keyboard controls, clips and groups, scroll views, windows, the layout solver |
| `Source/Helix.UI.Engine/Helix/UI/Engine/Element.h` | `CallbackEventHandler`, `Focusable`, `Element`, `TemplateContainer`, `VectorImage`: the elements for scripts (C# and the C++ script API), made when a script first needs them; an element lives while it is in a tree or a script holds it; the element types of C# and C++ script classes |
| `Source/Helix.UI.Runtime/` | The C# side (`Helix.UI.Runtime`, namespace `HelixEngine.UI`): the generated bindings and C# written by hand: `Element`'s hierarchy and queries (`Find`, `FindAll`), every event class over the panel's events, `RegisterCallback`, `SendEvent`, `IPanel`, `FocusController`, pointer and mouse capture, the scheduler, `IStyle` and `resolvedStyle` with their values (`Translate`, `Rotate`, `Scale`, `BackgroundSize`, `TextShadow`, `Cursor` ...), `customStyle`, `styleSheets`, `HxmlElement` and `HxmlAttribute`, `UITemplate.CloneTree`; the fields' generic bases (`BaseField<T>`, `BaseBoolField`, `BaseSlider<T>`, `TextInputBaseField<T>`, `TextValueField<T>`, `BaseCompositeField<,,>`, `BasePopupField<,>`, `PopupField<T>`, `BaseMaskField<T>`), `ChangeEvent<T>`, `InputEvent`, `INotifyValueChanged<T>`, `Clickable` and the manipulators, `RuntimePanelUtils`, `UIPanelSettings.SetScreenToPanelSpaceFunction`; the collections (`ListView`, `TreeView`, `MultiColumnListView`, `MultiColumnTreeView`, their controllers, `Columns`, sorting; a tree's drops onto an item or between items with the parent and index they go to), the menus (`DropdownMenu`, `GenericDropdownMenu` with submenus by path and a shortcut column, `ContextualMenuManipulator`), `ImguiContainer` (immediate GUI in an element's rectangle, drawn with the panel), `KeyboardNavigationManipulator`, the experimental animations (`ValueAnimation<T>`, `Easing`, `element.experimental.animation`), `generateVisualContent` with `MeshGenerationContext` and `Painter2D`, the data binding (`DataBinding`, `CustomBinding`, `ConverterGroups`) and `HelixEngine.Properties` (`PropertyPath`, `PropertyContainer`) |
| `Shaders/` | The UI's C# shader (`Hidden/UI/Default`: rounded boxes and borders from their distance, images, distance field, coverage and color glyphs, rounded clips, four texture slots) |
| `Tests/Core/` | Tests of the core, with every case of the Unicode bidi and line break tests |
| `Tests/Managed/` | C# tests run by the engine tests on the .NET host |
| `Tests/Api/Helix.UI.csv` | The public surface of the C# side (Helix.ScriptCore's inventory tool) |
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

```css
/* A theme (.tss): its variables at the root, the sheets of its look imported. */
@import url("Base.hss");
:root { --accent: #4f9dff; --panel: #1e222b; }

.tile { rotate: 15deg; scale: 1.2 0.9; translate: 0px -6px; transform-origin: left bottom;
        transition: rotate 0.3s ease-out, scale 0.3s ease-out; cursor: link; border-color: var(--accent); }
.tile:hover { rotate: 0deg; scale: 1; }
.badge { cursor: url("../Images/Hand.png") 4 2; --badge-size: 12px; }
```

Elements turn, scale and move about their transform origin as drawn and as picked (their descendants with them);
a clip turns with the element that clips. A theme imports sheets; custom properties of an element reach scripts
through `customStyle` (`CustomStyleResolvedEvent` tells them when they are resolved). The cursor of the element
under the mouse (a system shape or an image with its hotspot) goes to the engine's `Cursor`.

## Text

```xml
<Label class="body">Plain, &lt;b&gt;bold&lt;/b&gt; or <![CDATA[<color=#4f9dff>tagged</color>]]> text</Label>
```

```css
.body { -helix-font-definition: url("Fonts/Body.ttf"); -helix-font-family: "Segoe UI", "Microsoft YaHei";
        font-size: 16px; -helix-font-style: bold; -helix-text-align: upper-left; white-space: normal;
        text-overflow: ellipsis; -helix-text-overflow-position: middle; letter-spacing: 1px;
        text-shadow: 2px 2px 3px rgba(0, 0, 0, 0.8); -helix-text-outline: 1px #4f9dff; }
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
.photo { background-image: url("../Images/Photo.png"); background-size: cover; background-position: right center;
         -helix-background-image-tint-color: #ffffffc0; }
.stars { background-image: url("../Images/Star.png"); background-size: 16px 16px; background-repeat: repeat; }
.button { background-image: url("../Images/Frame.png"); -helix-slice-left: 16; -helix-slice-top: 16;
          -helix-slice-right: 16; -helix-slice-bottom: 16; -helix-slice-scale: 1; }
```

Boxes are quads whose pixels compute the distance of the rounded box, so corners and borders are antialiased at any
size; a border of four colors joins on the corners' diagonals. Background images are placed, sized (lengths, cover,
contain) and repeated (repeat, space, round) per axis as in CSS (`-helix-background-scale-mode` sets them as its three
modes do), or cut in nine parts, inside the rounded corners; readable textures of up to 256 pixels are copied into shared atlas pages, others draw
from their own texture. `overflow: hidden` clips the descendants to the rounded padding box (or the content box,
`-helix-overflow-clip-box`). Text takes shadows and outlines (distance field glyphs as layers under the glyph). Element types can paint
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

A `UIView` component names its markup asset (`m_Source`) and its `UIPanelSettings`. The views of one settings asset
share its panel (each view's root a child of the panel's, by the views' `sortingOrder`; a view under another view's
GameObject joins that one's panel); a view without settings has a panel of its own. A panel is laid out in the
screen's size divided by its scale (constant pixel size, constant physical size against `referenceDpi`, or scale
with screen size against `referenceResolution` and `match`) and drawn in the engine's screen overlay after the
cameras, with any render pipeline or none, by the settings' sorting order, or into the settings' `targetTexture`
(cleared as they say). Settings of `renderMode` `WorldSpace` put each view's panel in the scene: a panel pixel is a
unit of its GameObject's space, its `pivot` at the origin, its size its elements' or `worldSpaceSize`; the cameras
draw it with the scene's transparent objects, depth tested, and elements turned in 3D (`rotate: x 30deg`,
`rotate: 1 1 0 45deg`) show their depth. A pipeline of linear targets (the Physical pipeline sets `_HelixLinearOutput`) gets the panel's
colors made linear, as the scene's own. The views read the engine's input each frame without taking it away (a game
keeps its input; `UIView::Pick` tells it whether a point is over a view): pointers go to the top-most screen panel
with an element under them, else along the event cameras' rays to the nearest world space panel hit
(`PanelInputConfiguration`), or through a texture panel's `SetScreenToPanelSpaceFunction`. `RuntimePanelUtils`
maps screen and world points into a panel. Without a theme the panels take Helix's default theme. With `m_WatchFiles`
the markup and its style sheets are read again while the game runs and the page is built again when they change.

## Controls

```xml
<Toggle label="Shadows" value="true"/>
<Slider label="Volume" low-value="0" high-value="100" value="60" fill="true"/>
<TextField label="Name" value="Helix" max-length="16"/>
<DropdownField label="Renderer" choices="Forward, Deferred" index="0"/>
<TabView><Tab label="General"><Foldout text="More"><Label text="Inside"/></Foldout></Tab></TabView>
```

```csharp
var slider = new Slider("Volume", 0, 100);
slider.RegisterValueChangedCallback(e => progress.value = e.newValue);
var size = new PopupField<int>("Size", new List<int> { 8, 12, 16 }, 1);
var button = new Button(() => Debug.Log("clicked")) { text = "Go" };
```

The controls are C++ classes: markup makes their objects, which build their parts (each with its `helix-` class, the
theme's looks) and behave by callbacks on the world's events, so they work in any panel and without .NET. A field
keeps its value in its native slots; what the user changes reaches its C# object, which sends the `ChangeEvent<T>`
(a value set from C# sends it there). Text fields edit with the keyboard, the pointer (a double click selects a word,
a triple click the line), the clipboard (`GuiUtility.systemCopyBuffer`, the player's window) and the input method;
popup fields open their menu over the panel.

```cpp
// A C++ script (the C++ script API): the controls' typed values and their events.
namespace api = ::HelixEngine::UI;
api::Element root = GetComponent<api::UIView>().rootElement;
api::Slider speed = api::Slider::New();
speed.label = "Speed";
speed.value = 4.0f;
root.Find("settings").Add(speed);
speed.valueChanged.Add({[](void* self) { /* read the value */ }, this});
api::Button go = api::Button::New();
go.clicked.Add({[](void* self) { /* clicked */ }, this});
```

C++ scripts make the same controls through the C++ script API: `value` (Toggle, Slider, SliderInt, RadioButtonGroup,
TextField, IntegerField, FloatField), `index` (DropdownField), `text`, `label`, and the events `clicked` (Button,
RepeatButton), `valueChanged` (every field, Foldout, Scroller) and `activeTabChanged` (TabView); a value a script sets
is told like a change of the user's (C#'s ChangeEvent, the C++ scripts' event). `Find` finds an element by name.

## Collections, menus and animations

```xml
<ListView name="inventory" fixed-item-height="22" show-foldout-header="true" header-title="Inventory"
          show-add-remove-footer="true" reorderable="true" selection-type="Multiple"/>
<MultiColumnListView name="assets" sorting-mode="Default" show-alternating-row-backgrounds="All"/>
<Button text="Save" tooltip="Saves the file"/>
```

```csharp
list.makeItem = () => new Label();
list.bindItem = (e, i) => ((Label)e).text = items[i];
list.itemsSource = items;                                     // 10 000 items: rows only for the ones in view
tree.SetRootItems(new List<TreeViewItemData<string>> { new TreeViewItemData<string>(1, "Root", children) });
table.columns.Add(new Column { name = "size", title = "Size", comparison = (a, b) => sizes[a].CompareTo(sizes[b]) });
label.AddManipulator(new ContextualMenuManipulator(e => e.menu.AppendAction("Rename", _ => Rename())));
element.experimental.animation.Start(0f, 1f, 400, (e, v) => e.style.opacity = v).Ease(Easing.OutCubic);
```

The collection views are C# (their items, factories and binders are C# objects) over the native `ScrollView`: a pool
of rows for the items in view, of a fixed height or of each row's own (`DynamicHeight`), single or multiple
selection with the pointer and the keyboard, rows reordered by dragging, a list's foldout header and add/remove
footer, a tree's expansion, columns that sort, resize and hide from their header's menu. `ContextualMenuManipulator`
opens a `DropdownMenu` built by `ContextualMenuPopulateEvent` (a right click, the menu key or Shift+F10) as a
`GenericDropdownMenu`, whose items the keyboard walks; an element's `tooltip` shows after the mouse rests on it for
half a second. Animations run on the element's scheduler.

## Painting and data binding

```csharp
chart.generateVisualContent = ctx =>
{
    Painter2D p = ctx.painter2D;
    p.fillColor = Color.red;
    p.BeginPath(); p.MoveTo(new Vector2(0, 40)); p.LineTo(new Vector2(20, 0)); p.LineTo(new Vector2(40, 40));
    p.ClosePath(); p.Fill();
    ctx.DrawText("Score", new Vector2(4, 4), 12, Color.white);
};
page.dataSource = character;                                  // its children bind to its members by path
label.SetBinding("text", new DataBinding { dataSourcePath = new PropertyPath("stats.level"), bindingMode = BindingMode.ToTarget });
```

```xml
<Element data-source-path="stats">
  <IntegerField label="Strength">
    <Bindings><DataBinding property="value" data-source-path="strength" source-to-ui-converters="clamped"/></Bindings>
  </IntegerField>
</Element>
```

`generateVisualContent` draws into the panel's geometry while it is drawn: `painter2D` (paths of lines, arcs and
Bezier curves, filled by a fill rule or stroked with joins, caps and a gradient), meshes of `Allocate`, text and
`VectorImage`s (a painter's paths kept, `Painter2D.SaveToVectorImage`). A binding takes its data source from the
element or its ancestors (`dataSource`, `dataSourcePath`), reads and writes members along a `PropertyPath` by
reflection (public members, `[CreateProperty]`, `[SerializeField]`), converts through the global converters and
its `ConverterGroup`s, and updates when the source tells of a change (`INotifyBindablePropertyChanged`), when its
view hash changes or every frame; `TwoWay` bindings write the control's changes back.

## Immediate GUI

```csharp
void OnGui()
{
    if (Gui.Button(new Rect(10, 10, 120, 24), "Play"))
        StartGame();
    m_Volume = Gui.HorizontalSlider(new Rect(10, 40, 120, 20), m_Volume, 0, 1);
    m_Window = GuiLayout.Window(1, m_Window, id =>
    {
        m_Name = GuiLayout.TextField(m_Name);
        using (new GuiLayout.HorizontalScope())
        {
            GuiLayout.FlexibleSpace();
            if (GuiLayout.Button("OK", GuiLayout.Width(60))) Close();
        }
        Gui.DragWindow();
    }, "Settings");
}
```

```cpp
void OnGui()                                                  // a C++ script's, through the script API
{
    namespace api = ::HelixEngine;
    if (api::Gui::Button(::helix::Rect(10, 10, 120, 24), "Play"))
        StartGame();
    api::GuiLayout::BeginArea(::helix::Rect(10, 50, 200, 100));
    count_ = i32(api::GuiLayout::HorizontalSlider(f32(count_), 0, 10));
    api::GuiLayout::EndArea();
}
```

`OnGui` of every enabled script (C# and C++) runs once a pass: the frame's input events are sent before Update (the
player loop's IMGUISendQueuedEvents), each to the front-most script first (the smallest `Gui.depth`) until one uses
it (an event a script used does not reach the UI panels); Layout and Repaint run as the frame is drawn, the deepest
script first, and the GUI is drawn over the cameras after the UI panels, with any render pipeline or none. A script with `useGuiLayout` gets a Layout pass before each event:
`GuiLayout` records its controls and groups there (sizes from the styles, margins collapsed, options such as `Width`
and `ExpandHeight`, `FlexibleSpace` taking the room left) and the other passes get their rects in the same order.
Controls ask `GuiUtility.GetControlID` for ids that stay the same while the calls do; the held control
(`hotControl`) takes the mouse, the keyboard control the keys, tab moves through the text fields. Groups, clips and
scroll views move the origin and cut what is drawn (the mouse is in their space too); `Gui.matrix` scales and turns
the whole GUI. Windows run their function after the script's `OnGui`, take the clicks over them, come to the front
when clicked and move by `DragWindow`; a modal window takes all the input. The text controls edit through
`TextEditor` (selection, words, lines, the clipboard, undo). Styles draw their state's background 9-sliced and their
content (an image and text placed by the alignment); `Gui.skin` is Helix's default skin unless a script sets one.

## Element types of scripts

```csharp
[HxmlElement]
public class StatCard : Element
{
    [HxmlAttribute] public string title { get; set; }          // title="..."
    [HxmlAttribute("accent-color")] Color m_Accent;           // accent-color="#4f9dff"
    readonly Element m_Body = new Element();
    public override Element contentContainer => m_Body;      // the markup's children go here
    public StatCard() { hierarchy.Add(m_Body); }
}
```

```cpp
HX_CLASS(Namespace = "Game", HxmlElement)
class Gauge : public ::HelixEngine::UI::Element             // the script API's handle: this element
{
public:
    Gauge() { Add(::HelixEngine::UI::Element::New()); }
    HX_PROPERTY(HxmlAttribute) f32 value() const { return value_; }
    void SetValue(f32 v) { value_ = v; }                      // value="0.6" through the setter
    HX_FIELD(HxmlAttribute) Color tint;                       // tint="#3fd0a8"
private:
    f32 value_ = 0;
};
```

Markup makes elements of C# classes with `HxmlElement` (found when the engine registers an assembly) and of C++
classes of game modules deriving from the script API's `Element` (Helix.Bindgen registers them): the class's object
makes the element, then its attributes are set; its elements' type is the class's name (what type selectors match);
the markup's children go into its content container. A C++ class's object lives as long as its element.
`UITemplate.Instantiate` and `CloneTree` build a template from scripts; `Element.styleSheets` adds and removes sheets.

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
