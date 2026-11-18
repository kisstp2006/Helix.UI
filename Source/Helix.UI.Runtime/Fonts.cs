using System;
using System.Collections.Generic;
using HelixEngine.Text.LowLevel;

namespace HelixEngine.Text
{
    public partial struct GlyphRect : IEquatable<GlyphRect>
    {
        public int x { get => m_X; set => m_X = value; }
        public int y { get => m_Y; set => m_Y = value; }
        public int width { get => m_Width; set => m_Width = value; }
        public int height { get => m_Height; set => m_Height = value; }

        public GlyphRect(int x, int y, int width, int height)
        {
            m_X = x;
            m_Y = y;
            m_Width = width;
            m_Height = height;
        }

        public GlyphRect(Rect rect) : this((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height)
        {
        }

        public static GlyphRect zero => new GlyphRect(0, 0, 0, 0);

        public bool Equals(GlyphRect other) => x == other.x && y == other.y && width == other.width && height == other.height;
        public override bool Equals(object obj) => obj is GlyphRect r && Equals(r);
        public override int GetHashCode() => HashCode.Combine(x, y, width, height);
        public static bool operator ==(GlyphRect lhs, GlyphRect rhs) => lhs.Equals(rhs);
        public static bool operator !=(GlyphRect lhs, GlyphRect rhs) => !lhs.Equals(rhs);
    }

    public partial struct GlyphMetrics : IEquatable<GlyphMetrics>
    {
        public float width { get => m_Width; set => m_Width = value; }
        public float height { get => m_Height; set => m_Height = value; }
        public float horizontalBearingX { get => m_HorizontalBearingX; set => m_HorizontalBearingX = value; }
        public float horizontalBearingY { get => m_HorizontalBearingY; set => m_HorizontalBearingY = value; }
        public float horizontalAdvance { get => m_HorizontalAdvance; set => m_HorizontalAdvance = value; }

        public GlyphMetrics(float width, float height, float bearingX, float bearingY, float advance)
        {
            m_Width = width;
            m_Height = height;
            m_HorizontalBearingX = bearingX;
            m_HorizontalBearingY = bearingY;
            m_HorizontalAdvance = advance;
        }

        public bool Equals(GlyphMetrics other) =>
            width == other.width && height == other.height && horizontalBearingX == other.horizontalBearingX && horizontalBearingY == other.horizontalBearingY &&
            horizontalAdvance == other.horizontalAdvance;
        public override bool Equals(object obj) => obj is GlyphMetrics m && Equals(m);
        public override int GetHashCode() => HashCode.Combine(width, height, horizontalBearingX, horizontalBearingY, horizontalAdvance);
        public static bool operator ==(GlyphMetrics lhs, GlyphMetrics rhs) => lhs.Equals(rhs);
        public static bool operator !=(GlyphMetrics lhs, GlyphMetrics rhs) => !lhs.Equals(rhs);
    }

    /// <summary>A glyph of a font asset: its index in the face, metrics and place in the atlases.</summary>
    [Serializable]
    public class Glyph
    {
        public uint index { get; set; }
        public GlyphMetrics metrics { get; set; }
        public GlyphRect glyphRect { get; set; }
        public float scale { get; set; } = 1;
        public int atlasIndex { get; set; }
        public GlyphClassDefinitionType classDefinitionType { get; set; }

        public Glyph()
        {
        }

        public Glyph(Glyph glyph)
        {
            index = glyph.index;
            metrics = glyph.metrics;
            glyphRect = glyph.glyphRect;
            scale = glyph.scale;
            atlasIndex = glyph.atlasIndex;
            classDefinitionType = glyph.classDefinitionType;
        }

        public Glyph(uint index, GlyphMetrics metrics, GlyphRect glyphRect) : this(index, metrics, glyphRect, 1, 0)
        {
        }

        public Glyph(uint index, GlyphMetrics metrics, GlyphRect glyphRect, float scale, int atlasIndex)
        {
            this.index = index;
            this.metrics = metrics;
            this.glyphRect = glyphRect;
            this.scale = scale;
            this.atlasIndex = atlasIndex;
        }

        public bool Compare(Glyph other) => other != null && index == other.index && metrics == other.metrics && glyphRect == other.glyphRect;
    }

    public enum GlyphClassDefinitionType
    {
        Undefined = 0,
        Base = 1,
        Ligature = 2,
        Mark = 3,
        Component = 4,
    }

