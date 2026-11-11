#include "lpaintdevice.h"
#include "lkcore/ldebug.h"
LK_NAMESPACE_BEGIN

LPaintDevice::LPaintDevice()
{

}


int LPaintDevice::metric(Metric m) const
{
    if (m == PdmDevicePixelRatioScaled){
        return this->metric(PdmDevicePixelRatio) * devicePixelRatioFScale();
    }

    lWarn() <<"LPaintDevice::metrics: Device has no metric information";

    if (m == PdmDpiX) {
        return 72;
    } else if (m == PdmDpiY) {
        return 72;
    } else if (m == PdmNumColors) {
        return 256;
    } else if (m == PdmDevicePixelRatio) {
        return 1;
    } else {
        lDebug()<< "Unrecognised metric " << int(m);
        return 0;
    }
}

LK_NAMESPACE_END
