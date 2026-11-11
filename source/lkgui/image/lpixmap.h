#ifndef LPIXMAP_H
#define LPIXMAP_H
#include "lkgui/paint/lpaintdevice.h"
#include "lkgui/paint/ltransform2d.h"

LK_NAMESPACE_BEGIN
class LImage;
class LPlatformPixmap;
class LKGUIDEF LPixmap: public LPaintDevice
{
public:
    LPixmap();
    LPixmap(std::shared_ptr<LPlatformPixmap> data);
    LPixmap(int w, int h);

    inline virtual int devType() const override{
        return PDtPixmap;
    }

    virtual std::shared_ptr<LPaintEngine> paintEngine() const override;

    bool isNull() const;
    int width() const;
    int height() const;
    LSize<int> size() const;
    LRect<int> rect() const;
    LSize<double> sizeF() const;
    LRect<double> rectF() const;

    double devicePixelRatio() const;
    void setDevicePixelRatio(double scaleFactor);

    int depth() const;

    inline std::shared_ptr<LPlatformPixmap> handle() const{
        return m_data;
    }

    std::shared_ptr<LImage> mask(bool retLsb = false);
    void setMask(const std::shared_ptr<LImage> mask);

    std::shared_ptr<LPixmap> copy(const LRect<int> &rect = LRect<int>()) const;
    std::shared_ptr<LImage> toImage() const;
    static std::shared_ptr<LPixmap> fromImage(const std::shared_ptr<LImage> image,
                                              LFlags<LImageConvertFlag> flags = LImageConvertFlag::AutoColor);

    std::shared_ptr<LPixmap> scaled(const LSize<int> &s,
                                   LAspectRatioMode aspectMode = LAspectRatioMode::IgnoreAspectRatio,
                                   LTransMode mode = LTransMode::Fast) const;
    std::shared_ptr<LPixmap> scaledToWidth(int w, LTransMode mode = LTransMode::Fast) const;
    std::shared_ptr<LPixmap> scaledToHeight(int h, LTransMode mode = LTransMode::Fast) const;

    std::shared_ptr<LPixmap> transformed(const LTransform2D &matrix,
                                         LTransMode mode = LTransMode::Fast) const;
protected:
    void doInit(int w, int h);
    virtual int metric(Metric ) const override;

    std::shared_ptr<LPlatformPixmap> m_data;
};

LK_NAMESPACE_END

#endif // LPIXMAP_H