    /// <summary>The base of what a text draws: characters of a font and sprites of a sprite asset.</summary>
    [Serializable]
    public abstract class TextElement
    {
        public TextElementType elementType { get; protected set; }
        public uint unicode { get; set; }
        public TextAsset textAsset { get; set; }
        public Glyph glyph { get; set; }
        public uint glyphIndex { get; set; }
        public float scale { get; set; } = 1;
    }

    /// <summary>A character of a font asset: its code point and its glyph.</summary>
    [Serializable]
    public class Character : TextElement
    {
        public Character()
        {
            elementType = TextElementType.Character;
        }

        public Character(uint unicode, Glyph glyph) : this()
        {
            this.unicode = unicode;
            this.glyph = glyph;
            glyphIndex = glyph?.index ?? 0;
        }

        public Character(uint unicode, FontAsset fontAsset, Glyph glyph) : this(unicode, glyph)
        {
            textAsset = fontAsset;
        }

        public Character(uint unicode, uint glyphIndex) : this()
        {
            this.unicode = unicode;
            this.glyphIndex = glyphIndex;
        }
    }

    /// <summary>The face's lines at a point size.</summary>
    [Serializable]
    public struct FaceInfo
    {
        public int faceIndex { get; set; }
        public string familyName { get; set; }
        public string styleName { get; set; }
        public float pointSize { get; set; }
        public float scale { get; set; }
        public int unitsPerEM { get; set; }
        public float lineHeight { get; set; }
        public float ascentLine { get; set; }
        public float capLine { get; set; }
        public float meanLine { get; set; }
        public float baseline { get; set; }
        public float descentLine { get; set; }
        public float superscriptOffset { get; set; }
        public float superscriptSize { get; set; }
        public float subscriptOffset { get; set; }
        public float subscriptSize { get; set; }
        public float underlineOffset { get; set; }
        public float underlineThickness { get; set; }
        public float strikethroughOffset { get; set; }
        public float strikethroughThickness { get; set; }
        public float tabWidth { get; set; }

        public bool Compare(FaceInfo other) =>
            familyName == other.familyName && styleName == other.styleName && pointSize == other.pointSize && lineHeight == other.lineHeight &&
            ascentLine == other.ascentLine && descentLine == other.descentLine;
    }

    [Serializable]
    public struct FontWeightPair
    {
        public FontAsset regularTypeface;
        public FontAsset italicTypeface;
    }

    /// <summary>How the asset was made (the creation window's values).</summary>
    [Serializable]
    public struct FontAssetCreationSettings
    {
        public string sourceFontFileName;
        public string sourceFontFileGUID;
        public int faceIndex;
        public int pointSizeSamplingMode;
        public float pointSize;
        public int padding;
        public int paddingMode;
        public int packingMode;
        public int atlasWidth;
        public int atlasHeight;
        public int characterSetSelectionMode;
        public string characterSequence;
        public string referencedFontAssetGUID;
        public string referencedTextAssetGUID;
        public int fontStyle;
        public float fontStyleModifier;
        public int renderMode;
        public bool includeFontFeatures;
    }

    /// <summary>The OpenType features of a font asset (its adjustment and substitution records).</summary>
    [Serializable]
    public class FontFeatureTable
    {
        public List<GlyphPairAdjustmentRecord> glyphPairAdjustmentRecords { get; set; } = new List<GlyphPairAdjustmentRecord>();
        public List<MultipleSubstitutionRecord> multipleSubstitutionRecords { get; set; } = new List<MultipleSubstitutionRecord>();
        public List<LigatureSubstitutionRecord> ligatureRecords { get; set; } = new List<LigatureSubstitutionRecord>();
        public List<MarkToBaseAdjustmentRecord> markToBaseAdjustmentRecords { get; set; } = new List<MarkToBaseAdjustmentRecord>();
        public List<MarkToMarkAdjustmentRecord> markToMarkAdjustmentRecords { get; set; } = new List<MarkToMarkAdjustmentRecord>();

        public void SortGlyphPairAdjustmentRecords() =>
            glyphPairAdjustmentRecords.Sort((a, b) =>
                a.firstAdjustmentRecord.glyphIndex != b.firstAdjustmentRecord.glyphIndex
                    ? a.firstAdjustmentRecord.glyphIndex.CompareTo(b.firstAdjustmentRecord.glyphIndex)
                    : a.secondAdjustmentRecord.glyphIndex.CompareTo(b.secondAdjustmentRecord.glyphIndex));
    }

