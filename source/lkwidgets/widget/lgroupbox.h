#ifndef LGROUPBOX_H
#define LGROUPBOX_H

#include "lkwidgets/widget/lframe.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LGroupBox : public LFrame
{
public:
    _LIVarTagEx(LGroupBox, LWidget)

    LGroupBox(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);
    LGroupBox(const std::wstring& text, std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);

    inline bool isCheckable() const{
        return m_checkable;
    }
    void setCheckable(bool checkable);
    inline bool isChecked() const{
        return m_checked;
    }
    void setChecked(bool checked);

    void setTitle(const std::wstring& text);
    const std::wstring& title() const{
        return m_title;
    }

    virtual LRect<int> customRect() const override;
    virtual int heightForWidth(int) const override;
    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSizeHint() const override;

protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> ev) override;

    void setChildrenEnabled(bool b);
    void calculateFrame();
    virtual void initStyleOpt(LStyleOptGroupBox& opt) const;
    LSize<int> frameRequireSize() const;

    std::wstring m_title;
    LFlags<LAlignment> m_titleAlignment = LAlignment::Left|LAlignment::VCenter;
    union{
        uint8_t m_groupBoxFlags = 0;
        struct{
            uint8_t m_checkable :1;
            uint8_t m_checked :1;
        };
    };



};


LK_NAMESPACE_END

#endif // LGROUPBOX_H
