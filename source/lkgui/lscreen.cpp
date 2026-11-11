#include "lscreen.h"
#include "lplatformscreen.h"
#include "lguiapplication.h"

LK_NAMESPACE_BEGIN

LScreen::LScreen()
{
}

void LScreen::init(std::shared_ptr<LPlatformScreen> ps, std::shared_ptr<LScreen> scr)
{
    ps->m_screen = scr;
    scr->setPlatformScreen(ps);
}

void LScreen::setPlatformScreen(std::shared_ptr<LPlatformScreen> screen)
{
    m_platformScreen = screen;

    m_orientation = screen->orientation();
    m_logicalDpi = LPlatformScreen::overrideDpi(screen->logicalDpi());

    m_refreshRate = screen->refreshRate();
    if (m_refreshRate < 1.0){
        m_refreshRate = 60.0;
    }
    updateHighDpi();
    updatePrimaryOrientation(); // derived from the geometry

    m_filteredOrientation = m_orientation;
    if (m_filteredOrientation == LScreenOrientation::Primary){
        m_filteredOrientation = m_primaryOrientation;
    }
}

std::shared_ptr<LPlatformScreen> LScreen::handle() const
{
    return m_platformScreen;
}

std::wstring LScreen::screenName() const
{
    return m_platformScreen->name();
}

LFlags<LScreenOrientation> LScreen::orientationUpdateMask() const
{
    return m_orientationUpdateMask;
}

void LScreen::setOrientationUpdateMask(LFlags<LScreenOrientation> mask)
{
    m_orientationUpdateMask = mask;
    std::shared_ptr<LScreen> t = getPtrT<LScreen>();
    LGuiApplication::updateFilteredScreenOrientation(t);
}

double LScreen::scaleFactor()
{
    return m_platformScreen->scaleFactor();
}

void LScreen::setScaleFactor(double factor)
{
    m_platformScreen->setScaleFactor(factor);
}

void LScreen::updateGeometriesWithSignals()
{
    const LRect<int> oldGeometry = m_geometry;
    const LRect<int> oldAvailableGeometry = m_availableGeometry;
    updateHighDpi();
    emitGeometryChangeSignals(oldGeometry != m_geometry, oldAvailableGeometry != m_availableGeometry);

}

double LScreen::physicalDotsPerInchX() const
{
    return size().width() / physicalSize().width() * double(25.4);
}

double LScreen::physicalDotsPerInchY() const
{
    return size().height() / physicalSize().height() * double(25.4);
}

double LScreen::physicalDotsPerInch() const
{
    LSize<int> sz = m_geometry.size();
    LSize<double> psz = physicalSize();
    return ((sz.height() / psz.height()) + (sz.width() / psz.width())) * double(25.4 * 0.5);
}

double LScreen::logicalDotsPerInchX() const
{
    if (LHighDpiScaling::isActive()){
        return LHighDpiScaling::logicalDpi(this).first;
    }
    return m_logicalDpi.first;
}

double LScreen::logicalDotsPerInchY() const
{
    if (LHighDpiScaling::isActive()){
        return LHighDpiScaling::logicalDpi(this).second;
    }
    return m_logicalDpi.second;

}

double LScreen::logicalDotsPerInch() const
{
    if(LHighDpiScaling::isActive()){
        std::pair<double, double> dpi = LHighDpiScaling::logicalDpi(this);
        return (dpi.first + dpi.second) * double(0.5);
    }else{
        return (m_logicalDpi.first + m_logicalDpi.second) * double(0.5);;
    }
}

double LScreen::devicePixelRatio() const
{
    if(m_platformScreen != nullptr){
        return m_platformScreen->devicePixelRatio() *LHighDpiScaling::factor(this);
    }
    return 1.0;
}

void LScreen::emitGeometryChangeSignals(bool geometryChanged, bool availableGeometryChanged)
{
    if (geometryChanged){
        sigGeometryChanged.emt(m_geometry);
    }

    if (availableGeometryChanged){
        sigAvailableGeometryChanged.emt(m_availableGeometry);
    }

//    if (geometryChanged || availableGeometryChanged) {
//        const auto siblings = m_virtualSiblings;
//        for (std::shared_ptr<LScreen> sibling : siblings){
//            sibling->sigVirtualGeometryChanged.emt(sibling->m_virtualGeometry);
//        }
//    }

    if (geometryChanged){
        sigPhysicalDotsPerInchChanged.emt(physicalDotsPerInch());
    }
}




LK_NAMESPACE_END
