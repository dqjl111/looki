#include "lcolor.h"
#include "lkcore/ldebug.h"
#include <format>
#include <iostream>
#include <iomanip>
#include "lkcore/3rd/nameof.h"
#include "lkgui/text/lcssparser.h"

LK_NAMESPACE_BEGIN
std::vector<LNamedVar<uint32_t, LIDynNamedVar>> LColor::m_innerColors;

// A table of precalculated results of 0x00ff00ff/alpha use by unpremultiply:
const uint32_t ljInvPremulFactor[256] = {
    0, 16711935, 8355967, 5570645, 4177983, 3342387, 2785322, 2387419,
    2088991, 1856881, 1671193, 1519266, 1392661, 1285533, 1193709, 1114129,
    1044495, 983055, 928440, 879575, 835596, 795806, 759633, 726605,
    696330, 668477, 642766, 618960, 596854, 576273, 557064, 539094,
    522247, 506422, 491527, 477483, 464220, 451673, 439787, 428511,
    417798, 407608, 397903, 388649, 379816, 371376, 363302, 355573,
    348165, 341059, 334238, 327685, 321383, 315319, 309480, 303853,
    298427, 293191, 288136, 283253, 278532, 273966, 269547, 265268,
    261123, 257106, 253211, 249431, 245763, 242201, 238741, 235379,
    232110, 228930, 225836, 222825, 219893, 217038, 214255, 211543,
    208899, 206320, 203804, 201348, 198951, 196611, 194324, 192091,
    189908, 187774, 185688, 183647, 181651, 179698, 177786, 175915,
    174082, 172287, 170529, 168807, 167119, 165464, 163842, 162251,
    160691, 159161, 157659, 156186, 154740, 153320, 151926, 150557,
    149213, 147893, 146595, 145321, 144068, 142837, 141626, 140436,
    139266, 138115, 136983, 135869, 134773, 133695, 132634, 131590,
    130561, 129549, 128553, 127572, 126605, 125653, 124715, 123792,
    122881, 121984, 121100, 120229, 119370, 118524, 117689, 116866,
    116055, 115254, 114465, 113686, 112918, 112160, 111412, 110675,
    109946, 109228, 108519, 107818, 107127, 106445, 105771, 105106,
    104449, 103800, 103160, 102527, 101902, 101284, 100674, 100071,
    99475, 98887, 98305, 97730, 97162, 96600, 96045, 95496,
    94954, 94417, 93887, 93362, 92844, 92331, 91823, 91322,
    90825, 90334, 89849, 89368, 88893, 88422, 87957, 87497,
    87041, 86590, 86143, 85702, 85264, 84832, 84403, 83979,
    83559, 83143, 82732, 82324, 81921, 81521, 81125, 80733,
    80345, 79961, 79580, 79203, 78829, 78459, 78093, 77729,
    77370, 77013, 76660, 76310, 75963, 75619, 75278, 74941,
    74606, 74275, 73946, 73620, 73297, 72977, 72660, 72346,
    72034, 71725, 71418, 71114, 70813, 70514, 70218, 69924,
    69633, 69344, 69057, 68773, 68491, 68211, 67934, 67659,
    67386, 67116, 66847, 66581, 66317, 66055, 65795, 65537
};