    [Serializable]
    public struct MultipleSubstitutionRecord
    {
        public uint targetGlyphID { get; set; }
        public uint[] substituteGlyphIDs { get; set; }
    }

    [Serializable]
    public struct LigatureSubstitutionRecord
    {
        public uint[] componentGlyphIDs { get; set; }
        public uint ligatureGlyphID { get; set; }
    }

    [Serializable]
    public struct GlyphAnchorPoint
    {
        public float xCoordinate { get; set; }
        public float yCoordinate { get; set; }
    }

    [Serializable]
    public struct MarkPositionAdjustment
    {
        public float xPositionAdjustment { get; set; }
        public float yPositionAdjustment { get; set; }
    }

    [Serializable]
    public struct MarkToBaseAdjustmentRecord
    {
        public uint baseGlyphID { get; set; }
        public GlyphAnchorPoint baseGlyphAnchorPoint { get; set; }
        public uint markGlyphID { get; set; }
        public MarkPositionAdjustment markPositionAdjustment { get; set; }
    }

    [Serializable]
    public struct MarkToMarkAdjustmentRecord
    {
        public uint baseMarkGlyphID { get; set; }
        public GlyphAnchorPoint baseMarkGlyphAnchorPoint { get; set; }
        public uint combiningMarkGlyphID { get; set; }
        public MarkPositionAdjustment combiningMarkPositionAdjustment { get; set; }
    }

    public partial class FontAsset
    {
        Dictionary<uint, Character> m_CharacterLookup;
        Dictionary<uint, Glyph> m_GlyphLookup;
        List<Character> m_Characters;
        List<Glyph> m_Glyphs;
        readonly FontFeatureTable m_FontFeatureTable = new FontFeatureTable();
        FontAssetCreationSettings m_CreationSettings;

        /// <summary>The face's lines at the sampling size.</summary>
        public FaceInfo faceInfo
        {
            get
            {
                FaceInfoData d = FaceInfoInternal();
                return new FaceInfo
                {
                    familyName = FamilyNameInternal(),
                    styleName = StyleNameInternal(),
                    pointSize = d.pointSize,
                    scale = d.scale,
                    unitsPerEM = d.unitsPerEM,
                    lineHeight = d.lineHeight,
                    ascentLine = d.ascentLine,
                    capLine = d.capLine,
                    meanLine = d.meanLine,
                    baseline = d.baseline,
                    descentLine = d.descentLine,
                    superscriptOffset = d.superscriptOffset,
                    superscriptSize = d.superscriptSize,
                    subscriptOffset = d.subscriptOffset,
                    subscriptSize = d.subscriptSize,
                    underlineOffset = d.underlineOffset,
                    underlineThickness = d.underlineThickness,
                    strikethroughOffset = d.strikethroughOffset,
                    strikethroughThickness = d.strikethroughThickness,
                    tabWidth = d.tabWidth,
                };
            }
            set { }
        }

        public GlyphRenderMode atlasRenderMode
        {
            get => AtlasRenderModeInternal() switch
            {
                1 => GlyphRenderMode.SDFAA,
                2 => GlyphRenderMode.SMOOTH,
                _ => GlyphRenderMode.SDFAA,
            };
            set => SetAtlasRenderModeInternal(value switch
            {
                GlyphRenderMode.SMOOTH or GlyphRenderMode.SMOOTH_HINTED or GlyphRenderMode.RASTER or GlyphRenderMode.RASTER_HINTED or GlyphRenderMode.COLOR or
                    GlyphRenderMode.COLOR_HINTED => 2,
                _ => 0,
            });
        }

        public Texture2D[] atlasTextures
        {
            get => AtlasTexturesInternal();
            set { }
        }

        public List<FontAsset> fallbackFontAssetTable
        {
            get => new List<FontAsset>(FallbacksInternal());
            set => SetFallbacksInternal(value?.ToArray() ?? Array.Empty<FontAsset>());
        }

