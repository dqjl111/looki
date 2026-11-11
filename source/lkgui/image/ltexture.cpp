#include "ltexture.h"
#include "limagereader.h"
#include "lkcore/ldebug.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/paint/lpainter.h"

LK_NAMESPACE_BEGIN

static double lkEffectiveDevicePixelRatio(std::shared_ptr<LWindow> window = nullptr)
{
    if (!LGuiApplication::testAttribute(LAppAttribute::UseHighDpiPixmaps))
        return double(1.0);

    if (window){
        return window->devicePixelRatio();
    }

    return LGuiApplication::devicePixelRatio(); // Don't know which window to target.
}

std::unordered_map<std::string, std::shared_ptr<LTexturePixmaps> > LTexture::m_cachedPixmaps;
std::unordered_map<std::string, std::shared_ptr<LTexturePixmaps> > LTexture::m_cachedSizedPixmaps;

LTexture::LTexture()
{

}

LTexture::LTexture(std::shared_ptr<LPixmap> pix, uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    setPixmap(pix, partId, rowNum, colNum);
}

LTexture::LTexture(std::shared_ptr<LImage> img, uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    setImage(img, partId, rowNum, colNum);
}

LTexture::LTexture(const std::string &path, uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    setImage(path, partId, rowNum, colNum);
}

LTexture::LTexture(std::shared_ptr<LTexturePixmaps> pixmaps, uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    setTexturePixmaps(pixmaps, partId, rowNum, colNum);
}

std::shared_ptr<LPixmap> LTexture::getNormal(bool checkRect) const
{
    if(m_pixmaps != nullptr && m_pixmaps->m_normal != nullptr){
        if(checkRect){
            if(m_pixmaps->m_normal->size() == size()){
                return m_pixmaps->m_normal;
            }else{
                return m_pixmaps->m_normal->copy(m_rect);
            }
        }else{
            return m_pixmaps->m_normal;
        }
    }
    return nullptr;
}

void LTexture::setRect(const LRect<int> &in)
{
    std::shared_ptr<LPixmap> pix = getNormal();
    if(pix != nullptr){
        m_rect = pix->rect().intersected(in);
        m_partId = uint16_t(-1);
    }else{
        m_rect.reset();
    }
}

void LTexture::setPixmapSize(const LSize<int> &s)
{
    if(m_pixmaps != nullptr){
        if(m_pixmaps->m_normal != nullptr){
            if(m_pixmaps->m_normal->width() == s.m_x
                && m_pixmaps->m_normal->height() == s.m_y){
                return;
            }
        }

        if(m_pixmaps->m_gradient != nullptr || m_pixmaps->m_normal != nullptr){
            m_pixmaps = getPixmaps(m_pixmaps->m_path, s);
            initRect();
        }
    }
}

void LTexture::setPixmap(std::shared_ptr<LPixmap> pix, uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    if(m_pixmaps != nullptr){
        if(m_pixmaps->m_normal != pix){
            m_pixmaps->m_normal = pix;
            m_pixmaps->m_disabled = nullptr;
            m_pixmaps->m_select = nullptr;
        }
    }else{
        m_pixmaps = std::make_shared<LTexturePixmaps>();
        m_pixmaps->m_normal = pix;
        m_pixmaps->m_disabled = nullptr;
        m_pixmaps->m_select = nullptr;
    }
    m_partId = partId;
    m_rowNum = rowNum==0 ? 1 : rowNum;
    m_colNum = colNum==0 ? 1: colNum;

    initRect();
}

void LTexture::setImage(std::shared_ptr<LImage> image, uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    if(image != nullptr){
        std::shared_ptr<LPixmap> pix = LPixmap::fromImage(image);
        setPixmap(pix, partId, rowNum, colNum);
    }else{
        if(m_pixmaps != nullptr){
            m_pixmaps = nullptr;
        }
        m_rect.reset();
    }
}

void LTexture::setImage(const std::string &path, uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    std::shared_ptr<LTexturePixmaps> pixmaps = getPixmaps(path);
    setTexturePixmaps(pixmaps, partId, rowNum, colNum);
}

void LTexture::setTexturePixmaps(std::shared_ptr<LTexturePixmaps> pixmaps,
                                 uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    m_pixmaps = pixmaps;
    m_partId = partId;
    m_rowNum = rowNum==0 ? 1 : rowNum;
    m_colNum = colNum==0 ? 1: colNum;

    initRect();
}

