#include "icons.h"

namespace {
constexpr uint16_t BLACK = 0x0000, WHITE = 0xffff, RED = 0xf800;

// Three-pixel strokes with the same treatment of horizontal and vertical edges.
void stroke(Adafruit_GFX& g, int x0, int y0, int x1, int y1, uint16_t color = BLACK) {
    g.drawLine(x0, y0, x1, y1, color);
    g.drawLine(x0-1, y0, x1-1, y1, color);
    g.drawLine(x0+1, y0, x1+1, y1, color);
    g.drawLine(x0, y0-1, x1, y1-1, color);
    g.drawLine(x0, y0+1, x1, y1+1, color);
}

void sun(Adafruit_GFX& g, int cx, int cy, int radius) {
    g.fillCircle(cx, cy, radius, BLACK);
    const int ray = radius + 3;
    g.fillRect(cx-1, cy-ray-2, 3, 3, BLACK);
    g.fillRect(cx-1, cy+ray, 3, 3, BLACK);
    g.fillRect(cx-ray-2, cy-1, 3, 3, BLACK);
    g.fillRect(cx+ray, cy-1, 3, 3, BLACK);
    const int diagonal = radius + 2;
    for (int sx : {-1, 1}) for (int sy : {-1, 1})
        g.fillRect(cx+sx*diagonal-1, cy+sy*diagonal-1, 3, 3, BLACK);
}

// White separation keeps a sun behind the cloud from merging into its outline.
void cloud(Adafruit_GFX& g, int cx, int top, bool small = false) {
    const int r = small ? 4 : 6, offset = small ? 5 : 7;
    for (int halo : {1, 0}) {
        const uint16_t color = halo ? WHITE : BLACK;
        g.fillCircle(cx, top+r, r+halo, color);
        g.fillCircle(cx-offset, top+r+4, r-1+halo, color);
        g.fillCircle(cx+offset, top+r+4, r-1+halo, color);
        g.fillRect(cx-offset, top+r+3, 2*offset+1, r+halo, color);
    }
}

void drop(Adafruit_GFX& g, int x, int y) {
    g.drawLine(x+1, y, x-1, y+4, BLACK);
    g.drawLine(x+2, y, x, y+4, BLACK);
}

void snowflake(Adafruit_GFX& g, int x, int y) {
    g.drawFastHLine(x-3, y, 7, BLACK);
    g.drawFastVLine(x, y-3, 7, BLACK);
    for (int sx : {-1, 1}) for (int sy : {-1, 1}) for (int d : {1, 2})
        g.drawPixel(x+sx*d, y+sy*d, BLACK);
}

// Both axes use exactly this arrowhead profile, reflected for reverse flow.
constexpr int HEAD_LENGTH = 5;
int headHalfWidth(int distance) { return (distance * 4 + 2) / HEAD_LENGTH; }
}