void LColor::initInnerColors()
{
    if(m_innerColors.empty()){
        m_innerColors.reserve((int)LColorID::Num);
        m_innerColors.resize((int)LColorID::Num);

#define ADD_INNER_COLOR(_ID, _VALUE) \
        m_innerColors[(int)_ID].name = LStringUtil::toLower(std::string(nameof::nameof_enum<_ID>())); \
        m_innerColors[(int)_ID].value = _VALUE;

        ADD_INNER_COLOR(LColorID::Transparent , 0x00000000);
        ADD_INNER_COLOR(LColorID::AliceBlue , 0xfffff8f0);
        ADD_INNER_COLOR(LColorID::AntiqueWhite , 0xffd7ebfa);
        ADD_INNER_COLOR(LColorID::Aqua , 0xffffff00);
        ADD_INNER_COLOR(LColorID::Aquamarine , 0xffd4ff7f);
        ADD_INNER_COLOR(LColorID::Azure , 0xfffffff0);
        ADD_INNER_COLOR(LColorID::Beige , 0xffdcf5f5);
        ADD_INNER_COLOR(LColorID::Bisque , 0xffc4e4ff);
        ADD_INNER_COLOR(LColorID::Black , 0xff000000);
        ADD_INNER_COLOR(LColorID::BlanchedAlmond , 0xffcdebff);
        ADD_INNER_COLOR(LColorID::Blue , 0xffff0000);
        ADD_INNER_COLOR(LColorID::BlueViolet , 0xffe22b8a);
        ADD_INNER_COLOR(LColorID::Brown , 0xff2a2aa5);
        ADD_INNER_COLOR(LColorID::BurlyWood , 0xff87b8de);
        ADD_INNER_COLOR(LColorID::CadetBlue , 0xffa09e5f);
        ADD_INNER_COLOR(LColorID::Chartreuse , 0xff00ff7f);
        ADD_INNER_COLOR(LColorID::Chocolate , 0xff1e69d2);
        ADD_INNER_COLOR(LColorID::Coral , 0xff507fff);
        ADD_INNER_COLOR(LColorID::CornflowerBlue , 0xffed9564);
        ADD_INNER_COLOR(LColorID::Cornsilk , 0xffdcf8ff);
        ADD_INNER_COLOR(LColorID::Crimson , 0xff3c14dc);
        ADD_INNER_COLOR(LColorID::Cyan , 0xffffff00);
        ADD_INNER_COLOR(LColorID::DarkBlue , 0xff8b0000);
        ADD_INNER_COLOR(LColorID::DarkCyan , 0xff8b8b00);
        ADD_INNER_COLOR(LColorID::DarkGoldenRod , 0xff0b86b8);
        ADD_INNER_COLOR(LColorID::DarkGray , 0xffa9a9a9);
        ADD_INNER_COLOR(LColorID::DarkGreen , 0xff006400);
        ADD_INNER_COLOR(LColorID::DarkGrey , 0xffa9a9a9);
        ADD_INNER_COLOR(LColorID::DarkKhaki , 0xff6bb7bd);
        ADD_INNER_COLOR(LColorID::DarkMagenta , 0xff8b008b);
        ADD_INNER_COLOR(LColorID::DarkOliveGreen , 0xff2f6b55);
        ADD_INNER_COLOR(LColorID::DarkOrange , 0xff008cff);
        ADD_INNER_COLOR(LColorID::DarkOrchid , 0xffcc3299);
        ADD_INNER_COLOR(LColorID::DarkRed , 0xff00008b);
        ADD_INNER_COLOR(LColorID::DarkSalmon , 0xff7a96e9);
        ADD_INNER_COLOR(LColorID::DarkSeaGreen , 0xff8fbc8f);
        ADD_INNER_COLOR(LColorID::DarkSlateBlue , 0xff8b3d48);
        ADD_INNER_COLOR(LColorID::DarkSlateGray , 0xff4f4f2f);
        ADD_INNER_COLOR(LColorID::DarkSlateGrey , 0xff4f4f2f);
        ADD_INNER_COLOR(LColorID::DarkTurquoise , 0xffd1ce00);
        ADD_INNER_COLOR(LColorID::DarkViolet , 0xffd30094);
        ADD_INNER_COLOR(LColorID::DeepPink , 0xff9314ff);
        ADD_INNER_COLOR(LColorID::DeepSkyBlue , 0xffffbf00);
        ADD_INNER_COLOR(LColorID::DimGray , 0xff696969);
        ADD_INNER_COLOR(LColorID::DimGrey , 0xff696969);
        ADD_INNER_COLOR(LColorID::DodgerBlue , 0xffff901e);
        ADD_INNER_COLOR(LColorID::FireBrick , 0xff2222b2);
        ADD_INNER_COLOR(LColorID::FloralWhite , 0xfff0faff);
        ADD_INNER_COLOR(LColorID::ForestGreen , 0xff228b22);
        ADD_INNER_COLOR(LColorID::Fuchsia , 0xffff00ff);
        ADD_INNER_COLOR(LColorID::Gainsboro , 0xffdcdcdc);
        ADD_INNER_COLOR(LColorID::GhostWhite , 0xfffff8f8);
        ADD_INNER_COLOR(LColorID::GoldenRod , 0xff20a5da);
        ADD_INNER_COLOR(LColorID::Gold , 0xff00d7ff);
        ADD_INNER_COLOR(LColorID::Gray , 0xff808080);
        ADD_INNER_COLOR(LColorID::Green , 0xff008000);
        ADD_INNER_COLOR(LColorID::GreenYellow , 0xff2fffad);
        ADD_INNER_COLOR(LColorID::Grey , 0xff808080);
        ADD_INNER_COLOR(LColorID::HoneyDew , 0xfff0fff0);
        ADD_INNER_COLOR(LColorID::HotPink , 0xffb469ff);
        ADD_INNER_COLOR(LColorID::IndianRed , 0xff5c5ccd);
        ADD_INNER_COLOR(LColorID::Indigo , 0xff82004b);
        ADD_INNER_COLOR(LColorID::Ivory , 0xfff0ffff);
        ADD_INNER_COLOR(LColorID::Khaki , 0xff8ce6f0);
        ADD_INNER_COLOR(LColorID::Lavenderblush , 0xfff5f0ff);
        ADD_INNER_COLOR(LColorID::Lavender , 0xfffae6e6);
        ADD_INNER_COLOR(LColorID::LawnGreen , 0xff00fc7c);
        ADD_INNER_COLOR(LColorID::Lemonchiffon , 0xffcdfaff);
        ADD_INNER_COLOR(LColorID::LightBlue , 0xffe6d8ad);
        ADD_INNER_COLOR(LColorID::LightCoral , 0xff8080f0);
        ADD_INNER_COLOR(LColorID::LightCyan , 0xffffffe0);
        ADD_INNER_COLOR(LColorID::LightGoldenRodYellow , 0xffd2fafa);
        ADD_INNER_COLOR(LColorID::LightGray , 0xffd3d3d3);
        ADD_INNER_COLOR(LColorID::LightGreen , 0xff90ee90);
        ADD_INNER_COLOR(LColorID::LightGrey , 0xffd3d3d3);
        ADD_INNER_COLOR(LColorID::LightPink , 0xffc1b6ff);
        ADD_INNER_COLOR(LColorID::LightSalmon , 0xff7aa0ff);
        ADD_INNER_COLOR(LColorID::LightSeaGreen , 0xffaab220);
        ADD_INNER_COLOR(LColorID::LightSkyBlue , 0xffface87);
        ADD_INNER_COLOR(LColorID::LightSlateGray , 0xff998877);
        // ADD_INNER_COLOR(LColorID::LightSlateGrey , 0xff998877);
        ADD_INNER_COLOR(LColorID::LightSteelBlue , 0xffdec4b0);
        ADD_INNER_COLOR(LColorID::LightYellow , 0xffe0ffff);
        ADD_INNER_COLOR(LColorID::Lime , 0xff00ff00);
        ADD_INNER_COLOR(LColorID::LimeGreen , 0xff32cd32);
        ADD_INNER_COLOR(LColorID::Linen , 0xffe6f0fa);
        ADD_INNER_COLOR(LColorID::Magenta , 0xffff00ff);
        ADD_INNER_COLOR(LColorID::Maroon , 0xff000080);
        ADD_INNER_COLOR(LColorID::MediumAquamarine , 0xffaacd66);
        ADD_INNER_COLOR(LColorID::MediumBlue , 0xffcd0000);
        ADD_INNER_COLOR(LColorID::MediumOrchid , 0xffd355ba);
        ADD_INNER_COLOR(LColorID::MediumPurple , 0xffdb7093);
        ADD_INNER_COLOR(LColorID::MediumSeaGreen , 0xff71b33c);
        ADD_INNER_COLOR(LColorID::MediumSlateBlue , 0xffee687b);
        ADD_INNER_COLOR(LColorID::MediumSpringGreen , 0xff9afa00);
        ADD_INNER_COLOR(LColorID::MediumTurquoise , 0xffccd148);
        ADD_INNER_COLOR(LColorID::MediumVioletRed , 0xff8515c7);
        ADD_INNER_COLOR(LColorID::MidnightBlue , 0xff701919);
        ADD_INNER_COLOR(LColorID::MintCream , 0xfffafff5);
        ADD_INNER_COLOR(LColorID::MistyRose , 0xffe1e4ff);
        ADD_INNER_COLOR(LColorID::Moccasin , 0xffb5e4ff);
        ADD_INNER_COLOR(LColorID::NavajoWhite , 0xffaddeff);
        ADD_INNER_COLOR(LColorID::Navy , 0xff800000);
        ADD_INNER_COLOR(LColorID::Oldlace , 0xffe6f5fd);
        ADD_INNER_COLOR(LColorID::Olive , 0xff008080);
        ADD_INNER_COLOR(LColorID::Olivedrab , 0xff238e6b);
        ADD_INNER_COLOR(LColorID::Orange , 0xff00a5ff);
        ADD_INNER_COLOR(LColorID::OrangeRed , 0xff0045ff);
        ADD_INNER_COLOR(LColorID::Orchid , 0xffd670da);
        ADD_INNER_COLOR(LColorID::PaleGoldenRod , 0xffaae8ee);
        ADD_INNER_COLOR(LColorID::PaleGreen , 0xff98fb98);
        ADD_INNER_COLOR(LColorID::PaleTurquoise , 0xffeeeeaf);
        ADD_INNER_COLOR(LColorID::PaleVioletRed , 0xff9370db);
        ADD_INNER_COLOR(LColorID::Papayawhip , 0xffd5efff);
        ADD_INNER_COLOR(LColorID::Peachpuff , 0xffb9daff);
        ADD_INNER_COLOR(LColorID::Peru , 0xff3f85cd);
        ADD_INNER_COLOR(LColorID::Pink , 0xffcbc0ff);
        ADD_INNER_COLOR(LColorID::Plum , 0xffdda0dd);
        ADD_INNER_COLOR(LColorID::PowderBlue , 0xffe6e0b0);
        ADD_INNER_COLOR(LColorID::Purple , 0xff800080);
        ADD_INNER_COLOR(LColorID::RebeccaPurple , 0xff993366);
        ADD_INNER_COLOR(LColorID::Red , 0xff0000ff);
        ADD_INNER_COLOR(LColorID::RosyBrown , 0xff8f8fbc);
        ADD_INNER_COLOR(LColorID::RoyalBlue , 0xffe16941);
        ADD_INNER_COLOR(LColorID::SaddleBrown , 0xff13458b);
        ADD_INNER_COLOR(LColorID::Salmon , 0xff7280fa);
        ADD_INNER_COLOR(LColorID::SandyBrown , 0xff60a4f4);
        ADD_INNER_COLOR(LColorID::SeaGreen , 0xff578b2e);
        ADD_INNER_COLOR(LColorID::SeaShell , 0xffeef5ff);
        ADD_INNER_COLOR(LColorID::Sienna , 0xff2d52a0);
        ADD_INNER_COLOR(LColorID::Silver , 0xffc0c0c0);
        ADD_INNER_COLOR(LColorID::SkyBlue , 0xffebce87);
        ADD_INNER_COLOR(LColorID::SlateBlue , 0xffcd5a6a);
        ADD_INNER_COLOR(LColorID::SlateGray , 0xff908070);
        ADD_INNER_COLOR(LColorID::SlateGrey , 0xff908070);
        ADD_INNER_COLOR(LColorID::Snow , 0xfffafaff);
        ADD_INNER_COLOR(LColorID::SpringGreen , 0xff7fff00);
        ADD_INNER_COLOR(LColorID::SteelBlue , 0xffb48246);
        ADD_INNER_COLOR(LColorID::Tan , 0xff8cb4d2);
        ADD_INNER_COLOR(LColorID::Teal , 0xff808000);
        ADD_INNER_COLOR(LColorID::Thistle , 0xffd8bfd8);
        ADD_INNER_COLOR(LColorID::Tomato , 0xff4763ff);
        ADD_INNER_COLOR(LColorID::Turquoise , 0xffd0e040);
        ADD_INNER_COLOR(LColorID::Violet , 0xffee82ee);
        ADD_INNER_COLOR(LColorID::Wheat , 0xffb3def5);
        ADD_INNER_COLOR(LColorID::White , 0xffffffff);
        ADD_INNER_COLOR(LColorID::WhiteSmoke , 0xfff5f5f5);
        ADD_INNER_COLOR(LColorID::Yellow , 0xff00ffff);
        ADD_INNER_COLOR(LColorID::YellowGreen , 0xff32cd9a);

#undef ADD_INNER_COLOR
    }
}

