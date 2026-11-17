#pragma once

#include <Helix/Math/Geometry.h>
#include <Helix/UI/Elements/ElementTree.h>
#include <Helix/UI/Layout/LayoutStyle.h>

#include <atomic>

// The flexbox layout of an element tree. Each element's LayoutStyle (from its computed style) goes in; its LayoutBox
// comes out. A layout runs again only where the tree marks layout dirty; other subtrees keep their results when their
// constraints are the same. Elements with content (text, images) size it through a measure function.
namespace helix::ui
{
    // The content size of an element for the available width and height (border box minus padding and border).
    using MeasureFunction = Vector2 (*)(void* user, ElementHandle element, f32 width, MeasureMode widthMode, f32 height, MeasureMode heightMode);

    class LayoutEngine
    {
    public:
        explicit LayoutEngine(ElementTree& tree) : tree_(tree) {}

        const LayoutStyle& Style(ElementHandle e);
        // Sets an element's layout inputs (its layout is marked dirty when they change).
        void SetStyle(ElementHandle e, const LayoutStyle& style);
        void SetMeasure(ElementHandle e, MeasureFunction function, void* user);
        // The measure function's content changed: the element is laid out again.
        void MarkContentDirty(ElementHandle e) { tree_.MarkLayoutDirty(e); }

        // Lays out the tree under root in a box of the given size (the root's border box).
        void Update(ElementHandle root, f32 width, f32 height);
        // The same with the modes of the box's sides (AtMost: the root no larger than them, sized by its content).
        void Update(ElementHandle root, f32 width, MeasureMode widthMode, f32 height, MeasureMode heightMode);

        const LayoutBox& Box(ElementHandle e) const;
        // The border box in the root's space with its edges on the pixel grid, as the reference's layout rounds them:
        // each edge to the nearest pixel, a measured element's right and bottom up (its text keeps its room).
        Rect WorldRect(ElementHandle e) const;

        // How many elements the last Update laid out or measured (the cache's misses).
        u32 LastComputed() const { return computed_.load(std::memory_order_relaxed); }

    private:
        struct Cache
        {
            f32 width = -1, height = -1, ownerWidth = -1, ownerHeight = -1;
            MeasureMode widthMode = MeasureMode::Undefined, heightMode = MeasureMode::Undefined;
            f32 resultWidth = 0, resultHeight = 0;
            bool valid = false;
            // The Update that made it.
            u32 pass = 0;
        };

        struct Slot
        {
            u32 generation = ~0u;
            LayoutStyle style;
            LayoutBox box;
            MeasureFunction measure = nullptr;
            void* user = nullptr;
            // The last measurements (a few constraints are asked for in turn), and the last layout done for real (its children
            // placed).
            static constexpr u32 kMeasured = 4;
            Cache measured[kMeasured], laidOut;
            u32 nextMeasured = 0;
            Vector2 world;
            // The border box in the root's space on the pixel grid.
            Rect pixel;
            // The Update that laid out its subtree for real (not from the cache).
            u32 laidPass = 0;
        };

        Slot& At(ElementHandle e);
        const Cache* Cached(const Slot& s, ElementHandle e, f32 ownerWidth, f32 ownerHeight, f32 width, MeasureMode widthMode, f32 height,
                            MeasureMode heightMode, bool place) const;
        bool IsCached(ElementHandle e, f32 ownerWidth, f32 ownerHeight, f32 width, MeasureMode widthMode, f32 height, MeasureMode heightMode, bool place);
        // Runs f for 0..count-1, on the jobs when `work` of them must really be computed.
        template <typename F>
        void RunAll(u32 count, u32 work, const F& f);
        const Slot* Find(ElementHandle e) const;
        Vector2 Compute(ElementHandle e, f32 ownerWidth, f32 ownerHeight, f32 width, MeasureMode widthMode, f32 height, MeasureMode heightMode,
                        bool place);
        Vector2 ComputeContainer(ElementHandle e, Slot& s, f32 ownerWidth, f32 ownerHeight, f32 width, MeasureMode widthMode, f32 height,
                                 MeasureMode heightMode, bool place);
        void PlaceAbsolute(ElementHandle child, const Slot& parent, f32 width, f32 height);
        void ClearLayoutDirty(ElementHandle root);
        void UpdateWorld(ElementHandle e, Vector2 origin, bool force);

        ElementTree& tree_;
        Array<Slot> slots_;
        std::atomic<u32> computed_{0};
        u32 pass_ = 0;
        // Children are being placed on the jobs (their subtrees then run serially).
        std::atomic<bool> parallel_{false};
    };
}
