#ifndef LLABEL_H
#define LLABEL_H

#include "lkwidgets/widget/lframe.h"
#include "lkgui/image/ltexture.h"

LK_NAMESPACE_BEGIN
class LKWIDGETSDEF LLabelData
{
public:
    LLabelData();

    void setIcon(std::shared_ptr<LImage> image, bool isVertical=false, bool fixIndent = true);
    void setIcon(std::shared_ptr<LPixmap> pixmap, bool isVertical=false, bool fixIndent = true);
    void setIcon(std::shared_ptr<LTexture> texture, bool isVertical=false, bool fixIndent = true);

    inline const std::shared_ptr<LTexture>& icon() const{
        return m_icon;
    }

    void setText(const std::wstring &s);
    inline const std::wstring &text() const{
        return m_text;
    }

    void setIsTextLabel(bool v);
    void setIndent(uint16_t v,
                   LFlags<LOrientation> dir = LOrientation::Horizontal|LOrientation::Vertical);

    void setTextAlign(LFlags<LAlignment> a);

    union{
        uint8_t m_labelFlags = 0;
        struct{
            mutable uint8_t m_validHints : 1;
            uint8_t m_scaledcontents : 1;
            uint8_t m_textLayoutDirty : 1;
            uint8_t m_textDirty : 1;
            uint8_t m_isTextLabel : 1;
            uint8_t m_imageVertical : 1;
            uint8_t m_hasMark : 1; //菜单按钮需要提示三角标记, 或选择框需要显示勾选标记
            uint8_t m_isMarkLeft : 1; //标记在左
        };
    };


    uint16_t m_align = (LAlignment::Left | LAlignment::VCenter | LAlignment::ExpandTabs);
    uint16_t m_indentHor = 0; //文字对齐位置水平方向，当其值比水平放置的图标大时显示图标
    uint16_t m_indentVer = 0; //文字对齐位置竖直方向，当其值比坚直放置的图标大时显示图标
    std::wstring m_text;
protected:
    virtual void updateLabel() = 0;
    void updateTextureIndent(bool isVertical, bool fixIndent);
    LSize<int> sizeForWidth(int w, const LWidget& widget) const;
    LSize<int> minimumSizeHint(const LWidget& widget) const;

    std::shared_ptr<LTexture> m_icon;
    mutable LSize<int> m_sizeHint;
    mutable LSize<int> m_minSizeHint;
};

class LKWIDGETSDEF LLabel : public LWidget, public LLabelData,  protected LFrameData
{
public:
    _LIVarTagEx(LLabel, LWidget)

    LLabel(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);
    LLabel(const std::wstring &text,
           std::shared_ptr<LWidget> parent = nullptr,
           LFlags<LWindowType> f = 0);

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSizeHint() const override;
    virtual int heightForWidth(int w) const override;
    virtual void setSizePolicy(LSizePolicy sp) override;
protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    virtual void updateLabel() override;
    virtual void initSpecific() override;

};

LK_NAMESPACE_END
#endif // LLABEL_H