void LTexture::switchPart(uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    m_partId = partId;

    if(rowNum != 0){
        m_rowNum = rowNum;
    }

    if(colNum != 0){
        m_colNum = colNum;
    }

    initRect();
}

std::shared_ptr<LTexture> LTexture::switchFromThis(uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    if(rowNum == 0){
        rowNum = m_rowNum;
    }

    if(colNum != 0){
        colNum = m_colNum;
    }

    return std::make_shared<LTexture>(m_pixmaps, partId, rowNum, colNum);
}

std::shared_ptr<LTexture> LTexture::switchSizeFromThis(const LSize<int> &s)
{
    if(!isEmpty()){
        if(s.width() != m_rect.m_width || s.height() != m_rect.m_height){
            std::shared_ptr<LPixmap> org = m_pixmaps->m_normal;
            LSize<int> pixSize
                = LSize<double>((double)org->width() * s.width()/(double)m_rect.m_width,
                                (double)org->height() * s.height()/(double)m_rect.m_height).toSize();

            return getTexture(m_pixmaps->m_path, pixSize, m_partId, m_rowNum, m_colNum);
        }
    }
    return nullptr;
}

void LTexture::toNext()
{
    if((m_partId+1) < m_rowNum*m_colNum){
        m_partId++;
        initRect();
    }
}

void LTexture::toPrevious()
{
    if(m_partId >0){
        m_partId--;
        initRect();
    }
}

std::shared_ptr<LPixmap> LTexture::toPixmap()
{
    if(isEmpty()){
        return std::make_shared<LPixmap>();
    }
    return m_pixmaps->m_normal->copy(m_rect);
}

bool LTexture::isPartValid()
{
    if(isEmpty()){
        return false;
    }

    if(m_partId == uint16_t(-1)){
        return false;
    }
    return m_partId>=0 && m_partId <m_rowNum*m_colNum;
}

bool LTexture::isSplitWithPart()
{
    return m_partId != uint16_t(-1);
}

bool LTexture::registPixmaps(const std::string &path, std::shared_ptr<LTexturePixmaps> pixmaps)
{
    if(pixmaps != nullptr){
        pixmaps->m_path = path;
        m_cachedPixmaps[path] = pixmaps;
        return true;
    }
    return false;
}

bool LTexture::registPixmaps(const std::string &path, const LSize<int> &s, std::shared_ptr<LTexturePixmaps> pixmaps)
{
    if(pixmaps != nullptr){
        pixmaps->m_path = path;
        m_cachedPixmaps.find(path);

        std::unordered_map<std::string, std::shared_ptr<LTexturePixmaps> >::const_iterator
            iter = m_cachedPixmaps.find(path);
        if( iter == m_cachedPixmaps.end()){
            m_cachedPixmaps[path] = pixmaps;
        }

        std::string key = sizedKey(path, s);
        m_cachedSizedPixmaps[key] = pixmaps;
        return true;
    }
    return false;
}

void LTexture::removePixmaps(const std::string &path)
{
    m_cachedPixmaps.erase(path);
}

void LTexture::removePixmaps(const std::string &path, const LSize<int> &s)
{
    m_cachedPixmaps.erase(sizedKey(path, s));
}


std::shared_ptr<LTexture> LTexture::getTexture(const std::string &path, uint16_t partId, uint16_t rowNum, uint16_t colNum)
{
    std::shared_ptr<LTexturePixmaps> pixmaps = getPixmaps(path);
    return std::make_shared<LTexture>(pixmaps, partId, rowNum, colNum);
}

std::shared_ptr<LTexture> LTexture::getTexture(const std::string &path,
                                               const LSize<int> &s,
                                               uint16_t partId,
                                               uint16_t rowNum,
                                               uint16_t colNum)
{
    std::shared_ptr<LTexturePixmaps> pixmaps = getPixmaps(path, s);
    return std::make_shared<LTexture>(pixmaps, partId, rowNum, colNum);
}

