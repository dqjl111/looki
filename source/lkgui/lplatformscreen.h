#ifndef LPLATFORMSCREEN_H
#define LPLATFORMSCREEN_H
#include "lkcore/lookiconfig.h"
#include "lrect.h"
#include "lkcore/lflags.h"
#include <unordered_map>
#include "lguienums.h"
#include "lplatformcursor.h"

LK_NAMESPACE_BEGIN
class LScreen;
class LWindow;
class LKGUIDEF LPlatformScreen  : public LIVar
{
public:
    LPlatformScreen();

    virtual LRect<int> geometry() const = 0;
    virtual LRect<int> availableGeometry() const { return geometry(); }
    virtual int depth() const = 0;
    //QImage::Format format() const override { return m_data.format; }
    virtual LSize<double> physicalSize() const;
    virtual std::pair<double, double> logicalDpi() const = 0;
    virtual std::pair<double, double> logicalBaseDpi() const = 0;
    virtual double devicePixelRatio() const = 0;
    virtual double refreshRate() const = 0;
    virtual LScreenOrientation orientation() const = 0;
    virtual std::wstring name() const { return L""; }

    const std::shared_ptr<LPlatformScreen> screenForPosition(const LPoint<int> &point) const;
    virtual std::list<std::shared_ptr<LPlatformScreen> > virtualSiblings() const;

    LRect<int> deviceIndependentGeometry() const;

    virtual std::shared_ptr<LWindow> topLevelAt(const LPoint<int> & pos) const;
    std::shared_ptr<LScreen> screen() const;

    static std::pair<double, double> overrideDpi(const std::pair<double, double> &in);
    double scaleFactor() const;
    void setScaleFactor(double factor);

    virtual std::shared_ptr<LPlatformCursor> cursor() const;
protected:
    std::weak_ptr<LScreen> m_screen;
    static int m_overrideDpi;
    friend class LScreen;

    static std::unordered_map<std::wstring, double> m_scaleFactors;
    double m_scaleFactor = 1;
};


LK_NAMESPACE_END
#endif // LPLATFORMSCREEN_H
