#include "lhighdpiscaling.h"
#include "lguiapplication.h"
#include "lplatformscreen.h"
#include "lplatformwindow.h"
#include "lscreen.h"

LK_NAMESPACE_BEGIN

double LHighDpiScaling::m_factor = 1;
bool LHighDpiScaling::m_active = false; //"overall active" - is there any scale factor set.
bool LHighDpiScaling::m_usePixelDensity = false; // use scale factor from platform plugin
bool LHighDpiScaling::m_pixelDensityScalingActive = false; // pixel density scale factor > 1
bool LHighDpiScaling::m_globalScalingActive = false; // global scale factor is active
bool LHighDpiScaling::m_screenFactorSet = false; // LHighDpiScaling::setScreenFactor has been used
bool LHighDpiScaling::m_usePhysicalDpi = false;
LHighDpiScaling::HighDpiScaleRoundPolicy LHighDpiScaling::m_highDpiScaleRoundPolicy
    = LHighDpiScaling::HighDpiScaleRoundPolicy::Round;
LHighDpiScaling::DpiAdjustmentPolicy LHighDpiScaling::m_dpiAdjustmentPolicy
    = LHighDpiScaling::DpiAdjustmentPolicy::UpOnly;


void LHighDpiScaling::initHighDpiScaling()
{
    // Determine if there is a global scale factor set.
    m_factor = 1;
    m_globalScalingActive = false;
    m_usePixelDensity = false;
    m_pixelDensityScalingActive = false; //set in updateHighDpiScaling below
    m_active = m_globalScalingActive || m_usePixelDensity;
}

static inline bool usePixelDensity()
{
    if (LCoreApplication::testAttribute(LAppAttribute::DisableHighDpiScaling)){
        return false;
    }
    return LCoreApplication::testAttribute(LAppAttribute::EnableHighDpiScaling);
}

void LHighDpiScaling::updateHighDpiScaling()
{
    if (LCoreApplication::testAttribute(LAppAttribute::DisableHighDpiScaling)){
        m_usePixelDensity = false;
        return;
    }

    m_usePixelDensity = usePixelDensity();

    if (!m_pixelDensityScalingActive) {
        const auto screens = LGuiApplication::screens();
        for (std::shared_ptr<LScreen> screen : screens) {
            std::shared_ptr<LPlatformScreen> pscr = screen->handle();
            if (!lFuzzyIsZero(screenSubfactor(pscr.get())-1)) {
                m_pixelDensityScalingActive = true;
                break;
            }
        }
    }
    m_active = m_globalScalingActive || m_screenFactorSet || m_pixelDensityScalingActive;
}

void LHighDpiScaling::setGlobalFactor(double factor)
{
    if (lFuzzyIsZero(factor-m_factor)){
        return;
    }

    m_globalScalingActive = !lFuzzyIsZero(factor-1);
    m_factor = m_globalScalingActive ? factor : double(1);
    m_active = m_globalScalingActive || m_screenFactorSet || m_pixelDensityScalingActive;
    const auto screens = LGuiApplication::screens();
    for (std::shared_ptr<LScreen> screen : screens){
        screen->updateHighDpi();
    }
}

void LHighDpiScaling::setScreenFactor(LScreen *screen, double factor)
{
    if (!lFuzzyIsZero(factor-1)) {
        m_screenFactorSet = true;
        m_active = true;
    }
    screen->setScaleFactor(factor);

    // hack to force re-evaluation of screen geometry
    std::shared_ptr<LPlatformScreen> handle = screen->handle();
    if (handle != nullptr){
        screen->setPlatformScreen(handle);
    }
}

LHighDpiScaling::ScaleAndOrigin LHighDpiScaling::scaleAndOrigin(const LPlatformScreen *platformScreen, LPoint<int> *nativePosition)
{
    if (!m_active){
        return { double(1), LPoint<int>() };
    }
    if (!platformScreen){
        return { m_factor, LPoint<int>() }; // the global factor
    }
    return { m_factor * screenSubfactor(platformScreen), platformScreen->geometry().topLeft() };
}

