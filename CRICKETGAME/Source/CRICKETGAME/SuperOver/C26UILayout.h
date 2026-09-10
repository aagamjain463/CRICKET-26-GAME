#pragma once
#include <algorithm>

namespace C26UI
{
struct Point { float X, Y; };
struct Box
{
    float X, Y, W, H;
    bool Contains(Point P) const { return P.X >= X && P.X <= X + W && P.Y >= Y && P.Y <= Y + H; }
    bool Overlaps(const Box& B) const { return X < B.X + B.W && X + W > B.X && Y < B.Y + B.H && Y + H > B.Y; }
};

struct Layout
{
    static constexpr float Width = 1600.f, Height = 900.f;
    float Scale = 1.f, OffsetX = 0.f, OffsetY = 0.f;
    bool Compact = false;
    Point FootCenter{160.f, 700.f};
    float FootRadius = 64.f;
    Box FootBounds{64.f, 604.f, 192.f, 192.f};
    Box Primary{1200.f, 720.f, 320.f, 68.f};
    Box Subtitle{460.f, 746.f, 680.f, 90.f};
    Box CompactBody{344.f, 224.f, 1200.f, 480.f};
    Box CompactBack{344.f, 752.f, 240.f, 112.f};
    Box CompactContinue{964.f, 752.f, 580.f, 112.f};
    Box ConfirmYes{424.f, 530.f, 352.f, 112.f};
    Box ConfirmNo{824.f, 530.f, 352.f, 112.f};

    static Layout Fit(float W, float H, float Left = 0.f, float Top = 0.f, float Right = 0.f, float Bottom = 0.f)
    {
        Layout L;
        W = std::max(1.f, W); H = std::max(1.f, H);
        Left = std::clamp(Left, 0.f, W * .2f); Right = std::clamp(Right, 0.f, W * .2f);
        Top = std::clamp(Top, 0.f, H * .2f); Bottom = std::clamp(Bottom, 0.f, H * .2f);
        const float AvailableW = W - Left - Right, AvailableH = H - Top - Bottom;
        L.Scale = std::min(AvailableW / Width, AvailableH / Height);
        L.OffsetX = Left + (AvailableW - Width * L.Scale) * .5f;
        L.OffsetY = Top + (AvailableH - Height * L.Scale) * .5f;
        L.Compact = L.Scale < .7f;
        if (L.Compact)
        {
            L.FootCenter = {176.f, 694.f};
            L.FootRadius = 100.f;
            L.FootBounds = {64.f, 582.f, 224.f, 224.f};
            L.Primary = {1176.f, 736.f, 360.f, 112.f};
            L.Subtitle = {420.f, 724.f, 700.f, 140.f};
        }
        return L;
    }
    Point ToDesign(Point P) const { return {(P.X - OffsetX) / Scale, (P.Y - OffsetY) / Scale}; }
    Point ToScreen(Point P) const { return {OffsetX + P.X * Scale, OffsetY + P.Y * Scale}; }
    Point FootInput(Point P) const
    {
        return {std::clamp((P.X - FootCenter.X) / FootRadius, -1.f, 1.f),
                std::clamp((FootCenter.Y - P.Y) / FootRadius, -1.f, 1.f)};
    }
};
}
