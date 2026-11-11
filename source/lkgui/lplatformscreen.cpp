#include "lplatformscreen.h"
#include "lhighdpiscaling.h"
#include "lguiapplication.h"
#include "lrect.h"

LK_NAMESPACE_BEGIN

int LPlatformScreen::m_overrideDpi =  -1;
std::unordered_map<std::wstring, double> LPlatformScreen::m_scaleFactors;

LPlatformScreen::LPlatformScreen()
{

}

LSize<double> LPlatformScreen::physicalSize() const
{
    static const int dpi = 100;
    return geometry().size().toSizeF() / dpi * double(25.4);
}

const std::shared_ptr<LPlatformScreen> LPlatformScreen::screenForPosition(const LPoint<int> &point) const
{
    if (!geometry().contains(point)) {
        const auto screens = virtualSiblings();
        for (const std::shared_ptr<LPlatformScreen> screen : screens) {
            if (screen->geometry().contains(point)){
                return screen;
            }
        }
    }
    return getPtrT<LPlatformScreen>();
}

std::list<std::shared_ptr<LPlatformScreen> > LPlatformScreen::virtualSiblings() const
{
    std::list<std::shared_ptr<LPlatformScreen> > list;
    list.push_back(getPtrT<LPlatformScreen>());
    return list;
}

LRect<int> LPlatformScreen::deviceIndependentGeometry() const
{
    double scaleFactor = LHighDpiScaling::factor(this);
    LRect<int> nativeGeometry = geometry();
    return LRect<int>(nativeGeometry.m_x, nativeGeometry.m_y,
                      LHighDpi::fromNative(nativeGeometry.size(), scaleFactor));
}

std::shared_ptr<LWindow> LPlatformScreen::topLevelAt(const LPoint<int> &pos) const
{
    const std::vector<std::shared_ptr<LWindow> > wins = LGuiApplication::topLevelWindows();
    for (int i = wins.size()-1; i >= 0; --i) {
        std::shared_ptr<LWindow> w = wins[i];
        if (w->visible() && LHighDpi::toNativePixels<LRect<int>>(w->geometry(), w.get()).contains(pos)){
            return w;
        }
    }

    return nullptr;
}

std::shared_ptr<LScreen> LPlatformScreen::screen() const
{
    return m_screen.lock();
}

std::pair<double, double> LPlatformScreen::overrideDpi(const std::pair<double, double> &in)
{
    return m_overrideDpi > 0 ?
               std::pair<double, double>(m_overrideDpi, m_overrideDpi) :
               in;
}

double LPlatformScreen::scaleFactor() const
{
    std::wstring t = name();
    if(!t.empty()){
        std::unordered_map<std::wstring, double>::const_iterator iter
            = m_scaleFactors.find(t);
        if(iter != m_scaleFactors.end()){
            return iter->second;
        }
    }
    return m_scaleFactor;
}

void LPlatformScreen::setScaleFactor(double factor)
{
    std::wstring t = name();
    if(!t.empty()){
        m_scaleFactors[t] = factor;
    }
    m_scaleFactor = factor;
}

std::shared_ptr<LPlatformCursor> LPlatformScreen::cursor() const
{
    return nullptr;
}



LK_NAMESPACE_END