namespace Icons {
void horizontalArrow(Adafruit_GFX& g, int left, int right, int y, int direction) {
    if (!direction) { g.fillRect(left, y-1, right-left+1, 3, BLACK); return; }
    const int tip = direction > 0 ? right : left;
    const int shaftLeft = direction > 0 ? left : left+HEAD_LENGTH;
    const int shaftRight = direction > 0 ? right-HEAD_LENGTH : right;
    g.fillRect(shaftLeft, y-1, shaftRight-shaftLeft+1, 3, BLACK);
    for (int d = 0; d <= HEAD_LENGTH; ++d) {
        const int half = headHalfWidth(d);
        g.drawFastVLine(tip-direction*d, y-half, 2*half+1, BLACK);
    }
}

void verticalArrow(Adafruit_GFX& g, int top, int bottom, int x, int direction, uint16_t color) {
    if (!direction) { g.fillRect(x-1, top, 3, bottom-top+1, color); return; }
    const int tip = direction > 0 ? bottom : top;
    const int shaftTop = direction > 0 ? top : top+HEAD_LENGTH;
    const int shaftBottom = direction > 0 ? bottom-HEAD_LENGTH : bottom;
    g.fillRect(x-1, shaftTop, 3, shaftBottom-shaftTop+1, color);
    for (int d = 0; d <= HEAD_LENGTH; ++d) {
        const int half = headHalfWidth(d);
        g.drawFastHLine(x-half, tip-direction*d, 2*half+1, color);
    }
}

void weather(Adafruit_GFX& g, Logic::WeatherIcon icon) {
    using I = Logic::WeatherIcon;
    if (icon == I::Sun) { sun(g, 16, 25, 6); return; }
    if (icon == I::MostlySunny || icon == I::PartlyCloudy) {
        sun(g, 11, 20, 4);
        cloud(g, icon == I::MostlySunny ? 20 : 18, 23, icon == I::MostlySunny);
        return;
    }
    if (icon == I::Cloud) { cloud(g, 16, 17); return; }
    if (icon == I::Unknown) return;
    const bool showers = icon == I::Showers || icon == I::SnowShowers;
    if (showers) sun(g, 10, 18, 3);
    cloud(g, showers ? 18 : 16, 13);
    if (icon == I::Fog) {
        g.fillRect(5, 31, 23, 2, BLACK);
        g.fillRect(8, 35, 17, 2, BLACK);
        g.fillRect(5, 39, 23, 2, BLACK);
    } else if (icon == I::Storm || icon == I::Hail) {
        static const char* const bolt[] = {
            "...####", "..####.", ".####..", "######.", "...###.",
            "..###..", "..##...", ".##....", ".#....."
        };
        for (int y = 0; y < 9; ++y) for (int x = 0; x < 7; ++x)
            if (bolt[y][x] == '#') g.drawPixel(13+x, 31+y, BLACK);
        if (icon == I::Hail) { g.fillCircle(7, 34, 1, BLACK); g.fillCircle(25, 34, 1, BLACK); }
    } else if (icon == I::Snow || icon == I::SnowShowers) {
        snowflake(g, 9, 35); snowflake(g, 23, 35);
    } else if (icon == I::FreezingRain) {
        snowflake(g, 9, 35); drop(g, 23, 33);
    } else {
        for (int x : {8, 16, 24}) {
            if (icon == I::Drizzle) g.fillCircle(x, 34, 1, BLACK);
            else drop(g, x, 33);
        }
    }
}

void umbrella(Adafruit_GFX& g, int x, int y) {
    // Symmetric canopy and a hooked handle, at the final display resolution.
    static const char* const rows[] = {
        ".....#.....", "...#####...", "..#######..", ".#########.",
        "###########", "##.#####.##", ".....#.....", ".....#.....",
        ".....#.....", "..#..#.....", "..####....."
    };
    for (int row = 0; row < 11; ++row) for (int col = 0; col < 11; ++col)
        if (rows[row][col] == '#') g.drawPixel(x+col, y+row, BLACK);
}

void solar(Adafruit_GFX& g) {
    // Native 35 x 37 pixel symbol: a full sun over six separate panel cells.
    // The x=126 centerline and mirrored rays keep the reference's symmetry.
    constexpr int cx = 126, cy = 17;
    g.fillCircle(cx, cy, 5, BLACK);
    g.fillRect(cx-1, 5, 3, 5, BLACK);
    g.fillRect(cx-12, cy-1, 5, 3, BLACK);
    g.fillRect(cx+8, cy-1, 5, 3, BLACK);
    g.fillRect(cx-1, 25, 3, 3, BLACK);
    for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
        for (int d = 7; d <= 9; ++d) {
            g.drawPixel(cx+sx*d, cy+sy*d, BLACK);
            g.drawPixel(cx+sx*(d+1), cy+sy*d, BLACK);
            g.drawPixel(cx+sx*d, cy+sy*(d+1), BLACK);
        }
    }

    // Three cells per row, with two-pixel white gutters. Each lower row is
    // wider, giving the same perspective as the reference without a stand.
    for (int row = 0; row < 2; ++row) {
        for (int dy = 0; dy < 5; ++dy) {
            const int y = 30 + row*7 + dy;
            const int outerHalf = row ? 15+dy/2 : 12+(dy+1)/2;
            const int centerHalf = 3+row+dy/3;
            const int sideWidth = outerHalf-centerHalf-2;
            g.drawFastHLine(cx-outerHalf, y, sideWidth, BLACK);
            g.drawFastHLine(cx-centerHalf, y, 2*centerHalf+1, BLACK);
            g.drawFastHLine(cx+centerHalf+3, y, sideWidth, BLACK);
        }
    }
}

