#include "lcssbuilder.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/text/lfontmetrics.h"

LK_NAMESPACE_BEGIN
using namespace lcss;

LCssBuilder::LCssBuilder(std::shared_ptr<lcss::StyleComputed> styleComputed) {
    if(LFont::defaultDpi() != m_inToPx){
        setDpi(LFont::defaultDpi());
    }

    setStyleComputed(styleComputed);
}

void LCssBuilder::setStyleComputed(std::shared_ptr<lcss::StyleComputed> styleComputed)
{
    m_styleComputed = styleComputed;
    if(m_styleComputed != nullptr){
        if(m_styleComputed->fontSize.populated){
            int t = std::lround(lengthToPx(m_styleComputed->fontSize.value));
            if(t != 0){
                m_emToPx = t;
            }
        }
    }
}

void LCssBuilder::setDpi(int dpi)
{
    m_inToPx = dpi;
    m_cmToPx = m_inToPx/2.54;
    m_mmToPx = m_cmToPx/10.0;
}

double LCssBuilder::widthToPx(const lcss::StyleLength &len) const
{
    if(len.unit == lcss::StyleLengthUnit::Percent){
        lcss::StyleLength tLen = len;
        tLen.unit = lcss::StyleLengthUnit::vw;
        return lengthToPx(tLen);
    }else{
        return lengthToPx(len);
    }
}

double LCssBuilder::heightToPx(const lcss::StyleLength &len) const
{
    if(len.unit == lcss::StyleLengthUnit::Percent){
        lcss::StyleLength tLen = len;
        tLen.unit = lcss::StyleLengthUnit::vh;
        return lengthToPx(tLen);
    }else{
        return lengthToPx(len);
    }
}

double LCssBuilder::lengthToPx(const lcss::StyleLength &len) const
{
    switch(len.unit){
    case lcss::StyleLengthUnit::in:{
        return len.number*m_inToPx;
    }
    case lcss::StyleLengthUnit::cm:{
        return len.number*m_cmToPx;
    }
    case lcss::StyleLengthUnit::mm:{
        return len.number*m_mmToPx;
    }
    case lcss::StyleLengthUnit::em:{
        return len.number*m_emToPx;
    }
    case lcss::StyleLengthUnit::vw:{
        return len.number*m_viewSize.width()/100.0;
    }
    case lcss::StyleLengthUnit::vh:{
        return len.number*m_viewSize.height()/100.0;
    }
    default:{
        return len.number;
    }
    }
}

std::array<double, 4> LCssBuilder::buildBorderImageSlices() const
{
    std::array<double, 4>  slices = {0,0,0,0};
    for(uint8_t i=0; i<4; ++i){
        const StyleValueComputed<StyleLength>& computed = m_styleComputed->borderImageSlice(LDirection(i));
        if(computed.populated){
            slices[i] = lengthToPx(computed.value);
        }
    }
    return  slices;
}

std::array<double, 4> LCssBuilder::buildBoxPaddings() const
{
    std::array<double, 4>  paddings = {0,0,0,0};
    if(m_styleComputed != nullptr){
        for(uint8_t i=0; i<4; ++i){
            const StyleValueComputed<StyleLength>& computed = m_styleComputed->padding(LDirection(i));
            if(computed.populated){
                paddings[i] = lengthToPx(computed.value);
            }
        }
    }
    return  paddings;
}

std::array<double, 4> LCssBuilder::buildBoxMargins() const
{
    std::array<double, 4>  margins = {0,0,0,0};
    for(uint8_t i=0; i<4; ++i){
        const StyleValueComputed<StyleLength>& computed = m_styleComputed->margin(LDirection(i));
        if(computed.populated){
            margins[i] = lengthToPx(computed.value);
        }
    }
    return  margins;
}


std::array<LColor, 4> LCssBuilder::buildBorderColors() const
{
    std::array<LColor, 4>  colors;
    for(uint8_t i=0; i<4; ++i){
        const StyleValueComputed<LColor>& computed = m_styleComputed->borderColor(LDirection(i));
        if(computed.populated){
            colors[i] = computed.value;
        }else{
            colors[i] = LColorID::Transparent;
        }
    }
    return  colors;
}

std::array<LSize<double>, 4> LCssBuilder::buildBorderRadiuses() const
{
    std::array<LSize<double>, 4> radii;
    if(m_styleComputed != nullptr){
        for(uint8_t i=0; i<4; ++i){
            const StyleValueComputed<StyleLength>& computed = m_styleComputed->borderRadius(LCorner(i));
            if(computed.populated){
                radii[i].m_x = lengthToPx(computed.value);
                radii[i].m_y = lengthToPx(computed.value);
            }
        }
    }
    return radii;
}

std::array<double, 4> LCssBuilder::buildBorderWidths() const
{
    std::array<double, 4>  widths = {0,0,0,0};
    for(uint8_t i=0; i<4; ++i){
        const StyleValueComputed<StyleLength>& computed = m_styleComputed->borderWidth(LDirection(i));
        if(computed.populated){
            widths[i] = lengthToPx(computed.value);
        }
    }
    return  widths;
}

std::array<BorderStyle, 4> LCssBuilder::buildBorderStyles() const
{
    std::array<lcss::BorderStyle, 4>  styles;
    for(uint8_t i=0; i<4; ++i){
        const StyleValueComputed<BorderStyle>& computed = m_styleComputed->borderStyle(LDirection(i));
        if(computed.populated){
            styles[i] = computed.value;
        }else{
            styles[i] = lcss::BorderStyle::None;
        }
    }
    return styles;
}

