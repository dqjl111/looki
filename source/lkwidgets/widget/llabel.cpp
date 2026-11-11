#include "llabel.h"
#include "lkgui/text/lfontmetrics.h"
#include "lkwidgets/draw/lstyle.h"

LK_NAMESPACE_BEGIN
LLabelData::LLabelData()
{

}


void LLabelData::setText(const std::wstring &s)
{
    m_text = s;
    m_isTextLabel = true;
    m_textDirty = true;
    updateLabel();
}

void LLabelData::updateTextureIndent(bool isVertical, bool fixIndent)
{
    m_imageVertical = isVertical;
    if(m_icon != nullptr){
        if(fixIndent){
            if(m_imageVertical){
                m_indentVer = std::max<int>(m_indentVer, std::lround(m_icon->height()/m_icon->devicePixelRatio()));
            }else{
                m_indentHor = std::max<int>(m_indentHor, std::lround(m_icon->width()/m_icon->devicePixelRatio()));
            }
        }
    }
    updateLabel();
}

LSize<int> LLabelData::sizeForWidth(int w, const LWidget& widget) const
{
    int minW = widget.minimumWidth();
    if(minW > 0){
        w = std::max(w, minW);
    }
    LMargins<int> margin = widget.contentsMargins();
    LSize<int> contentsMargin(margin.m_left + margin.m_right, margin.m_top + margin.m_bottom);
    LSize<int> contentsSize;

    if (m_isTextLabel) {
        std::shared_ptr<LStyle> pStyle = widget.style();
        int iconSpan = pStyle->get(&LStylePixelMetric::iconSpan, &widget.elementStyle());

        int hextra = 0;
        int vextra = 0;

        if ((m_align & (uint16_t)LAlignment::LeftRightMask)){
            hextra += m_indentHor;
        }
        if ((m_align & (uint16_t)LAlignment::TopBottomMask)){
            vextra += m_indentVer;
        }
        if(m_imageVertical){
            vextra += iconSpan;
        }else{
            hextra += iconSpan;
        }

        const std::wstring& text = m_text;
        if(!text.empty()){
            LFontMetrics fm = widget.fontMetrics();

            int flags = m_align & ~uint16_t(LAlignment::VCenter | LAlignment::HCenter);

            bool tryWidth = false;
            if (w < 0){
                if(m_align & (uint16_t)LAlignment::WordWrap){
                    w = std::min<int>(std::lround(fm.averageCharWidth() * 80), widget.maximumSize().width());
                    tryWidth = true;
                }else{
                    w = 2000;
                }
            }
            w -= (hextra + contentsMargin.width());
            LRect<int> textRect = fm.boundingRect(0, 0, w ,2000, flags, text);
            if (tryWidth && textRect.height() < 4*fm.lineSpacing() && textRect.width() > w/2){
                textRect = fm.boundingRect(0, 0, w/2, 2000, flags, text);
            }
            if (tryWidth && textRect.height() < 2*fm.lineSpacing() && textRect.width() > w/4){
                textRect = fm.boundingRect(0, 0, w/4, 2000, flags, text);
            }

            if(m_hasMark){
                int markSpan = pStyle->get(&LStylePixelMetric::itemMarkSpan, &widget.elementStyle());
                int markSize = pStyle->get(&LStylePixelMetric::smallIconSize, &widget.elementStyle());

                textRect.setWidth(textRect.width() + markSpan + markSize);
                textRect.setHeight(std::min<int>(textRect.height(), markSize));
            }

            contentsSize.setWidth(textRect.width() + hextra);
            contentsSize.setHeight(textRect.height() + vextra);
        }else{
            contentsSize.setWidth(hextra);
            contentsSize.setHeight(vextra);
        }

        if (m_icon != nullptr) {
            LSize<int> t = m_icon->size() / m_icon->devicePixelRatio();
            if(m_imageVertical){
                if(m_indentVer >= t.height()){
                    contentsSize.setWidth(std::max<int>(contentsSize.width(), t.width()));
                }
            }else{
                if(m_indentHor >= t.width()){
                    contentsSize.setHeight(std::max<int>(contentsSize.height(), t.height()));
                }
            }
        }
    }else{
        if (m_icon != nullptr) {
            contentsSize = (m_icon->size() / m_icon->devicePixelRatio());
        }

        if(m_imageVertical){
            if(m_hasMark){
                std::shared_ptr<LStyle> pStyle = widget.style();
                contentsSize.setHeight(contentsSize.height()
                                      + pStyle->get(&LStylePixelMetric::iconSpan, &widget.elementStyle())
                                      + pStyle->get(&LStylePixelMetric::smallIconSize, &widget.elementStyle()));
            }
        }else{
            if(m_hasMark){
                std::shared_ptr<LStyle> pStyle = widget.style();
                contentsSize.setWidth(contentsSize.width()
                                      + pStyle->get(&LStylePixelMetric::iconSpan, &widget.elementStyle())
                                      + pStyle->get(&LStylePixelMetric::smallIconSize, &widget.elementStyle()));
            }
        }
    }
    return (contentsSize + contentsMargin).expandedTo(widget.minimumSize());
}