LColor::LColor(){

}

LColor::LColor(const LColor &c)
    : m_alpha(c.m_alpha),
    m_red(c.m_red),
    m_green(c.m_green),
    m_blue(c.m_blue)

{

}

LColor::LColor(LColorID id)
{
    colorIdToRgba(id, m_red, m_green, m_blue, m_alpha);
}

LColor::LColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    :
    m_alpha(a),
    m_red(r),
    m_green(g),
    m_blue(b)
{}

void LColor::colorIdToRgba(LColorID id, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a)
{

    if(id < LColorID::Num){
        if(m_innerColors.empty()){
            initInnerColors();
        }

        uint32_t value = m_innerColors[(int)id].value;
        a = (value >> 24);
        b = (value >> 16) & 0xffu;
        g = (value >> 8) & 0xffu;
        r = value & 0xffu;
    }

}

const std::string& LColor::colorIdName(LColorID id)
{
    if(id < LColorID::Num){
        if(m_innerColors.empty()){
            initInnerColors();
        }

        return m_innerColors[(int)id].name;
    }
    return LStringUtil::empty;
}

const std::vector<LNamedVar<uint32_t, LIDynNamedVar> > &LColor::innerColors()
{
    if(m_innerColors.empty()){
        initInnerColors();
    }
    return m_innerColors;
}

