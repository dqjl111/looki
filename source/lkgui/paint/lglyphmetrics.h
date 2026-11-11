#ifndef LGLYPHMETRICS_H
#define LGLYPHMETRICS_H
#include "ltransform2d.h"

LK_NAMESPACE_BEGIN

/**
 * @brief The LGlyphMetrics class
 * 对应windows结构体
 * typedef struct _GLYPHMETRICS {
      UINT  gmBlackBoxX; //完全包围字形的最小矩形的宽度(其黑框) 对应m_width
      UINT  gmBlackBoxY; //完全包围字形的最小矩形的高度(其黑框) 对应m_height
      POINT gmptGlyphOrigin; //完全包围字形的最小矩形左上角的x坐标和y坐标 对应m_x和m_y
      short gmCellIncX; //从当前字符单元格的原点到下一个字符单元格的原点的水平距离 对应m_xoff
      short gmCellIncY; //从当前字符单元格的原点到下一个字符单元格的原点的垂直距离 对应m_yoff
    } GLYPHMETRICS, *LPGLYPHMETRICS;
 */
template<typename T>
class LGlyphMetrics
{
public:
    inline LGlyphMetrics()
        : m_x(100000),  m_y(100000) {}
    inline LGlyphMetrics(T _x, T _y, T _width, T _height, T _xoff, T _yoff)
        : m_x(_x),
        m_y(_y),
        m_width(_width),
        m_height(_height),
        m_xoff(_xoff),
        m_yoff(_yoff)
    {}
    T m_x;
    T m_y;
    T m_width;
    T m_height;
    T m_xoff;
    T m_yoff;

    LGlyphMetrics transformed(const LTransform2D &matrix) const{
        if (matrix.type() < LTransform2D::TxTranslate){
            return *this;
        }

        LGlyphMetrics m = *this;

        double w = m_width;
        double h = m_height;

        LRect<double> t = matrix.mapRect(LRect<double>(0, 0, w, h, false));
        LTransform2D xform =  matrix * LTransform2D::fromTranslate(-t.m_x, -t.m_y);

        LRect<T> rect(0, 0, w, h, false);
        rect = xform.mapRect(rect);
        m.m_width = rect.m_width;
        m.m_height =rect.m_height;

        LLine<T> l = xform.map(LLine<T>(m_x, m_y, m_xoff, m_yoff));

        m.m_x = l.m_pt1.m_x;
        m.m_y = l.m_pt1.m_y;

        // The offset is relative to the baseline which is why we use dx/dy of the line
        m.m_xoff = l.dx();
        m.m_yoff = l.dy();

        return m;
    }
    inline bool isValid() const {return m_x != 100000 && m_y != 100000;}

    inline T leftBearing() const
    {
        if (!isValid()){
            return 0;
        }
        return m_x;
    }

    inline T rightBearing() const
    {
        if (!isValid()){
            return 0;
        }
        return m_xoff - m_x - m_width;
    }
};

LK_NAMESPACE_END
#endif // LGLYPHMETRICS_H