LSize<int> LLabelData::minimumSizeHint(const LWidget &widget) const
{
    if (m_validHints) {
        return m_minSizeHint;
    }

    m_sizeHint = sizeForWidth(-1, widget); // wrap ? golden ratio : min doc size
    if (!m_isTextLabel) {
        m_minSizeHint = m_sizeHint;
        m_validHints = true;
        return m_sizeHint;
    } else {
        LSize<int> msh(-1, -1);
        msh.m_y = sizeForWidth(LWIDGETSIZE_MAX, widget).m_y; // height for one line
        msh.m_x = sizeForWidth(0, widget).m_x; // wrap ? size of biggest word : min doc size
        if (m_sizeHint.m_y < msh.m_y){
            msh.m_y = m_sizeHint.m_y;
        }
        m_minSizeHint = msh;
        m_validHints = true;
        return m_minSizeHint;
    }
}


void LLabelData::setIcon(std::shared_ptr<LImage> image, bool isVertical, bool fixIndent)
{
    if(image == nullptr){
        m_icon = nullptr;
        updateTextureIndent(isVertical, fixIndent);
    }else{
        std::shared_ptr<LPixmap> pix = LPixmap::fromImage(image);
        setIcon(pix, isVertical, fixIndent);
    }
}

void LLabelData::setIcon(std::shared_ptr<LPixmap> pixmap, bool isVertical, bool fixIndent)
{
    if(pixmap == nullptr){
        m_icon = nullptr;
    }else{
        if(m_icon != nullptr){
            if(m_icon->getNormal() != pixmap){
                m_icon->setPixmap(pixmap);
            }
        }else{
            m_icon = std::make_shared<LTexture>(pixmap);
        }
    }
    updateTextureIndent(isVertical, fixIndent);
}

void LLabelData::setIcon(std::shared_ptr<LTexture> texture, bool isVertical, bool fixIndent)
{
    m_icon = texture;
    updateTextureIndent(isVertical, fixIndent);
}

void LLabelData::setIsTextLabel(bool v)
{
    if(m_isTextLabel != v){
        m_isTextLabel = true;
        m_textDirty = true;
        updateLabel();
    }
}

void LLabelData::setIndent(uint16_t v, LFlags<LOrientation> dir)
{
    bool isUpdate = false;
    if(dir & LOrientation::Horizontal){
        if(m_indentHor != v){
            m_indentHor = v;
            isUpdate = true;
        }
    }

    if(dir & LOrientation::Vertical){
        if(m_indentVer != v){
            m_indentVer = v;
            isUpdate = true;
        }
    }
    if(isUpdate){
        updateLabel();
    }
}

void LLabelData::setTextAlign(LFlags<LAlignment> a)
{
    if(m_align != a){
        m_align = a;
        updateLabel();
    }
}


LLabel::LLabel(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f) : LWidget(parent, f)
{
    m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::Label);
    m_leftMargin = 1;
}

LLabel::LLabel(const std::wstring &text, std::shared_ptr<LWidget> parent, LFlags<LWindowType> f): LWidget(parent, f)
{
    m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::Label);
    m_leftMargin = 1;
    m_text = text;
    m_isTextLabel = true;
    m_textDirty = true;
}

LSize<int> LLabel::sizeHint() const
{
    if (!m_validHints){
        minimumSizeHint();
    }
    return m_sizeHint;
}

LSize<int> LLabel::minimumSizeHint() const
{
    return LLabelData::minimumSizeHint(*this);
}

int LLabel::heightForWidth(int w) const
{
    if (m_isTextLabel){
        return sizeForWidth(w, *this).height();
    }
    return LWidget::heightForWidth(w);
}

void LLabel::setSizePolicy(LSizePolicy sp)
{
    m_sizePolicy = sp;
    m_validHints = false;
}

void LLabel::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    LPainter painter(paintDevice());

    std::shared_ptr<LStyle> pStyle = style();
    std::shared_ptr<LWidget> f = getPtrT<LWidget>();
    pStyle->drawFrame(f, &painter, *this);
    pStyle->drawLabel(f, &painter, *this, LDrawMark::None);
}

void LLabel::updateLabel()
{
    m_textDirty = false;
    m_validHints = false;
    if (m_isTextLabel) {
        LSizePolicy policy = sizePolicy();
        const bool wrap = m_align & (uint16_t)LTextFlag::WordWrap;
        policy.setHasHeightForWidth(wrap);
        if (policy != sizePolicy()){
            setSizePolicy(policy);
        }
        m_textLayoutDirty = true;
    }
    updateGeometry();
    update(contentsRect());
}


void LLabel::initSpecific()
{
    if(m_textDirty){
        updateLabel();
    }
}
LK_NAMESPACE_END