        public FontWeightPair[] fontWeightTable
        {
            get
            {
                var table = new FontWeightPair[10];
                for (int i = 0; i < 10; i++)
                    table[i] = new FontWeightPair { regularTypeface = WeightTypefaceInternal(i, false), italicTypeface = WeightTypefaceInternal(i, true) };
                return table;
            }
            set
            {
                for (int i = 0; value != null && i < value.Length && i < 10; i++)
                {
                    SetWeightTypefaceInternal(i, false, value[i].regularTypeface);
                    SetWeightTypefaceInternal(i, true, value[i].italicTypeface);
                }
            }
        }

        public FontAssetCreationSettings creationSettings
        {
            get
            {
                m_CreationSettings.pointSize = samplingPointSize;
                m_CreationSettings.atlasWidth = atlasWidth;
                m_CreationSettings.atlasHeight = atlasHeight;
                m_CreationSettings.renderMode = (int)atlasRenderMode;
                return m_CreationSettings;
            }
            set => m_CreationSettings = value;
        }

        public FontFeatureTable fontFeatureTable { get => m_FontFeatureTable; set { } }
        public bool getFontFeatures { get; set; } = true;

        /// <summary>The glyphs rendered so far, in glyph order.</summary>
        public List<Glyph> glyphTable
        {
            get
            {
                Refresh();
                return m_Glyphs;
            }
            set { }
        }

        public List<Character> characterTable
        {
            get
            {
                Refresh();
                return m_Characters;
            }
            set { }
        }

        public Dictionary<uint, Glyph> glyphLookupTable
        {
            get
            {
                Refresh();
                return m_GlyphLookup;
            }
        }

        public Dictionary<uint, Character> characterLookupTable
        {
            get
            {
                Refresh();
                return m_CharacterLookup;
            }
        }

        void Refresh()
        {
            m_Glyphs = new List<Glyph>();
            m_GlyphLookup = new Dictionary<uint, Glyph>();
            foreach (GlyphData d in GlyphTableInternal())
            {
                var g = new Glyph(d.index, d.metrics, d.glyphRect, d.scale, d.atlasIndex);
                m_Glyphs.Add(g);
                m_GlyphLookup[d.index] = g;
            }
            m_Characters = new List<Character>();
            m_CharacterLookup = new Dictionary<uint, Character>();
            foreach (CharacterData d in CharacterTableInternal())
            {
                m_GlyphLookup.TryGetValue(d.glyphIndex, out Glyph g);
                var c = new Character(d.unicode, this, g) { glyphIndex = d.glyphIndex, scale = d.scale };
                m_Characters.Add(c);
                m_CharacterLookup[d.unicode] = c;
            }
        }

        public bool HasCharacter(int character) => HasCharacterInternal((uint)character, false, false);
        public bool HasCharacter(char character, bool searchFallbacks = false, bool tryAddCharacter = false) =>
            HasCharacterInternal(character, searchFallbacks, tryAddCharacter);
        public bool HasCharacter(uint character, bool searchFallbacks = false, bool tryAddCharacter = false) =>
            HasCharacterInternal(character, searchFallbacks, tryAddCharacter);

        public bool HasCharacters(string text) => HasCharacters(text, out List<char> _);

        public bool HasCharacters(string text, out List<char> missingCharacters)
        {
            missingCharacters = new List<char>();
            foreach (char c in text ?? "")
                if (!HasCharacterInternal(c, false, false))
                    missingCharacters.Add(c);
            return missingCharacters.Count == 0;
        }

        public bool HasCharacters(string text, out uint[] missingCharacters, bool searchFallbacks = false, bool tryAddCharacter = false)
        {
            var missing = new List<uint>();
            foreach (char c in text ?? "")
                if (!HasCharacterInternal(c, searchFallbacks, tryAddCharacter))
                    missing.Add(c);
            missingCharacters = missing.ToArray();
            return missing.Count == 0;
        }

        public bool TryAddCharacters(uint[] unicodes, bool includeFontFeatures = false) => TryAddCharacters(unicodes, out uint[] _, includeFontFeatures);

        public bool TryAddCharacters(uint[] unicodes, out uint[] missingUnicodes, bool includeFontFeatures = false)
        {
            missingUnicodes = TryAddCharactersInternal(unicodes ?? Array.Empty<uint>());
            return missingUnicodes.Length == 0;
        }

