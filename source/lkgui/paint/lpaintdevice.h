#ifndef LPAINTDEVICE_H
#define LPAINTDEVICE_H
#include "lkgui/lrect.h"
#include "lkcore/livar.h"
#include <functional>

LK_NAMESPACE_BEGIN

class LPaintEngine;
class LPainter;
class LKGUIDEF LPaintDevice : public LIVar
{
public:
    LPaintDevice();

    enum Metric {
        PdmWidth = 1,
        PdmHeight,
        PdmWidthMM,
        PdmHeightMM,
        PdmNumColors,
        PdmDepth,
        PdmDpiX,
        PdmDpiY,
        PdmPhysicalDpiX,
        PdmPhysicalDpiY,
        PdmDevicePixelRatio,
        PdmDevicePixelRatioScaled
    };

    enum DevType {
        PDtUnknown = 0x00,
        PDtPixmap        = 0x01,
        PDtImage         = 0x02
    };

    inline virtual int devType() const{
        return PDtUnknown;
    }
    bool paintingActive() const{
        return m_painters != 0;
    }
    virtual std::shared_ptr<LPaintEngine> paintEngine() const = 0;

    int width() const { return metric(PdmWidth); }
    int height() const { return metric(PdmHeight); }
    int widthMM() const { return metric(PdmWidthMM); }
    int heightMM() const { return metric(PdmHeightMM); }
    int logicalDpiX() const { return metric(PdmDpiX); }
    int logicalDpiY() const { return metric(PdmDpiY); }
    int physicalDpiX() const { return metric(PdmPhysicalDpiX); }
    int physicalDpiY() const { return metric(PdmPhysicalDpiY); }
    int devicePixelRatio() const { return metric(PdmDevicePixelRatio); }
    double devicePixelRatioF()  const { return metric(PdmDevicePixelRatioScaled) / devicePixelRatioFScale(); }
    int colorCount() const { return metric(PdmNumColors); }
    int depth() const { return metric(PdmDepth); }

    double dpiScaled(double value) const{
        return lDpiScaled(value, logicalDpiX());
    }

    static inline double devicePixelRatioFScale() { return 0x10000; }

    inline void setStartPos(const double x, const double y){
        m_startPos.m_x = x;
        m_startPos.m_y = y;
    }

    LPoint<double> m_startPos;
    std::function<void(LPainter*)> m_initPaintFun;
protected:
    virtual int metric(Metric v) const;  
    uint16_t m_painters = 0;

    friend class LPainter;
};


LK_NAMESPACE_END
#endif // LPAINTDEVICE_H