std::shared_ptr<LTexturePixmaps> LTexture::getPixmaps(const std::string &path)
{
    std::unordered_map<std::string, std::shared_ptr<LTexturePixmaps> >::const_iterator
        iter = m_cachedPixmaps.find(path);
    if( iter != m_cachedPixmaps.end()){
        return iter->second;
    }else{
        std::shared_ptr<LTexturePixmaps> pixmaps;
        std::shared_ptr<LGradient> g = parseGradient(path);
        if(g!= nullptr){
            iter = m_cachedPixmaps.find(g->toCss());
            if( iter != m_cachedPixmaps.end()){
                pixmaps = iter->second;
                m_cachedPixmaps[path] = pixmaps;
                return pixmaps;
            }else{
                pixmaps = std::make_shared<LTexturePixmaps>();
                pixmaps->m_gradient = g;
                pixmaps->m_path = g->toCss();
                m_cachedPixmaps[g->toCss()] = pixmaps;
            }
        }else{
            pixmaps = std::make_shared<LTexturePixmaps>();
        }

        if(pixmaps->m_gradient == nullptr){
            std::shared_ptr<LImage> img = LImageReader::loadFromFile(path);
            pixmaps->m_normal = LPixmap::fromImage(img);
            pixmaps->m_path = path;
        }
        m_cachedPixmaps[path] = pixmaps;
        return pixmaps;
    }
}

std::shared_ptr<LTexturePixmaps> LTexture::getPixmaps(const std::string &path, const LSize<int> &s)
{
    std::string key = sizedKey(path, s);
    std::unordered_map<std::string, std::shared_ptr<LTexturePixmaps> >::const_iterator
        iter = m_cachedSizedPixmaps.find(key);
    if( iter != m_cachedSizedPixmaps.end()){
        return iter->second;
    }else{
        std::shared_ptr<LTexturePixmaps> m = getPixmaps(path);
        if(m!= nullptr){
            if(m->m_normal!= nullptr){
                if(m->m_normal->width() == s.m_x && m->m_normal->height() == s.m_y){
                    m_cachedSizedPixmaps[key] = m;
                    return m;
                }
            }

            if(m->m_gradient != nullptr){
                std::shared_ptr<LImage> i = m->m_gradient->toImage(s);
                if(i != nullptr){
                    std::shared_ptr<LTexturePixmaps> pixmaps = std::make_shared<LTexturePixmaps>();
                    pixmaps->m_normal = LPixmap::fromImage(i);
                    pixmaps->m_path = path;
                    m_cachedSizedPixmaps[key] = pixmaps;
                    return pixmaps;
                }
            }

            if(m->m_normal!= nullptr){
                std::shared_ptr<LTexturePixmaps> pixmaps = std::make_shared<LTexturePixmaps>();
                pixmaps->m_normal = m->m_normal->scaled(s);
                pixmaps->m_path = path;
                m_cachedSizedPixmaps[key] = pixmaps;
                return pixmaps;
            }
        }
        return nullptr;
    }
}

std::shared_ptr<LPixmap> LTexture::getPixmap(Type type, bool checkRect) const
{
    switch(type){
    case Disable:{
        return getDisabled(checkRect);
    }
    case Select:{
        return getSelect(checkRect);
    }
    default:{
        return getNormal(checkRect);
    }
    }
}

std::shared_ptr<LPixmap> LTexture::getPixmap(const LSize<int> &s, Type mode, bool checkRect) const
{
    if(s.width() == m_rect.m_width && s.height() == m_rect.height()){
        return getPixmap(mode, checkRect);
    }else{
        if(!isEmpty()){
            std::shared_ptr<LPixmap> org = m_pixmaps->m_normal;
            LSize<int> pixSize
                = LSize<double>((double)org->width() * s.width()/(double)m_rect.m_width,
                                (double)org->height() * s.height()/(double)m_rect.m_height).toSize();

            std::shared_ptr<LTexture> t = getTexture(m_pixmaps->m_path, pixSize, m_partId, m_rowNum, m_colNum);
            if(!t->isEmpty()){
                return t->getPixmap(mode, checkRect);
            }
        }
    }
    return nullptr;
}

std::shared_ptr<LPixmap> LTexture::getPixmap(std::shared_ptr<LWindow> window,
                                             const LSize<int> &s,
                                             Type mode,
                                             bool checkRect) const
{
    if (isEmpty()){
        return nullptr;
    }

    double devicePixelRatio = lkEffectiveDevicePixelRatio(window);
    return getPixmap(devicePixelRatio, s, mode, checkRect);
}

std::shared_ptr<LPixmap> LTexture::getPixmap(std::shared_ptr<LPaintDevice> dev,
                                             const LSize<int> &s,
                                             Type mode,
                                             bool checkRect) const
{
    if (isEmpty()){
        return nullptr;
    }

    double devicePixelRatio = (dev != nullptr) ? dev->devicePixelRatio() : LGuiApplication::devicePixelRatio();
    return getPixmap(devicePixelRatio, s, mode, checkRect);
}