        public bool TryAddCharacters(string characters, out string missingCharacters, bool includeFontFeatures = false)
        {
            var codes = new List<uint>();
            for (int i = 0; characters != null && i < characters.Length; i++)
            {
                if (char.IsHighSurrogate(characters[i]) && i + 1 < characters.Length)
                {
                    codes.Add((uint)char.ConvertToUtf32(characters[i], characters[i + 1]));
                    i++;
                }
                else
                    codes.Add(characters[i]);
            }
            uint[] missing = TryAddCharactersInternal(codes.ToArray());
            var s = new System.Text.StringBuilder();
            foreach (uint m in missing)
                s.Append(char.ConvertFromUtf32((int)m));
            missingCharacters = s.ToString();
            return missing.Length == 0;
        }

        public bool TryGetGlyphIndex(uint unicode, out uint glyphIndex)
        {
            glyphIndex = GlyphIndexInternal(unicode);
            return glyphIndex != 0;
        }

        public static string GetCharacters(FontAsset fontAsset)
        {
            var s = new System.Text.StringBuilder();
            foreach (Character c in fontAsset.characterTable)
                s.Append(char.ConvertFromUtf32((int)c.unicode));
            return s.ToString();
        }

        public static int[] GetCharactersArray(FontAsset fontAsset)
        {
            List<Character> table = fontAsset.characterTable;
            var a = new int[table.Count];
            for (int i = 0; i < a.Length; i++)
                a[i] = (int)table[i].unicode;
            return a;
        }

        public static FontAsset CreateFontAsset(string familyName, string styleName, int pointSize = 90)
        {
            FontAsset f = CreateFontAssetInternal(familyName, styleName ?? "");
            if (f != null)
                f.SetSamplingPointSizeInternal(pointSize);
            return f;
        }
    }
}

namespace HelixEngine.Text.LowLevel
{
    public enum GlyphRenderMode
    {
        SMOOTH_HINTED = 4121,
        SMOOTH = 4117,
        COLOR_HINTED = 69641,
        COLOR = 69637,
        RASTER_HINTED = 4169,
        RASTER = 4165,
        SDF = 4134,
        SDF8 = 8230,
        SDF16 = 16422,
        SDF32 = 32806,
        SDFAA_HINTED = 4137,
        SDFAA = 4133,
    }

    public enum GlyphPackingMode
    {
        BestShortSideFit = 0,
        BestLongSideFit = 1,
        BestAreaFit = 2,
        BottomLeftRule = 3,
        ContactPointRule = 4,
    }

    [Serializable]
    public struct GlyphValueRecord
    {
        public float xPlacement { get; set; }
        public float yPlacement { get; set; }
        public float xAdvance { get; set; }
        public float yAdvance { get; set; }

        public GlyphValueRecord(float xPlacement, float yPlacement, float xAdvance, float yAdvance)
        {
            this.xPlacement = xPlacement;
            this.yPlacement = yPlacement;
            this.xAdvance = xAdvance;
            this.yAdvance = yAdvance;
        }

        public static GlyphValueRecord operator +(GlyphValueRecord a, GlyphValueRecord b) =>
            new GlyphValueRecord(a.xPlacement + b.xPlacement, a.yPlacement + b.yPlacement, a.xAdvance + b.xAdvance, a.yAdvance + b.yAdvance);
    }

    [Serializable]
    public struct GlyphAdjustmentRecord
    {
        public uint glyphIndex { get; set; }
        public GlyphValueRecord glyphValueRecord { get; set; }

        public GlyphAdjustmentRecord(uint glyphIndex, GlyphValueRecord glyphValueRecord)
        {
            this.glyphIndex = glyphIndex;
            this.glyphValueRecord = glyphValueRecord;
        }
    }

    [Serializable]
    public struct GlyphPairAdjustmentRecord
    {
        public GlyphAdjustmentRecord firstAdjustmentRecord { get; set; }
        public GlyphAdjustmentRecord secondAdjustmentRecord { get; set; }
        public FontFeatureLookupFlags featureLookupFlags { get; set; }

        public GlyphPairAdjustmentRecord(GlyphAdjustmentRecord firstAdjustmentRecord, GlyphAdjustmentRecord secondAdjustmentRecord)
        {
            this.firstAdjustmentRecord = firstAdjustmentRecord;
            this.secondAdjustmentRecord = secondAdjustmentRecord;
            featureLookupFlags = FontFeatureLookupFlags.None;
        }
    }

    [Flags]
    public enum FontFeatureLookupFlags
    {
        None = 0x0,
        IgnoreLigatures = 0x4,
        IgnoreSpacingAdjustments = 0x100,
    }
}