LColor LColor::fromColorName(const std::string &name, bool* isValid)
{
    LColor c;
    lcss::StringSection s(LStringUtil::trim(name));
    if(isValid != nullptr){
        *isValid = LCssParser::parseColor(s, c);
    }else{
        LCssParser::parseColor(s, c);
    }
    return c;
}

LColor& LColor::toLighter(int factor)
{
    if (factor <= 0){                                // invalid lightness factor
        return *this;
    }
    else if (factor < 100){                        // makes color darker
        return toDarker(10000 / factor);
    }

    LHsv hsv;
    toHsv(hsv);
    int s = hsv.m_saturation;
    uint32_t v = hsv.m_value;

    v = (factor*v)/100;
    if (v > 255) {
        // overflow... adjust saturation
        s -= v - 255;
        if (s < 0){
            s = 0;
        }
        v = 255;
    }

    hsv.m_saturation = s;
    hsv.m_value = v;

    fromHsv(hsv);
    return *this;
}

LColor& LColor::toDarker(int factor)
{
    if (factor <= 0){                                // invalid darkness factor
        return *this;
    }
    else if (factor < 100){                        // makes color lighter
        return toLighter(10000 / factor);
    }

    LHsv hsv;
    toHsv(hsv);
    hsv.m_value = (hsv.m_value * 100) / factor;
    fromHsv(hsv);
    return *this;
}

