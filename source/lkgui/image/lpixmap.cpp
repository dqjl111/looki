#include "lpixmap.h"
#include "lplatformpixmap.h"
#include "lkgui/lguiapplication.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

LPixmap::LPixmap()
{

}

LPixmap::LPixmap(std::shared_ptr<LPlatformPixmap> data): m_data(data)
{

}

LPixmap::LPixmap(int w, int h)
{
    doInit(w, h);
}

std::shared_ptr<LPaintEngine> LPixmap::paintEngine() const
{
    return m_data ? m_data->paintEngine() : nullptr;
}

bool LPixmap::isNull() const
{
    return m_data == nullptr  || m_data->isNull();
}

int LPixmap::width() const
{
    return m_data ? m_data->width() : 0;
}

int LPixmap::height() const
{
    return m_data ? m_data->height() : 0;
}

LSize<int> LPixmap::size() const
{
    return m_data ? LSize<int>(m_data->width(), m_data->height()): LSize<int>();
}

LRect<int> LPixmap::rect() const
{
    return m_data ? LRect<int>(0, 0, m_data->width(), m_data->height(), false) : LRect<int>();
}

LSize<double> LPixmap::sizeF() const
{
    return m_data ? LSize<double>(m_data->width(), m_data->height()): LSize<double>();
}

LRect<double> LPixmap::rectF() const
{
    return m_data ? LRect<double>(0, 0, m_data->width(), m_data->height(), false) : LRect<double>();
}

double LPixmap::devicePixelRatio() const
{
    if(m_data != nullptr){
        return m_data->devicePixelRatio();
    }
    return 1.0;
}

void LPixmap::setDevicePixelRatio(double scaleFactor)
{
    if (isNull()){
        return;
    }

    if (scaleFactor == m_data->devicePixelRatio()){
        return;
    }

    m_data->setDevicePixelRatio(scaleFactor);
}

int LPixmap::depth() const
{
    return m_data ? m_data->depth() : 0;
}

std::shared_ptr<LImage> LPixmap::mask(bool retLsb)
{
    if(m_data == nullptr){
        return nullptr;
    }
    return m_data->mask(retLsb);
}

void LPixmap::setMask(const std::shared_ptr<LImage> mask)
{
    if(m_data == nullptr){
        return;
    }
    m_data->setMask(mask);
}

std::shared_ptr<LPixmap> LPixmap::copy(const LRect<int> &rect) const
{
    if(m_data == nullptr){
        return nullptr;
    }

    LRect<int> r(0, 0, width(), height(), false);
    if (!rect.isEmpty()){
        r = r.intersected(rect);
    }

    std::shared_ptr<LPlatformPixmap> d = m_data->createCompatiblePlatformPixmap();
    d->copy(m_data, r);
    return std::make_shared<LPixmap>(d);
}

std::shared_ptr<LImage> LPixmap::toImage() const
{
    if(m_data){
        return m_data->toImage();
    }
    return std::make_shared<LImage>();
}

std::shared_ptr<LPixmap> LPixmap::fromImage(const std::shared_ptr<LImage> image,
                                            LFlags<LImageConvertFlag> flags)
{
    if (image == nullptr){
        return std::make_shared<LPixmap>();
    }

    if (L_UNLIKELY(LGuiApplication::instance()) == nullptr) {
        lWarn() <<  "LPixmap::fromImage no gui instance:";
        return std::make_shared<LPixmap>();
    }

    std::shared_ptr<LPlatformPixmap> data = LGuiApplication::platformIntegration()->createPlatformPixmap();
    data->fromImage(image, flags);
    return std::make_shared<LPixmap>(data);
}

std::shared_ptr<LPixmap> LPixmap::scaled(const LSize<int> &s, LAspectRatioMode aspectMode, LTransMode mode) const
{
    if (isNull()) {
        return std::make_shared<LPixmap>();
    }
    if (s.isEmpty()){
        return std::make_shared<LPixmap>();
    }

    LSize<int> newSize = size();
    newSize.scale(s, aspectMode);
    newSize.rwidth() = std::max<int>(newSize.width(), 1);
    newSize.rheight() = std::max<int>(newSize.height(), 1);
    if (newSize == size()){
        return getPtrT<LPixmap>();
    }

    LTransform2D wm = LTransform2D::fromScale((double)newSize.width() / (double)width(),
                                          (double)newSize.height() / (double)height());
    std::shared_ptr<LPixmap> pix = transformed(wm, mode);
    return pix;

}

std::shared_ptr<LPixmap> LPixmap::scaledToWidth(int w, LTransMode mode) const
{
    if (isNull()) {
        return std::make_shared<LPixmap>();
    }
    if (w <= 0){
        return std::make_shared<LPixmap>();
    }

    double factor = (double) w / (double)width();
    LTransform2D wm = LTransform2D::fromScale(factor, factor);
    return transformed(wm, mode);
}

std::shared_ptr<LPixmap> LPixmap::scaledToHeight(int h, LTransMode mode) const
{
    if (isNull()) {
        return std::make_shared<LPixmap>();
    }
    if (h <= 0){
        return std::make_shared<LPixmap>();
    }

    double factor = (double) h / (double)height();
    LTransform2D wm = LTransform2D::fromScale(factor, factor);
    return transformed(wm, mode);
}

std::shared_ptr<LPixmap> LPixmap::transformed(const LTransform2D &transform, LTransMode mode) const
{
    if (isNull() || transform.type() <= LTransform2D::TxTranslate){
        return copy();
    }
    return m_data->transformed(transform, mode);
}

void LPixmap::doInit(int w, int h)
{
    m_data = LPlatformPixmap::create(w, h);
}

int LPixmap::metric(Metric m) const
{
    if(m_data != nullptr){
        return m_data->metric(m);
    }
    return 0;
}

LK_NAMESPACE_END