std::shared_ptr<LFont> LCssBuilder::buildFont() const
{
    if(m_styleComputed->fontFamily.populated && !m_styleComputed->fontFamily.value.empty()){
        return  std::make_shared<LFont>(m_styleComputed->fontFamily.value, m_emToPx);
    }else{
        std::shared_ptr<LFont> f = LGuiApplication::font();
        if(m_styleComputed->fontSize.populated){
            f = std::make_shared<LFont>(f);
            f->setPointSize(m_emToPx);
            return f;
        }
        return nullptr;
    }
}

std::shared_ptr<LCssBorderData> LCssBuilder::buildBorderData() const
{
    if(hasBorderData()){
        std::shared_ptr<LCssBorderData> data = std::make_shared<LCssBorderData>();
        data->radiis = buildBorderRadiuses();
        data->styles = buildBorderStyles();
        data->colors = buildBorderColors();
        data->borders = buildBorderWidths();

        return data;
    }
    return nullptr;
}

std::shared_ptr<LCssBorderImageData> LCssBuilder::buildBorderImage() const
{
    if(hasBorderImage()){
        std::shared_ptr<LCssBorderImageData> data = std::make_shared<LCssBorderImageData>();
        data->texture = LTexture::getTexture(m_styleComputed->borderImage.value);
        data->slices = buildBorderImageSlices();
        if(m_styleComputed->borderImageHorizRepeat.populated){
            data->horizRepeat = m_styleComputed->borderImageHorizRepeat.value;
        }

        if(m_styleComputed->borderImageVertRepeat.populated){
            data->vertRepeat = m_styleComputed->borderImageVertRepeat.value;
        }
        return data;
    }
    return nullptr;
}

std::shared_ptr<LCssBoxData> LCssBuilder::buildBoxData() const
{
    if(hasBoxData()){
        std::shared_ptr<LCssBoxData> data = std::make_shared<LCssBoxData>();
        data->margins = buildBoxMargins();
        data->paddings = buildBoxPaddings();
        return data;
    }
    return nullptr;
}

std::shared_ptr<LCssForegroundData> LCssBuilder::buildForegroundData() const
{
    if(hasForegroundData()){
        std::shared_ptr<LCssForegroundData> data = std::make_shared<LCssForegroundData>();
        data->font = buildFont();
        if(m_styleComputed->color.populated){
            data->color = m_styleComputed->color.value;
        }
        return data;
    }
    return nullptr;
}

std::shared_ptr<LCssBackgroundData> LCssBuilder::buildBackgroundData() const
{
    if(hasBackgroundData()){
        std::shared_ptr<LCssBackgroundData> data = std::make_shared<LCssBackgroundData>();
        if(m_styleComputed->backgroundColor.populated){
            data->color = m_styleComputed->backgroundColor.value;
        }

        if(m_styleComputed->backgroundRepeat.populated){
            data->repeat = m_styleComputed->backgroundRepeat.value;
        }

        if(m_styleComputed->backgroundOrigin.populated){
            data->origin = m_styleComputed->backgroundOrigin.value;
        }

        if(m_styleComputed->backgroundClip.populated){
            data->clip = m_styleComputed->backgroundClip.value;
        }

        if(m_styleComputed->backgroundPosition.populated){
            data->position = m_styleComputed->backgroundPosition.value;
        }

        if(m_styleComputed->backgroundAttachment.populated){
            data->attachment = m_styleComputed->backgroundAttachment.value;
        }

        if(m_styleComputed->backgroundImage.populated){
            data->texture = LTexture::getTexture(m_styleComputed->backgroundImage.value);
        }
        return data;
    }
    return nullptr;
}


bool LCssBuilder::hasBorderImage() const
{
    if(m_styleComputed != nullptr){
        return m_styleComputed->borderImage.populated;
    }
    return false;
}

bool LCssBuilder::hasBorderData() const
{
    if(m_styleComputed != nullptr){
        for(uint8_t i=0; i<4; ++i){
            {
                const StyleValueComputed<LColor>& computed = m_styleComputed->borderColor(LDirection(i));
                if(computed.populated){
                    return true;
                }
            }
            {
                const StyleValueComputed<StyleLength>& computed = m_styleComputed->borderRadius(LCorner(i));
                if(computed.populated){
                    return true;
                }
            }
            {
                const StyleValueComputed<StyleLength>& computed = m_styleComputed->borderWidth(LDirection(i));
                if(computed.populated){
                    return true;
                }
            }
            {
                const StyleValueComputed<BorderStyle>& computed = m_styleComputed->borderStyle(LDirection(i));
                if(computed.populated){
                    return true;
                }
            }
        }

        return m_styleComputed->backgroundImage.populated;
    }
    return false;
}

bool LCssBuilder::hasBoxData() const
{
    if(m_styleComputed != nullptr){
        for(uint8_t i=0; i<4; ++i){
            {
                const StyleValueComputed<StyleLength>& computed = m_styleComputed->padding(LDirection(i));
                if(computed.populated){
                    return true;
                }
            }
            {
                const StyleValueComputed<StyleLength>& computed = m_styleComputed->margin(LDirection(i));
                if(computed.populated){
                    return true;
                }
            }
        }
    }
    return false;
}

bool LCssBuilder::hasBackgroundData() const
{
    if(m_styleComputed != nullptr){
        if(m_styleComputed->backgroundColor.populated){
            return true;
        }

        if(m_styleComputed->backgroundImage.populated){
            return true;
        }
    }
    return false;
}

bool LCssBuilder::hasForegroundData() const
{
    if(m_styleComputed != nullptr){
        if(m_styleComputed->color.populated){
            return true;
        }

        if(m_styleComputed->fontFamily.populated){
            return true;
        }

        if(m_styleComputed->fontSize.populated){
            return true;
        }
    }
    return false;
}

LK_NAMESPACE_END


