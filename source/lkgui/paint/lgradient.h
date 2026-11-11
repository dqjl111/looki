#ifndef LGRADIENT_H
#define LGRADIENT_H
#include "lkgui/paint/lcolor.h"
#include "lkgui/lpoint.h"
#include <memory>

LK_NAMESPACE_BEGIN

typedef std::pair<double, LColor> LGradientStop;
typedef std::list<LGradientStop> LGradientStops;

#define GRADIENT_STOPTABLE_SIZE 1024
#define GRADIENT_STOPTABLE_SIZE_SHIFT 10


struct LLinearGradientData
{
    struct {
        double x = 0;
        double y = 0;
    } origin;
    struct {
        double x = 0;
        double y = 0;
    } end;
};

struct LRadialGradientData
{
    struct {
        double x = 0;
        double y = 0;
        double radius = 0;
    } center;
    struct {
        double x = 0;
        double y = 0;
        double radius = 0;
    } focal;
};

struct LConicalGradientData
{
    struct {
        double x = 0;
        double y = 0;
    } center;
    double angle = 0; //弧度,LGradient中m_spread值为Repeat才能生效
};

class LImage;
class LKGUIDEF LGradient : public LIVar
{
public:
    LGradient();

    enum class Type  : uint8_t{
        No,
        Linear,
        Radial,
        Conical,
    };

    enum class Spread  : uint8_t{
        Pad,
        Reflect,
        Repeat
    };

    enum class Mode  : uint8_t{
        Logical,
        StretchToDevice,
        ObjectBounding,
        Object
    };

    enum class Interpolation : uint8_t{
        Color,
        Component
    };


    union {
        LLinearGradientData m_linear;
        LRadialGradientData m_radial;
        LConicalGradientData m_conical;
        uint8_t m_data[sizeof(LRadialGradientData)];
    };

    struct{
        union{
            uint8_t m_flags = 0;
            struct{
                Type m_type : 2;
                Spread m_spread : 2;
                Mode m_coordinate : 2;
                Interpolation m_interpolation :1;
                bool m_hasAlpha : 1;
            };
        };
        bool m_dirty : 1;
    };


    LGradientStops m_stops;
    std::shared_ptr<uint32_t[GRADIENT_STOPTABLE_SIZE]> m_colorTable; //[GRADIENT_STOPTABLE_SIZE];

    std::shared_ptr<LImage> toImage(const LSize<int> s= LSize<int>(GRADIENT_STOPTABLE_SIZE, GRADIENT_STOPTABLE_SIZE)) const;
    void ensureColorTable();

    LGradient& operator=(const LGradient &b);

    bool operator==(const LGradient &b) const;
    inline bool operator!=(const LGradient &b) const { return !(operator==(b)); }

    LSize<int> suggestSize();
    void setColorAt(double pos, const LColor &color);

    static LPoint<double> radialAdaptFocalPoint(const LPoint<double> &center, double radius, const LPoint<double> &focalPoint);
    std::string toCss();
private:
    void generateColorTable();
    uint64_t stopColorValue(LColor c, bool needPremultiply);
};


LK_NAMESPACE_END

#endif // LGRADIENT_H