LHighDpiScaling::ScaleAndOrigin LHighDpiScaling::scaleAndOrigin(const LScreen *screen, LPoint<int> *nativePosition)
{
    if (!m_active){
        return { double(1), LPoint<int>() };
    }
    if (!screen){
        return { m_factor, LPoint<int>() }; // the global factor
    }
    std::shared_ptr<LPlatformScreen> pscr = screen->handle();
    return scaleAndOrigin(pscr.get(), nativePosition);
}

LHighDpiScaling::ScaleAndOrigin LHighDpiScaling::scaleAndOrigin(const LWindow *window, LPoint<int> *nativePosition)
{
    if (!m_active){
        return { double(1), LPoint<int>() };
    }

    std::shared_ptr<LScreen> screen = window ? window->screen() : LGuiApplication::primaryScreen();
    const bool searchScreen = !window || window->isTopLevel();
    return scaleAndOrigin(screen.get(), searchScreen ? nativePosition : nullptr);
}

LPoint<int> LHighDpiScaling::mapPositionFromNative(const LPoint<int> &pos, const LPlatformScreen *platformScreen)
{
    if (platformScreen == nullptr){
        return pos;
    }
    const double scaleFactor = factor(platformScreen);
    const LPoint<int> topLeft = platformScreen->geometry().topLeft();
    return (pos - topLeft) / scaleFactor + topLeft;
}

LPoint<int> LHighDpiScaling::mapPositionToNative(const LPoint<int> &pos, const LPlatformScreen *platformScreen)
{
    if (platformScreen == nullptr){
        return pos;
    }
    const double scaleFactor = factor(platformScreen);
    const LPoint<int> topLeft = platformScreen->geometry().topLeft();
    return (pos - topLeft) * scaleFactor + topLeft;
}

LPoint<int> LHighDpiScaling::mapPositionToGlobal(const LPoint<int> &pos, const LPoint<int> &windowGlobalPosition, const LWindow *window)
{
    LPoint<int> globalPosCandidate = pos + windowGlobalPosition;
    if (LGuiApplication::m_screens.size() <= 1){
        return globalPosCandidate;
    }

    // The global position may be outside device independent screen geometry
    // in cases where a window spans screens. Detect this case and map via
    // native coordinates to the correct screen.
    std::shared_ptr<LScreen> currentScreen = window->screen();
    if (currentScreen != nullptr){
        if(!currentScreen->m_geometry.contains(globalPosCandidate)) {
            auto nativeGlobalPos = LHighDpi::toNativePixels(globalPosCandidate, currentScreen.get());
            if (auto actualPlatformScreen = currentScreen->handle()->screenForPosition(nativeGlobalPos)){
                return LHighDpi::fromNativePixels(nativeGlobalPos, actualPlatformScreen->screen().get());
            }
        }
    }
    return globalPosCandidate;
}

LPoint<int> LHighDpiScaling::mapPositionFromGlobal(const LPoint<int> &pos, const LPoint<int> &windowGlobalPosition, const LWindow *window)
{
    LPoint<int> windowPosCandidate = pos - windowGlobalPosition;
    if (LGuiApplication::m_screens.size() <= 1 || window->handle() == nullptr){
        return windowPosCandidate;
    }

    // Device independent global (screen) space may discontiguous when high-dpi scaling
    // is active. This means that the normal subtracting of the window global position from the
    // position-to-be-mapped may not work in cases where a window spans multiple screens.
    // Map both positions to native global space (using the correct screens), subtract there,
    // and then map the difference back using the scale factor for the window.
    std::shared_ptr<LScreen> posScreen = LGuiApplication::screenAt(pos);
    if (posScreen != nullptr){
        if(posScreen != window->screen()) {
            LPoint<int> nativePos = LHighDpi::toNativePixels(pos, posScreen.get());
            LPoint<int> windowNativePos = window->handle()->geometry().topLeft();
            return LHighDpi::fromNativeLocalPosition(nativePos - windowNativePos, window);
        }
    }
    return windowPosCandidate;
}