void LColor::toHsv(LHsv &hsv) const
{
    hsv.m_alpha = m_alpha;
    const double r = m_red   / 255.0;
    const double g = m_green / 255.0;
    const double b = m_blue  / 255.0;
    const double max = ( r > g && r > b) ? r : (g > b ? g : b) ;
    const double min = ( r < g && r < b) ? r : (g < b ? g : b) ;
    const double delta = max - min;
    hsv.m_value = (uint8_t)std::lround(max * 255);
    if (lFuzzyIsZero(delta)) {
        // achromatic case, hue is undefined
        hsv.m_hue = USHRT_MAX;
        hsv.m_saturation = 0;
    } else {
        // chromatic case
        double hue = 0;
        hsv.m_saturation = (uint8_t)std::lround((delta / max) * 255);
        if (lFuzzyCompare(r, max)) {
            hue = ((g - b) /delta);
        } else if (lFuzzyCompare(g, max)) {
            hue = (double(2.0) + (b - r) / delta);
        } else if (lFuzzyCompare(b, max)) {
            hue = (double(4.0) + (r - g) / delta);
        } else {
            lWarn() << "LColor::toHsv internal error";
        }
        hue *= double(60.0);
        if (hue < double(0.0)){
            hue += double(360.0);
        }
        hsv.m_hue = (uint8_t)std::lround(hue * 100);
    }

}