std::shared_ptr<LPixmap> LTexture::getPixmap(double devicePixelRatio,
                                             const LSize<int> &s,
                                             Type mode,
                                             bool checkRect) const
{
    if (isEmpty()){
        return nullptr;
    }

    // Handle the simple normal-dpi case:
    if (!(devicePixelRatio > 1.0)) {
        std::shared_ptr<LPixmap> p = getPixmap(s, mode, checkRect);
        if(p != nullptr){
            p->setDevicePixelRatio(1.0);
        }
        return p;
    }else{
        std::shared_ptr<LPixmap> p = getPixmap(s*devicePixelRatio, mode, checkRect);
        if(p != nullptr){
            p->setDevicePixelRatio(devicePixelRatio);
        }
        return p;
    }
}

std::shared_ptr<LPixmap> LTexture::getDisabled(bool checkRect) const
{
    std::shared_ptr<LPixmap> t;

    if(m_pixmaps != nullptr){
        if(m_pixmaps->m_disabled != nullptr){
            t = m_pixmaps->m_disabled;
        }else{
            if(m_pixmaps->m_normal != nullptr){
                std::shared_ptr<LImage> im = m_pixmaps->m_normal->toImage();
                im = im->convertToFormat(LImage::Format::ARGB32);

                // Create a colortable based on the background (black -> bg -> white)
                LColor bg = LGuiApplication::palette()->color(LPalette::Disabled, LPalette::Window);
                int red = bg.m_red;
                int green = bg.m_green;
                int blue = bg.m_blue;
                uint8_t reds[256], greens[256], blues[256];
                for (int i=0; i<128; ++i) {
                    reds[i]   = uint8_t((red   * (i<<1)) >> 8);
                    greens[i] = uint8_t((green * (i<<1)) >> 8);
                    blues[i]  = uint8_t((blue  * (i<<1)) >> 8);
                }
                for (int i=0; i<128; ++i) {
                    reds[i+128]   = uint8_t(std::min<int>(red   + (i << 1), 255));
                    greens[i+128] = uint8_t(std::min<int>(green + (i << 1), 255));
                    blues[i+128]  = uint8_t(std::min<int>(blue  + (i << 1), 255));
                }

                // 30% red, 59% green, 11% blue
                int intensity =(77 * red + 150 * green + 28 * blue) / 255;
                const int factor = 191;

                // High intensity colors needs dark shifting in the color table, while
                // low intensity colors needs light shifting. This is to increase the
                // perceived contrast.
                if ((red - factor > green && red - factor > blue)
                    || (green - factor > red && green - factor > blue)
                    || (blue - factor > red && blue - factor > green)){
                    intensity = std::min<int>(255, intensity + 91);
                }
                else if (intensity <= 128){
                    intensity -= 51;
                }

                for (int y=0; y<im->height(); ++y) {
                    uint32_t * scanLine = (uint32_t *) im->scanLine(y);
                    for (int x=0; x<im->width(); ++x) {
                        uint32_t pixel = *scanLine;
                        // Calculate color table index, taking intensity adjustment
                        // and a magic offset into account.
                        uint8_t ci = uint8_t(LColor::toGray(pixel)/3 + (130 - intensity / 3));
                        *scanLine = LColor(reds[ci], greens[ci], blues[ci], LColor::uintToAlpha(pixel)).toUInt();
                        ++scanLine;
                    }
                }

                const_cast<LTexture*>(this)->m_pixmaps->m_disabled =  LPixmap::fromImage(im);
                t = m_pixmaps->m_disabled;
            }
        }
    }

    if(t != nullptr){
        if(checkRect){
            if(t->size() == size()){
                return t;
            }else{
                return t->copy(m_rect);
            }
        }else{
            return t;
        }
    }
    return nullptr;
}

std::shared_ptr<LPixmap> LTexture::getSelect(bool checkRect) const
{
    std::shared_ptr<LPixmap> t;
    if(m_pixmaps != nullptr){
        if(m_pixmaps->m_select != nullptr){
            t =  m_pixmaps->m_select;
        }else{
            if(m_pixmaps->m_normal != nullptr){
                std::shared_ptr<LImage> im = m_pixmaps->m_normal->toImage();
                im = im->convertToFormat(LImage::Format::ARGB32Premultiplied);

                LColor color = LGuiApplication::palette()->color(LPalette::Normal, LPalette::Highlight);
                color.m_alpha =  77;
                LPainter painter(im);
                painter.setCompositionMode(LCompositionMode::SourceAtop);
                painter.fillRect(im->rectF(), color);
                painter.end();

                const_cast<LTexture*>(this)->m_pixmaps->m_select =  LPixmap::fromImage(im);
                t = m_pixmaps->m_select;
            }
        }
    }
    if(t != nullptr){
        if(checkRect){
            if(t->size() == size()){
                return t;
            }else{
                return t->copy(m_rect);
            }
        }else{
            return t;
        }
    }
    return nullptr;
}

