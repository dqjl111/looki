#ifndef LSCREEN_H
#define LSCREEN_H
#include "lplatformscreen.h"
#include "lkcore/lflags.h"
#include "lkcore/lsignal.h"
#include "lrect.h"
#include "lhighdpiscaling.h"

LK_NAMESPACE_BEGIN
class LKGUIDEF LScreen : public LObject
{
public:
    explicit LScreen();
    void setPlatformScreen(std::shared_ptr<LPlatformScreen> screen);
    void updateHighDpi()
    {
        m_geometry = m_platformScreen->deviceIndependentGeometry();
        m_availableGeometry = LHighDpi::fromNative(m_platformScreen->geometry(),
                                                   LHighDpiScaling::factor(m_platformScreen.get()),
                                                   m_geometry.topLeft());
    }

    void updatePrimaryOrientation()
    {
        m_primaryOrientation = m_geometry.width() >= m_geometry.height()
                                   ? LScreenOrientation::Landscape
                                   : LScreenOrientation::Portrait;
    }


    std::shared_ptr<LPlatformScreen> handle() const;
    std::wstring screenName() const;

    LFlags<LScreenOrientation> orientationUpdateMask() const;
    void setOrientationUpdateMask(LFlags<LScreenOrientation> mask);

    double scaleFactor();
    void setScaleFactor(double factor);
    void updateGeometriesWithSignals();
    double physicalDotsPerInchX() const;
    double physicalDotsPerInchY() const;
    double physicalDotsPerInch() const;
    double logicalDotsPerInchX() const;
    double logicalDotsPerInchY() const;
    double logicalDotsPerInch() const;

    double devicePixelRatio() const;

    inline LSize<int> size() const{
        return m_geometry.size();
    }
    inline LSize<double> physicalSize() const{
        if(m_platformScreen != nullptr){
            return m_platformScreen->geometry().size().toSizeF();
        }
        return LSize<double>();
    }

    LScreenOrientation m_orientation = LScreenOrientation::Primary;
    LScreenOrientation m_filteredOrientation = LScreenOrientation::Primary;
    LScreenOrientation m_primaryOrientation = LScreenOrientation::Landscape;
    LRect<int> m_geometry;
    LRect<int> m_availableGeometry;
    std::pair<double, double> m_logicalDpi = {96, 96};
    double m_refreshRate = 60;


    LSignal<LRect<int>> sigGeometryChanged;  //geometry
    LSignal<LRect<int>> sigAvailableGeometryChanged; //geometry
    LSignal<LSize<double>> sigPhysicalSizeChanged; //size
    LSignal<double> sigPhysicalDotsPerInchChanged; //dpi
    LSignal<double> sigLogicalDotsPerInchChanged; //dpi
//    LSignal<LRect<int>> sigVirtualGeometryChanged; //rect
    LSignal<LScreenOrientation> sigPrimaryOrientationChanged; //orientation
    LSignal<LScreenOrientation> sigOrientationChanged;
    LSignal<double> sigRefreshRateChanged; //refreshRate;

    void emitGeometryChangeSignals(bool geometryChanged, bool availableGeometryChanged);
private:
    std::shared_ptr<LPlatformScreen> m_platformScreen;
    LFlags<LScreenOrientation> m_orientationUpdateMask;

    static void init(std::shared_ptr<LPlatformScreen> ps, std::shared_ptr<LScreen> scr);
    friend class LWindowSystemInterface;
};

LK_NAMESPACE_END
#endif // LSCREEN_H
