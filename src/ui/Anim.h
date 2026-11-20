#pragma once

#include <JuceHeader.h>
#include <cmath>

// ============================================================
// Anim — UI 动画基础设施（与 Hasil 插件家族统一）
// ============================================================
namespace Anim
{
    static constexpr double kDrawerMs     = 250.0;
    static constexpr double kModuleAnimMs = 340.0;   // 路线 A 频段舱模块入场/出场与核心模组贝塞尔平移时长
    static constexpr double kPopoverMs    = 220.0;
    static constexpr double kSegSlideMs   = 220.0;   // A/B 等分段胶囊 0.22s 阻尼平移动画
    static constexpr double kFadeMs       = 160.0;   // hover / 选中淡入淡出
    static constexpr double kKnobMs       = 120.0;   // 旋钮四态渐变
    static constexpr double kBlinkMs      = 500.0;   // 输入光标半周期
    static constexpr double kMeterTauMs   = 90.0;    // 显示量程平滑

    static constexpr float kSnapEps = 0.002f;
}

namespace Ease
{
    inline float outCubic (float t)
    {
        const float u = 1.0f - t;
        return 1.0f - u * u * u;
    }

    inline float inOutCubic (float t)
    {
        return (t < 0.5f) ? (4.0f * t * t * t)
                          : (1.0f - std::pow (-2.0f * t + 2.0f, 3.0f) * 0.5f);
    }

    /** 精确求解 CSS cubic-bezier(x1, y1, x2, y2)，默认对应 cubic-bezier(0.22, 1.0, 0.36, 1.0) */
    inline float cubicBezier (float t,
                              float x1 = 0.22f, float y1 = 1.0f,
                              float x2 = 0.36f, float y2 = 1.0f) noexcept
    {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;

        const float cx = 3.0f * x1;
        const float bx = 3.0f * (x2 - x1) - cx;
        const float ax = 1.0f - cx - bx;

        const float cy = 3.0f * y1;
        const float by = 3.0f * (y2 - y1) - cy;
        const float ay = 1.0f - cy - by;

        float u = t;
        for (int i = 0; i < 5; ++i)
        {
            const float xEst = ((ax * u + bx) * u + cx) * u - t;
            if (std::abs (xEst) < 1.0e-5f)
                break;
            const float dx = (3.0f * ax * u + 2.0f * bx) * u + cx;
            if (std::abs (dx) < 1.0e-5f)
                break;
            u = juce::jlimit (0.0f, 1.0f, u - xEst / dx);
        }
        return ((ay * u + by) * u + cy) * u;
    }

    inline float linear (float t) { return t; }
}

struct Tween
{
    double t0ms   = 0.0;
    double durMs  = Anim::kFadeMs;
    float  from   = 0.0f;
    float  to     = 0.0f;
    bool   active = false;

    void start (double nowMs, float newTarget, double ms, float current)
    {
        if (newTarget == to && active)
            return;
        from   = current;
        to     = newTarget;
        t0ms   = nowMs;
        durMs  = (ms > 0.0) ? ms : Anim::kFadeMs;
        active = true;
    }

    void snapTo (float v)
    {
        from = to = v;
        active = false;
    }

    float value (double nowMs) const
    {
        if (! active)
            return to;
        const double t = (nowMs - t0ms) / durMs;
        if (t >= 1.0) return to;
        if (t <= 0.0) return from;
        return from + (to - from) * Ease::outCubic ((float) t);
    }

    float valueBezier (double nowMs,
                       float x1 = 0.22f, float y1 = 1.0f,
                       float x2 = 0.36f, float y2 = 1.0f) const noexcept
    {
        if (! active)
            return to;
        const double t = (nowMs - t0ms) / durMs;
        if (t >= 1.0) return to;
        if (t <= 0.0) return from;
        return from + (to - from) * Ease::cubicBezier ((float) t, x1, y1, x2, y2);
    }

    bool done (double nowMs) const
    {
        return (! active) || ((nowMs - t0ms) >= durMs);
    }

    float target() const noexcept { return to; }
};

inline void tweenTo (Tween& t, double nowMs, float target, double ms)
{
    if (target != t.target())
        t.start (nowMs, target, ms, t.value (nowMs));
    else if (t.done (nowMs))
        t.snapTo (target);
}
