#include "lbrush.h"
LK_NAMESPACE_BEGIN


LBrush::LBrush(LBrush* b )
{
    if(b != nullptr){
        *this = *b;
    }
}

LBrush::LBrush(const LColor &color, Style bs)
{
    m_color = color;
    m_style = bs;
}

LBrush::LBrush(const std::shared_ptr<LImage> image)
{
    if(image != nullptr){
        m_color = LColorID::Black;
        m_style = LBrush::TexturePattern;
        m_image = image;
        m_hasPixmapTexture = false;
    }
}

LBrush::LBrush(const std::shared_ptr<LPixmap> pm)
{
    if(pm != nullptr){
        m_color = LColorID::Black;
        m_style = LBrush::TexturePattern;
        m_pixmap = pm;
        m_hasPixmapTexture = true;
    }
}

LBrush::LBrush(const std::shared_ptr<LGradient> g)
{
    if(g != nullptr){
        m_color = LColorID::Black;
        m_style = LBrush::GradientPattern;
        m_gradient = g;
    }
}

std::shared_ptr<LBrush> LBrush::backBrush()
{
    std::shared_ptr<LBrush> ret = std::make_shared<LBrush>(this);
    return ret;
}

LBrush &LBrush::operator=(const LBrush &b)
{
    m_style = b.m_style;
    m_color = b.m_color;
    m_transform = b.m_transform;

    m_pixmap = b.m_pixmap;
    m_image = b.m_image;
    m_hasPixmapTexture = b.m_hasPixmapTexture;

    return *this;
}

bool LBrush::operator==(const LBrush &b) const
{
    if(b.m_style != m_style){
        return false;
    }

    if(b.m_transform != m_transform){
        return false;
    }

    switch(b.m_style){
    case TexturePattern:{

        if(b.m_hasPixmapTexture == m_hasPixmapTexture){
            if(m_hasPixmapTexture){
                return b.m_pixmap == m_pixmap;
            }else{
                return b.m_image == m_image;
            }
        }else{
            return false;
        }
    }
    case GradientPattern:{
        if(b.m_gradient == m_gradient){
            return true;
        }

        if(b.m_gradient != nullptr ){
            if(m_gradient == nullptr){
                return false;
            }
        }else{
            if(m_gradient != nullptr){
                return false;
            }
        }
        return *b.m_gradient == *m_gradient;
    }
    default:
    {
        return b.m_color == m_color;
    }
    }
}

bool LBrush::isOpaque() const
{
    if (m_style == LBrush::SolidPattern){
        return m_color.m_alpha == 255;
    }
    return false;
}

std::shared_ptr<LPixmap> LBrush::texture() const
{
    if (m_pixmap != nullptr) {
        return m_pixmap;
    }else{
        if(m_image != nullptr){
            const_cast<LBrush*>(this)->m_pixmap = LPixmap::fromImage(m_image);
            return m_pixmap;
        }
    }
    return nullptr;
}

std::shared_ptr<LImage> LBrush::textureImage() const
{
    if (m_image != nullptr) {
        return m_image;
    }else{
        if(m_pixmap != nullptr){
            const_cast<LBrush*>(this)->m_image = m_pixmap->toImage();
            return m_image;
        }
    }
    return nullptr;
}

std::shared_ptr<LGradient> LBrush::gradient() const
{
    return m_gradient;
}

void LBrush::setPixmap(std::shared_ptr<LPixmap> pm)
{
    m_pixmap =  pm;
    m_hasPixmapTexture = (m_pixmap != nullptr);
    m_image = nullptr;
}

void LBrush::setImage(std::shared_ptr<LImage> img)
{
    m_image =  img;
    m_hasPixmapTexture = false;
    m_pixmap = nullptr;
}

void LBrush::setGradient(std::shared_ptr<LGradient> g)
{
    m_gradient = g;
}



LK_NAMESPACE_END
