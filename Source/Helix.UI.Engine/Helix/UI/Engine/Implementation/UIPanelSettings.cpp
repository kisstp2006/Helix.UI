#include <Helix/UI/Engine/UIPanelSettings.h>

#include <Helix/Scripting/Bindings/Interop.h>

#include <bit>
#include <cmath>

namespace helix::ui
{
    namespace
    {
        // C#'s function of the settings' functions: (the settings' id, x, y, out x y z) -> 1 when on the panel.
        using ManagedScreenToPanel = i32 (*)(u64 settings, f32 x, f32 y, f32* out);
        ManagedScreenToPanel managedScreenToPanel = nullptr;

        void SetScreenToPanelCallback(void* function) { managedScreenToPanel = std::bit_cast<ManagedScreenToPanel>(function); }

        struct ExportRegistrar
        {
            ExportRegistrar() { scripting::RegisterNativeExport("HelixUI.SetScreenToPanelCallback", std::bit_cast<const void*>(&SetScreenToPanelCallback)); }
        } registrar;
    }

    bool UIPanelSettings::ScreenToPanel(Vector2 screen, Vector3& out)
    {
        if (!screenToPanel_ || !managedScreenToPanel)
            return false;
        f32 r[3] = {0, 0, 0};
        if (!managedScreenToPanel(scripting::ObjectHandles<engine::Object>::ToId(this), screen.x, screen.y, r))
            return false;
        out = Vector3(r[0], r[1], screenToPanel_ == 2 ? r[2] : 0.0f);
        return !std::isnan(r[0]) && !std::isnan(r[1]);
    }

    f32 UIPanelSettings::ResolveScale(f32 width, f32 height, f32 dpi) const
    {
        f32 s = scale_ > 0 ? scale_ : 1.0f;
        switch (scaleMode_)
        {
        case PanelScaleMode::ConstantPhysicalSize:
        {
            f32 screen = dpi > 0 ? dpi : fallbackDpi_;
            f32 reference = referenceDpi_ > 0 ? referenceDpi_ : 96.0f;
            return s * screen / reference;
        }
        case PanelScaleMode::ScaleWithScreenSize:
        {
            f32 rw = referenceResolution_.x > 0 ? f32(referenceResolution_.x) : 1.0f;
            f32 rh = referenceResolution_.y > 0 ? f32(referenceResolution_.y) : 1.0f;
            if (width <= 0 || height <= 0)
                return s;
            f32 kw = width / rw, kh = height / rh;
            switch (screenMatchMode_)
            {
            case PanelScreenMatchMode::Shrink: return s * (kw > kh ? kw : kh);
            case PanelScreenMatchMode::Expand: return s * (kw < kh ? kw : kh);
            default:
            {
                // Between the width's and the height's scale in log space.
                f32 m = match_ < 0 ? 0 : match_ > 1 ? 1 : match_;
                f32 lw = std::log2(kw), lh = std::log2(kh);
                return s * std::exp2(lw + (lh - lw) * m);
            }
            }
        }
        default: return s;
        }
    }
}
