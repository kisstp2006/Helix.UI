#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Testing/Bench.h>
#include <Helix/UI/Text/Fonts/SystemFonts.h>
#include <Helix/UI/Text/Layout/TextLayout.h>

#include <chrono>
#include <cstdio>

using namespace helix;
using namespace helix::ui;

namespace
{
    f64 Ms(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b)
    {
        return std::chrono::duration<f64, std::milli>(b - a).count();
    }

    // A face of the machine by family (null when it has none: the benchmark says so and skips).
    std::shared_ptr<const Face> MachineFace(StringView family)
    {
        const SystemFontFace* f = SystemFonts::Find(family);
        Array<u8> bytes;
        if (!f || !platform::ReadFile(f->path.View(), bytes))
            return nullptr;
        auto face = std::make_shared<Face>();
        String error;
        return face->Open(Span<const u8>(bytes.Data(), bytes.Size()), f->index, error) ? face : nullptr;
    }

    // A page of text: paragraphs of Latin prose with a little rich text, about 3000 characters.
    String Page()
    {
        String page;
        for (int p = 0; p < 6; p++)
            page += "The quick brown fox jumps over the lazy dog while the <b>bold</b> heron waits by the river; numbers like 1,234.56 and "
                    "words like <i>co-operation</i> break where the rules allow, and a long paragraph flows over many lines of the box "
                    "until it reaches its end. Another sentence follows with more words to fill the line widths of the page.\n";
        return page;
    }
}

// A page of text (6 paragraphs, about 3000 characters) laid out in a 600 pixel column: shaped, broken into lines,
// placed, its glyph images taken from a warm atlas. Shaping alone: the same text as one run.
HX_BENCH(TextPage)
{
    std::shared_ptr<const Face> face = MachineFace("Segoe UI");
    if (!face)
        face = MachineFace("DejaVu Sans");
    if (!face)
    {
        std::printf("no Segoe UI or DejaVu Sans on this machine: skipped\n");
        while (state.KeepRunning())
        {
        }
        return;
    }
    TextFont font(face, GlyphRenderMode::Sdf, 48, 6, 1024);
    TextGenerationSettings settings;
    settings.font = &font;
    settings.fontSize = 16;
    settings.width = 600;
    String page = Page();
    TextGenerator generator;
    TextInfo info;
    generator.Generate(page.View(), settings, info);
    state.SetItemsPerIteration(info.characterCount);
    // Shaping alone: one run of the plain text.
    Array<char32_t> text;
    for (usize at = 0; at < page.Size();)
        text.Push(DecodeUtf8(page.View(), at));
    ShapingRun run;
    run.end = u32(text.Size());
    run.script = FindScript("Latn");
    Shaper shaper;
    Array<ShapedGlyph> glyphs;
    f64 layout = 0, shaping = 0;
    u32 rounds = 0;
    while (state.KeepRunning())
    {
        auto t0 = std::chrono::steady_clock::now();
        generator.Generate(page.View(), settings, info);
        auto t1 = std::chrono::steady_clock::now();
        shaper.Shape(*face, Span<const char32_t>(text.Data(), text.Size()), run, glyphs);
        auto t2 = std::chrono::steady_clock::now();
        layout += Ms(t0, t1);
        shaping += Ms(t1, t2);
        rounds++;
    }
    std::printf("page of %u characters in %u lines: layout %.3f ms, shaping alone %.3f ms\n", info.characterCount, info.lineCount, layout / rounds,
                shaping / rounds);
}
