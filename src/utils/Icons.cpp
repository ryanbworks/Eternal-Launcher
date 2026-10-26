#include "utils/Icons.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Pi = 3.14159265F;

    struct Pen24
    {
        Gdiplus::Graphics& g;
        Gdiplus::Pen pen;
        Pen24(Gdiplus::Graphics& graphics, const Gdiplus::Color& color, float width)
            : g(graphics), pen(color, width)
        {
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            pen.SetLineJoin(Gdiplus::LineJoinRound);
        }
        void line(float x1, float y1, float x2, float y2) { g.DrawLine(&pen, x1, y1, x2, y2); }
        void lines(std::initializer_list<Gdiplus::PointF> points)
        {
            g.DrawLines(&pen, points.begin(), static_cast<INT>(points.size()));
        }
        void polygon(std::initializer_list<Gdiplus::PointF> points)
        {
            g.DrawPolygon(&pen, points.begin(), static_cast<INT>(points.size()));
        }
        void circle(float cx, float cy, float r) { g.DrawEllipse(&pen, cx - r, cy - r, 2 * r, 2 * r); }
        void arc(float cx, float cy, float r, float start, float sweep)
        {
            g.DrawArc(&pen, cx - r, cy - r, 2 * r, 2 * r, start, sweep);
        }
        void rect(float x, float y, float w, float h, float radius)
        {
            Gdiplus::GraphicsPath path;
            const float d = radius * 2;
            path.AddArc(x, y, d, d, 180, 90);
            path.AddArc(x + w - d, y, d, d, 270, 90);
            path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
            path.AddArc(x, y + h - d, d, d, 90, 90);
            path.CloseFigure();
            g.DrawPath(&pen, &path);
        }
    };
}