void LColor::fromHsv(LHsv &hsv)
{
    if (hsv.m_saturation == 0 || hsv.m_hue == USHRT_MAX) {
        // achromatic case
        m_red = m_green = m_blue = hsv.m_value;
        return;
    }

    // chromatic case
    const double h = hsv.m_hue == 36000 ? 0 : hsv.m_hue / 6000.;
    const double s = hsv.m_saturation / 255.0;
    const double v = hsv.m_value / 255.0;
    const int i = int(h);
    const double f = h - i;
    const double p = v * (double(1.0) - s);

    if (i & 1) {
        const double q = v * (double(1.0) - (s * f));

        switch (i) {
        case 1:
            m_red   = (uint8_t)std::lround(q * 255);
            m_green = (uint8_t)std::lround(v * 255);
            m_blue  = (uint8_t)std::lround(p * 255);
            break;
        case 3:
            m_red   = (uint8_t)std::lround(p * 255);
            m_green = (uint8_t)std::lround(q * 255);
            m_blue  = (uint8_t)std::lround(v * 255);
            break;
        case 5:
            m_red   = (uint8_t)std::lround(v * 255);
            m_green = (uint8_t)std::lround(p * 255);
            m_blue  = (uint8_t)std::lround(q * 255);
            break;
        }
    } else {
        const double t = v * (double(1.0) - (s * (double(1.0) - f)));

        switch (i) {
        case 0:
            m_red   = (uint8_t)std::lround(v * 255);
            m_green = (uint8_t)std::lround(t * 255);
            m_blue  = (uint8_t)std::lround(p * 255);
            break;
        case 2:
            m_red   = (uint8_t)std::lround(p * 255);
            m_green = (uint8_t)std::lround(v * 255);
            m_blue  = (uint8_t)std::lround(t * 255);
            break;
        case 4:
            m_red   = (uint8_t)std::lround(t * 255);
            m_green = (uint8_t)std::lround(p * 255);
            m_blue  = (uint8_t)std::lround(v * 255);
            break;
        }
    }
}

std::string LColor::name(NameFormat fmt)
{
    switch (fmt) {
    case HexRgb:{
        return std::format("#{0:06X}", (uint32_t(m_red) << 16)
                                           | (uint32_t(m_green) << 8)
                                           | uint32_t(m_blue));
    }
    default:{
        return std::format("#{0:08X}", (uint32_t(m_alpha) << 24)
                                           | (uint32_t(m_red) << 16)
                                           | (uint32_t(m_green) << 8)
                                           | uint32_t(m_blue));
    }
    }
}

LColor LColor::mixColors(LColor a, LColor b, double ratioA)
{
    if(ratioA >=1){
        return a;
    }else if(ratioA <=0){
        return b;
    }
    double ratioB = 1-ratioA;
    return LColor((uint8_t)std::lround(a.m_red*ratioA + b.m_red*ratioB),
                  (uint8_t)std::lround(a.m_green*ratioA + b.m_green*ratioB),
                  (uint8_t)std::lround(a.m_blue*ratioA + b.m_blue*ratioB),
                  (uint8_t)std::lround(a.m_alpha*ratioA + b.m_alpha*ratioB));
}

bool LColor::operator==(const LColor &c) const
{
    return c.m_alpha == m_alpha
            && c.m_red == m_red
            && c.m_green == m_green
           && c.m_blue == m_blue;
}