std::pair<double, double> LHighDpiScaling::logicalDpi(const LScreen *screen)
{
    if (screen == nullptr){
        return std::pair<double, double>(96.0, 96.0);
    }

    std::shared_ptr<LPlatformScreen>  ps = screen->handle();
    if(ps == nullptr){
        return std::pair<double, double>(96.0, 96.0);
    }

    if (!m_usePixelDensity) {
        const double screenScaleFactor = screenSubfactor(ps.get());
        const std::pair<double, double> dpi = LPlatformScreen::overrideDpi(screen->handle()->logicalDpi());
        return std::pair<double, double>(dpi.first / screenScaleFactor, dpi.second / screenScaleFactor );
    }

    const double scaleFactor = rawScaleFactor(ps.get());
    const double roundedScaleFactor = roundScaleFactor(scaleFactor);
    return effectiveLogicalDpi(ps.get(), scaleFactor, roundedScaleFactor);
}

double LHighDpiScaling::rawScaleFactor(const LPlatformScreen *screen)
{
    double factor;
    std::pair<double, double> platformBaseDpi = screen->logicalBaseDpi();
    if (m_usePhysicalDpi) {
        LSize<int> sz = screen->geometry().size();
        LSize<double> psz = screen->physicalSize();
        double platformPhysicalDpi = ((sz.height() / psz.height()) + (sz.width() / psz.width())) * (25.4 * 0.5);
        factor = double(platformPhysicalDpi) / double(platformBaseDpi.first);
    } else {
        const std::pair<double, double> platformLogicalDpi
            = LPlatformScreen::overrideDpi(screen->logicalDpi());
        factor = double(platformLogicalDpi.first) / double(platformBaseDpi.first);
    }

    return factor;
}

double LHighDpiScaling::roundScaleFactor(double rawFactor)
{
    auto scaleFactorRoundPolicy = m_highDpiScaleRoundPolicy;
    // Apply rounding policy.
    double roundedFactor = rawFactor;
    switch (scaleFactorRoundPolicy) {
    case HighDpiScaleRoundPolicy::Round:
        roundedFactor = std::round(rawFactor);
        break;
    case HighDpiScaleRoundPolicy::Ceil:
        roundedFactor = std::ceil(rawFactor);
        break;
    case HighDpiScaleRoundPolicy::Floor:
        roundedFactor = std::floor(rawFactor);
        break;
    case HighDpiScaleRoundPolicy::RoundPreferFloor:
        // Round up for .75 and higher. This favors "small UI" over "large UI".
        roundedFactor = rawFactor - std::floor(rawFactor) < 0.75
                            ? std::floor(rawFactor) : std::ceil(rawFactor);
        break;
    case HighDpiScaleRoundPolicy::PassThrough:
    case HighDpiScaleRoundPolicy::Unset:
        break;
    }

    if (scaleFactorRoundPolicy != HighDpiScaleRoundPolicy::PassThrough){
        roundedFactor = std::max(roundedFactor, double(1));
    }
    return roundedFactor;
}

std::pair<double, double> LHighDpiScaling::effectiveLogicalDpi(const LPlatformScreen *screen, double rawFactor, double roundedFactor)
{
    auto dpiAdjustmentPolicy = m_dpiAdjustmentPolicy;
    // Apply adjustment policy.
    const std::pair<double, double> baseDpi = screen->logicalBaseDpi();
    const double dpiAdjustmentFactor = rawFactor / roundedFactor;

    // Return the base DPI for cases where there is no adjustment
    if (dpiAdjustmentPolicy == DpiAdjustmentPolicy::Disabled){
        return baseDpi;
    }
    if (dpiAdjustmentPolicy == DpiAdjustmentPolicy::UpOnly && dpiAdjustmentFactor < 1){
        return baseDpi;
    }

    return std::pair<double, double>(baseDpi.first * dpiAdjustmentFactor, baseDpi.second * dpiAdjustmentFactor);
}

double LHighDpiScaling::screenSubfactor(const LPlatformScreen *screen)
{
    double factor = 1;
    if (!screen){
        return factor;
    }

    bool screenPropertyUsed = false;
    if (m_screenFactorSet) {
        factor = screen->scaleFactor();
    }

    if (!screenPropertyUsed && m_usePixelDensity){
        factor = roundScaleFactor(rawScaleFactor(screen));
    }
    return factor;
}



LK_NAMESPACE_END
