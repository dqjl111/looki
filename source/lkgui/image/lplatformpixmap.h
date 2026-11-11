#ifndef LPLATFORMPIXMAP_H
#define LPLATFORMPIXMAP_H
#include "lkgui/paint/lpaintengine.h"
#include "lkgui/paint/lpaintdevice.h"

LK_NAMESPACE_BEGIN

class LPlatformScreen;
class LKGUIDEF LPlatformPixmap
{
public:
    LPlatformPixmap();

    virtual std::shared_ptr<LPaintEngine> paintEngine() const = 0;
    virtual bool resize(int width, int height) = 0;

    static std::shared_ptr<LPlatformPixmap> create(int w, int h);

    inline int width() const { return m_width; }
    inline int height() const { return m_height; }
    inline int depth() const { return m_depth; }

    virtual std::shared_ptr<LImage> mask(bool retLsb) const;
    virtual void setMask(const std::shared_ptr<LImage> mask);

    virtual bool hasAlphaChannel() const = 0;
    virtual double devicePixelRatio() const = 0;
    virtual void setDevicePixelRatio(double scaleFactor) = 0;

    virtual std::shared_ptr<LPlatformPixmap>  createCompatiblePlatformPixmap() const;
    virtual void copy(const std::shared_ptr<LPlatformPixmap> data, const LRect<int> &rect);

    virtual int metric(LPaintDevice::Metric m) const = 0;
    virtual void fromImage(const std::shared_ptr<LImage> image,
                           LFlags<LImageConvertFlag> flags) = 0;

    virtual std::shared_ptr<LImage> toImage() const = 0;
    virtual std::shared_ptr<LImage> toImage(const LRect<int> &rect) const;
    virtual bool isNull()  = 0;

    virtual std::shared_ptr<LPixmap> transformed(const LTransform2D &matrix,
                                                 LTransMode mode = LTransMode::Fast) const;
protected:
    int m_width = 0;
    int m_height = 0;
    int m_depth = 0;
};

LK_NAMESPACE_END

#endif // LPLATFORMPIXMAP_H