uint32_t LColor::premultiply(uint32_t x)
{
    const uint32_t a = (x >> 24);
    uint32_t t = (x & 0xff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff) * a;
    x = (x + ((x >> 8) & 0xff) + 0x80);
    x &= 0xff00;
    return x | t | (a << 24);
}

uint32_t LColor::unpremultiply(uint32_t p)
{
    const uint32_t alpha = (p >> 24);
    // Alpha 255 and 0 are the two most common values, which makes them beneficial to short-cut.
    if (alpha == 255){
        return p;
    }
    if (alpha == 0){
        return 0;
    }
    // (p*(0x00ff00ff/alpha)) >> 16 == (p*255)/alpha for all p and alpha <= 256.
    const uint32_t invAlpha = ljInvPremulFactor[alpha];
    uint32_t red = (p >> 16) & 0xffu;
    uint32_t green = (p >> 8) & 0xffu;
    uint32_t blue = p & 0xffu;

    red = (red*invAlpha+ + 0x8000)>>16;
    green = (green*invAlpha+ + 0x8000)>>16;
    blue = (blue*invAlpha+ + 0x8000)>>16;

    // We add 0x8000 to get even rounding. The rounding also ensures that premultiply(unpremultiply(p)) == p for all p.
    return argbToUInt(red, green, blue, alpha);
}

uint64_t LColor::premultiply(uint64_t rgba)
{
    uint64_t alphaMask = uint64_t(0xffff) << 48;
    if ((rgba & alphaMask) == alphaMask){
        return rgba;
    }
    if ((rgba & alphaMask) == 0){
        return 0;
    }
    const uint64_t a = uint16_t(rgba >> 48);
    uint64_t br = (rgba & uint64_t(0xffff0000ffff)) * a;
    uint64_t ag = ((rgba >> 16) & uint64_t(0xffff0000ffff)) * a;
    br = (br + ((br >> 16) & uint64_t(0xffff0000ffff)) + uint64_t(0x800000008000));
    ag = (ag + ((ag >> 16) & uint64_t(0xffff0000ffff)) + uint64_t(0x800000008000));

    br = (br >> 16) & uint64_t(0xffff0000ffff);
    ag = ag & uint64_t(0xffff0000);
    return uint64_t((a << 48) | br | ag);
}

uint64_t LColor::unpremultiply(uint64_t rgba)
{
    uint64_t alphaMask = uint64_t(0xffff) << 48;
    if ((rgba & alphaMask) == alphaMask){
        return rgba;
    }
    if ((rgba & alphaMask) == 0){
        return 0;
    }

    const uint64_t a = uint16_t(rgba >> 48);
    const uint64_t fa = (uint64_t(0xffff00008000) + a/2) / a;
    const uint16_t r = uint16_t((uint16_t(rgba) * fa + 0x80000000) >> 32);
    const uint16_t g = uint16_t((uint16_t(rgba>>16) * fa + 0x80000000) >> 32);
    const uint16_t b = uint16_t((uint16_t(rgba>>32)  * fa + 0x80000000) >> 32);
    return LColor::argb16ToUInt64(r, g, b, uint16_t(a));
}

uint32_t LColor::unpremultiplyRgb30(uint32_t rgb30)
{
    const uint32_t a = rgb30 >> 30;
    switch (a) {
    case 0:
        return 0;
    case 1: {
        uint32_t rgb = rgb30 & 0x3fffffff;
        rgb *= 3;
        return (a << 30) | rgb;
    }
    case 2: {
        uint32_t rgb = rgb30 & 0x3fffffff;
        rgb += (rgb >> 1) & 0x5ff7fdff;
        return (a << 30) | rgb;
    }
    case 3:
        return rgb30;
    }
    return 0;
}


uint32_t LColor::argb64ToArgb(uint64_t c)
{
    uint8_t r, g, b, a;
    r =  div257(uint16_t(c));
    g =  div257(uint16_t(c>> 16));
    b =  div257(uint16_t(c>> 32));
    a =  div257(uint16_t(c>> 48));
    return argbToUInt(r, g, b, a);
}

uint64_t LColor::argbToArgb64(uint32_t c)
{
    LColor t;
    t.fromUInt(c);
    return t.toUInt64();
}


LK_NAMESPACE_END
