#include "lplatformpixmap.h"
#include "lkgui/lplatformintegration.h"
#include "lkgui/lguiapplication.h"
#include "lkcore/ldebug.h"
LK_NAMESPACE_BEGIN

LPlatformPixmap::LPlatformPixmap()
{

}

std::shared_ptr<LPlatformPixmap> LPlatformPixmap::create(int w, int h)
{
    std::shared_ptr<LPlatformPixmap>  data = LGuiApplication::platformIntegration()->createPlatformPixmap();
    if(data!= nullptr){
        data->resize(w, h);
    }
    return data;
}

std::shared_ptr<LImage> LPlatformPixmap::mask(bool retLsb) const
{
    if (!hasAlphaChannel()){
        return nullptr;
    }

    std::shared_ptr<LImage> img = toImage();
    if(img->format() != LImage::Format::ARGB32 && img->format() != LImage::Format::ARGB32Premultiplied){
        img = img->convertToFormat(LImage::Format::ARGB32Premultiplied);
    }

    const int w = img->width();
    const int h = img->height();

    std::shared_ptr<LImage> mask;
    if(retLsb){
        mask = std::make_shared<LImage>(w, h, LImage::Format::Mono);
    }else{
        mask = std::make_shared<LImage>(w, h, LImage::Format::MonoLSB);
    }
    if (mask->isNull()){ // allocation failed
        return nullptr;
    }

    mask->setDevicePixelRatio(devicePixelRatio());

    const int bpl = mask->bytesPerLine();

    if(retLsb){
        for (int y = 0; y < h; ++y) {
            const uint32_t* src = (uint32_t*)img->scanLine(y);
            uint8_t *dest = mask->scanLine(y);
            memset(dest, 0, bpl);
            for (int x = 0; x < w; ++x) {
                if (LColor::uintToAlpha(*src) > 0){
                    dest[x >> 3] |= (1 << (x & 7));
                }
                ++src;
            }
        }
    }else{
        for (int y = 0; y < h; ++y) {
            const uint32_t* src = (uint32_t*)img->scanLine(y);
            uint8_t *dest = mask->scanLine(y);
            memset(dest, 0, bpl);
            for (int x = 0; x < w; ++x) {
                if (LColor::uintToAlpha(*src) == 0){
                    dest[x >> 3] |= (1 << (x & 7));
                }
                ++src;
            }
        }
    }
    return mask;
}

void LPlatformPixmap::setMask(const std::shared_ptr<LImage> mask)
{
    if (mask == nullptr || mask->size().isEmpty()) {
        return;
    } else {
        std::shared_ptr<LImage> image = toImage();

        const int w = image->width();
        const int h = image->height();

        switch (image->depth()) {
        case 1: {
            const std::shared_ptr<LImage> imageMask = mask->convertToFormat(image->format(),
                                                                            LImageConvertFlag::AutoColor,
                                                                            true);
            for (int y = 0; y < h; ++y) {
                const uint8_t *mscan = imageMask->scanLine(y);
                uint8_t *tscan = image->scanLine(y);
                int bytesPerLine = image->bytesPerLine();
                for (int i = 0; i < bytesPerLine; ++i){
                    tscan[i] &= mscan[i];
                }
            }
            break;
        }
        default: {
            if(mask->format() == LImage::Format::Mono){
                for (int y = 0; y < h; ++y) {
                    const uint8_t *mscan = mask->scanLine(y);
                    uint32_t * tscan = (uint32_t *)image->scanLine(y);
                    for (int x = 0; x < w; ++x) {
                        if (mscan[x>>3] & (1 << (x&7))){
                            tscan[x] = 0;
                        }
                    }
                }
            }else{
                const std::shared_ptr<LImage> imageMask = mask->convertToFormat(LImage::Format::MonoLSB,
                                                                                LImageConvertFlag::AutoColor,
                                                                                true);
                image = image->convertToFormat(LImage::Format::ARGB32Premultiplied,
                                               LImageConvertFlag::AutoColor,
                                               true);
                for (int y = 0; y < h; ++y) {
                    const uint8_t *mscan = imageMask->scanLine(y);
                    uint32_t * tscan = (uint32_t *)image->scanLine(y);
                    for (int x = 0; x < w; ++x) {
                        if (!(mscan[x>>3] & (1 << (x&7)))){
                            tscan[x] = 0;
                        }
                    }
                }
            }
            break;
        }
        }
        fromImage(image, LImageConvertFlag::AutoColor);
    }
}

std::shared_ptr<LPlatformPixmap> LPlatformPixmap::createCompatiblePlatformPixmap() const
{
    std::shared_ptr<LPlatformPixmap>  data = LGuiApplication::platformIntegration()->createPlatformPixmap();
    return data;
}

void LPlatformPixmap::copy(const std::shared_ptr<LPlatformPixmap> data, const LRect<int> &rect)
{
    fromImage(data->toImage(rect), LImageConvertFlag::NoOpaqueDetection);
}

std::shared_ptr<LImage> LPlatformPixmap::toImage(const LRect<int> &rect) const
{
    std::shared_ptr<LImage> ret = toImage();
    if (rect.contains(LRect<int>(0, 0, m_width, m_height, false))){
        return ret;
    }else{
        return ret->copy(rect);
    }
}

std::shared_ptr<LPixmap> LPlatformPixmap::transformed(const LTransform2D &matrix, LTransMode mode) const
{
    std::shared_ptr<LImage> t = toImage();
    if(t != nullptr){
        return LPixmap::fromImage(t->transformed(matrix,
                                                 LTransMode((uint8_t)mode | (uint8_t)LTransMode::FastCanRetThis)));
    }else{
        return std::make_shared<LPixmap>();
    }
}

LK_NAMESPACE_END