void pylon(Adafruit_GFX& g) {
    // Reflect the actual pixels, avoiding Bresenham tie-breaking differences
    // between separately drawn left/right diagonals at this small size.
    g.fillRect(18, 69, 5, 4, BLACK);
    for (int y = 73; y <= 118; ++y) {
        const int spread = 1 + (y-73)*7/45;
        for (int side : {-1, 1}) g.fillRect(20+side*spread-1, y, 3, 1, BLACK);
    }
    for (int y = 77; y <= 83; ++y) {
        const int spread = (y-77)*11/6;
        for (int side : {-1, 1}) g.fillRect(20+side*spread-1, y-1, 3, 3, BLACK);
    }
    for (int y = 89; y <= 96; ++y) {
        const int spread = (y-89)*2;
        for (int side : {-1, 1}) g.fillRect(20+side*spread-1, y-1, 3, 3, BLACK);
    }
    for (int y = 85; y <= 99; ++y) {
        const int brace = -3 + (y-85)*8/14;
        for (int side : {-1, 1}) g.drawPixel(20+side*brace, y, BLACK);
    }
    for (int y = 100; y <= 117; ++y) {
        const int brace = -5 + (y-100)*13/17;
        for (int side : {-1, 1}) g.drawPixel(20+side*brace, y, BLACK);
    }
    for (int side : {-1, 1}) {
        g.fillRect(20+side*11-1, 83, 3, 4, BLACK);
        g.fillRect(20+side*14-1, 96, 3, 4, BLACK);
    }
    g.fillRect(8, 82, 25, 3, BLACK);
    g.fillRect(5, 95, 31, 3, BLACK);
    g.fillRect(7, 118, 27, 3, BLACK);
}

void house(Adafruit_GFX& g) {
    stroke(g, 107, 88, 145, 62);
    stroke(g, 145, 62, 163, 74);
    stroke(g, 163, 74, 163, 68);
    stroke(g, 163, 68, 173, 68);
    stroke(g, 173, 68, 173, 81);
    stroke(g, 173, 81, 183, 88);
    stroke(g, 110, 87, 110, 120);
    stroke(g, 180, 87, 180, 120);
    stroke(g, 110, 120, 180, 120);
}

void storage(Adafruit_GFX& g, double soc) {
    g.fillRect(218, 70, 11, 4, BLACK);
    g.fillRoundRect(210, 74, 27, 47, 4, BLACK);
    g.fillRoundRect(213, 77, 21, 41, 2, WHITE);
    for (int i = 0; i < Logic::batteryBars(soc); ++i)
        g.fillRoundRect(215, 109-i*10, 17, 7, 1, RED);
}

void deviceBattery(Adafruit_GFX& g, double percent) {
    g.fillRect(225, 12, 3, 2, BLACK);
    g.fillRoundRect(222, 14, 9, 13, 2, BLACK);
    g.fillRect(223, 15, 7, 11, WHITE);
    if (!std::isfinite(percent)) return;
    const int height = static_cast<int>(std::round(std::fmax(0, std::fmin(100, percent))*8/100));
    if (height) g.fillRect(224, 25-height, 5, height, percent < 15 ? RED : BLACK);
}

void wifi(Adafruit_GFX& g, bool connected) {
    // Two concentric, two-pixel-thick quarter-circle arcs, centered on the dot.
    // The 45-degree cuts on either side give the familiar 90-degree fan.
    constexpr int cx = 203, cy = 23;
    for (int arc = 1; arc <= 2; ++arc) {
        const int radius = 2 + 4*arc;
        const int inner = radius - 2;
        // Half-pixel boundaries avoid a lone pixel protruding at each crest.
        const int outerDiameter = 2*radius - 1;
        const int innerDiameter = 2*inner - 1;
        for (int y = -radius; y < 0; ++y) for (int x = y; x <= -y; ++x) {
            const int distanceSquared = 4*(x*x + y*y);
            if (distanceSquared > innerDiameter*innerDiameter &&
                distanceSquared <= outerDiameter*outerDiameter)
                g.drawPixel(cx+x, cy+y, BLACK);
        }
    }
    g.fillCircle(cx, cy, 1, BLACK);
    if (!connected) stroke(g, cx-4, cy-9, cx+4, cy-1, RED);
}

void openSwitch(Adafruit_GFX& g) {
    g.fillRect(85, 105, 7, 3, BLACK);
    g.drawCircle(93, 106, 2, BLACK);
    stroke(g, 95, 104, 103, 96);
    g.drawCircle(106, 106, 2, BLACK);
    g.fillRect(108, 105, 2, 3, BLACK);
}
}