void Icons::draw(Gdiplus::Graphics& g, Icon icon, const Gdiplus::RectF& b,
                 const Gdiplus::Color& color, float strokeWidth)
{
    const auto saved = g.Save();
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.TranslateTransform(b.X, b.Y);
    const float scale = std::min(b.Width, b.Height) / 24.0F;
    g.ScaleTransform(scale, scale);
    // Keep the visual stroke constant regardless of the icon size.
    Pen24 p(g, color, strokeWidth / std::max(scale, 0.01F));
    switch (icon)
    {
    case Icon::Home:
        p.lines({{3, 10.5F}, {12, 3}, {21, 10.5F}});
        p.lines({{5, 9.5F}, {5, 20}, {19, 20}, {19, 9.5F}});
        p.lines({{10, 20}, {10, 14}, {14, 14}, {14, 20}});
        break;
    case Icon::News:
        p.rect(3, 4, 15, 16, 2);
        p.lines({{18, 8}, {21, 8}, {21, 18}});
        p.arc(19, 18, 2, 0, 90);
        p.line(7, 8, 14, 8);
        p.line(7, 12, 14, 12);
        p.line(7, 16, 11, 16);
        break;
    case Icon::Mods:
        p.polygon({{12, 3}, {20, 7.5F}, {20, 16.5F}, {12, 21}, {4, 16.5F}, {4, 7.5F}});
        p.lines({{4, 7.5F}, {12, 12}, {20, 7.5F}});
        p.line(12, 12, 12, 21);
        break;
    case Icon::Settings:
        p.circle(12, 12, 3);
        p.circle(12, 12, 7);
        for (int i = 0; i < 8; ++i)
        {
            const float a = static_cast<float>(i) * Pi / 4;
            p.line(12 + std::cos(a) * 7, 12 + std::sin(a) * 7, 12 + std::cos(a) * 9.6F, 12 + std::sin(a) * 9.6F);
        }
        break;
    case Icon::Help:
        p.circle(12, 12, 9);
        p.arc(12, 9.5F, 2.5F, 180, 225);
        p.line(12, 12.4F, 12, 14);
        p.line(12, 17.3F, 12, 17.5F);
        break;
    case Icon::Pencil:
        p.polygon({{4, 20}, {8, 20}, {19, 9}, {15, 5}, {4, 16}});
        p.line(13.5F, 6.5F, 17.5F, 10.5F);
        break;
    case Icon::Check:
        p.lines({{5, 12.5F}, {10, 17}, {19, 7}});
        break;
    case Icon::User:
        p.circle(12, 8, 4);
        p.arc(12, 21, 8, 180, 180);
        break;
    case Icon::Key:
        p.circle(8, 15, 4);
        p.line(10.8F, 12.2F, 19, 4);
        p.line(16, 7, 19, 10);
        p.line(14, 9, 16, 11);
        break;
    case Icon::Folder:
        p.lines({{3, 18}, {3, 6}, {9, 6}, {11, 8}, {21, 8}, {21, 18}, {3, 18}});
        break;
    case Icon::Wrench:
        p.arc(16, 8, 4.5F, 90, 270);
        p.line(12.8F, 11.2F, 4.5F, 19.5F);
        p.line(6.5F, 21.5F, 14.8F, 13.2F);
        break;
    case Icon::File:
        p.lines({{6, 3}, {14, 3}, {19, 8}, {19, 21}, {6, 21}, {6, 3}});
        p.lines({{14, 3}, {14, 8}, {19, 8}});
        p.line(9, 13, 16, 13);
        p.line(9, 17, 14, 17);
        break;
    case Icon::Copy:
        p.rect(8, 8, 12, 12, 2);
        p.lines({{16, 8}, {16, 4}, {4, 4}, {4, 16}, {8, 16}});
        break;
    case Icon::External:
        p.lines({{18, 13}, {18, 20}, {4, 20}, {4, 6}, {11, 6}});
        p.lines({{15, 3}, {21, 3}, {21, 9}});
        p.line(10, 14, 21, 3);
        break;
    case Icon::ChevronDown:
        p.lines({{6, 9}, {12, 15}, {18, 9}});
        break;
    case Icon::ChevronRight:
        p.lines({{9, 6}, {15, 12}, {9, 18}});
        break;
    case Icon::Play:
    {
        Gdiplus::SolidBrush brush(color);
        const Gdiplus::PointF triangle[] = {{6, 3.5F}, {20, 12}, {6, 20.5F}};
        g.FillPolygon(&brush, triangle, 3);
        break;
    }
    case Icon::Minimize:
        p.line(4, 12, 20, 12);
        break;
    case Icon::Maximize:
        p.rect(5, 5, 14, 14, 2);
        break;
    case Icon::Restore:
        p.rect(4, 8, 12, 12, 2);
        p.lines({{8, 8}, {8, 4}, {20, 4}, {20, 16}, {16, 16}});
        break;
    case Icon::Close:
        p.line(5, 5, 19, 19);
        p.line(19, 5, 5, 19);
        break;
    case Icon::Plus:
        p.line(12, 5, 12, 19);
        p.line(5, 12, 19, 12);
        break;
    case Icon::Refresh:
        p.arc(12, 12, 8, 30, 300);
        p.lines({{20, 4}, {20, 9}, {15, 9}});
        break;
    case Icon::Shield:
        p.lines({{12, 3}, {4, 6}, {4, 12}});
        p.lines({{12, 3}, {20, 6}, {20, 12}});
        p.arc(12, 12, 8, 0, 180);
        p.lines({{9, 12}, {11, 14}, {15, 10}});
        break;
    case Icon::Bug:
        p.rect(7, 7, 10, 13, 5);
        p.line(12, 10, 12, 20);
        p.line(4, 13, 7, 13);
        p.line(17, 13, 20, 13);
        p.line(9, 4, 10, 7);
        p.line(15, 4, 14, 7);
        break;
    case Icon::Clock:
        p.circle(12, 12, 9);
        p.lines({{12, 7}, {12, 12}, {15.5F, 14}});
        break;
    case Icon::Calendar:
        p.rect(3, 5, 18, 16, 2);
        p.line(3, 10, 21, 10);
        p.line(8, 3, 8, 7);
        p.line(16, 3, 16, 7);
        break;
    case Icon::Server:
        p.rect(3, 4, 18, 7, 2);
        p.rect(3, 13, 18, 7, 2);
        p.line(7, 7.5F, 7.1F, 7.5F);
        p.line(7, 16.5F, 7.1F, 16.5F);
        break;
    case Icon::Download:
        p.line(12, 3, 12, 15);
        p.lines({{7, 10}, {12, 15}, {17, 10}});
        p.line(4, 20, 20, 20);
        break;
    case Icon::Trash:
        p.line(4, 7, 20, 7);
        p.lines({{6, 7}, {7, 20}, {17, 20}, {18, 7}});
        p.lines({{9, 7}, {9, 4}, {15, 4}, {15, 7}});
        break;
    case Icon::Power:
        p.arc(12, 13, 8, 300, 300);
        p.line(12, 3, 12, 12);
        break;
    }
    g.Restore(saved);
}