double LTexture::devicePixelRatio() const
{
    if(isEmpty()){
        return 1.0;
    }
    return m_pixmaps->m_normal->devicePixelRatio();
}

void LTexture::setDevicePixelRatio(double scaleFactor)
{
    if(isEmpty()){
        return;
    }
    m_pixmaps->m_normal->setDevicePixelRatio(scaleFactor);
    if(m_pixmaps->m_disabled != nullptr){
        m_pixmaps->m_disabled->setDevicePixelRatio(scaleFactor);
    }

    if(m_pixmaps->m_select != nullptr){
        m_pixmaps->m_select->setDevicePixelRatio(scaleFactor);
    }
}

LSize<int> LTexture::actualSize(const LSize<int> &s) const
{
    LSize<int> actualSize = LSize<int>(m_rect.width(), m_rect.height());
    if (actualSize.isNull()){
        return actualSize;
    }

    //实际更长或更高，进行缩小
    if (!actualSize.isNull() && (actualSize.width() > s.width() || actualSize.height() > s.height())){
        actualSize.scale(s, LAspectRatioMode::KeepAspectRatio);
    }
    return actualSize;

}

LSize<int> LTexture::actualSize(std::shared_ptr<LWindow> window, const LSize<int> &s) const
{
    double devicePixelRatio = lkEffectiveDevicePixelRatio(window);

    // Handle the simple normal-dpi case:
    if (!(devicePixelRatio > 1.0)){
        return actualSize(s);
    }
    return s;
}

bool LTexture::isWholePixmap(Type type)
{
    if(isEmpty()){
        return true;
    }

    switch(type){
    case Disable:{
        if(m_pixmaps->m_disabled != nullptr){
            if(m_pixmaps->m_disabled->size() == size()){
                return true;
            }
            return false;
        }
        break;
    }
    case Select:{
        if(m_pixmaps->m_select != nullptr){
            if(m_pixmaps->m_select->size() == size()){
                return true;
            }
            return false;
        }
        break;
    }
    default:{
        break;
    }
    }

    if(m_pixmaps->m_normal->size() == size()){
        return true;
    }
    return false;
}

void LTexture::paint(LPainter *painter, const LRect<int> &rect, LFlags<LAlignment> alignment, Type mode) const
{
    if (painter == nullptr){
        return;
    }

    const LSize<int> size = actualSize(rect.size());
    alignment = LGuiApplication::visualAlignment(painter->layoutDirection(), alignment);
    int x = rect.m_x;
    int y = rect.m_y;
    int w = size.width();
    int h = size.height();
    if ((alignment & LAlignment::VCenter) == LAlignment::VCenter){
        y += rect.size().height()/2 - h/2;
    }
    else if ((alignment & LAlignment::Bottom) == LAlignment::Bottom){
        y += rect.size().height() - h;
    }

    if ((alignment & LAlignment::Right) == LAlignment::Right){
        x += rect.size().width() - w;
    }
    else if ((alignment & LAlignment::HCenter) == LAlignment::HCenter){
        x += rect.size().width()/2 - w/2;
    }
    LRect<int> alignedRect(x, y, w, h);

    std::shared_ptr<LPixmap> px = getPixmap(painter->device(), alignedRect.size(), mode);
    painter->drawPixmap(alignedRect.toRectF(), px, LRect<double>());
}

void LTexture::initRect()
{
    std::shared_ptr<LPixmap> pix = getNormal();
    if(pix != nullptr && !pix->isNull()){
        uint16_t partNum = m_rowNum*m_colNum;

        if(m_partId < partNum){
            if(partNum == 1){
                m_rect = pix->rect();
            }else{
                m_rect.m_width = pix->width() / m_colNum;
                m_rect.m_height = pix->height()/m_rowNum;
                m_rect.m_x = m_partId%m_colNum*m_rect.m_width;
                m_rect.m_y = m_partId/m_rowNum*m_rect.m_height;
            }
        }else{
            m_rect.reset();
        }
    }else{
        m_rect.reset();
    }
}

std::shared_ptr<LGradient> LTexture::parseGradient(const std::string &s)
{
    if(s.starts_with(":")){ //程序资源文件
        return nullptr;
    }


    return nullptr;
}

LK_NAMESPACE_END
