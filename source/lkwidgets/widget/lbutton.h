#ifndef LBUTTON_H
#define LBUTTON_H

#include "labstractbutton.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LButton : public LAbstractButton, public LLabelData,  protected LFrameData
{
public:
    _LIVarTagEx(LButton, LWidget)

    LButton(std::shared_ptr<LWidget> parent = nullptr,
            LFlags<LWindowType> f=0);

    LButton(const std::wstring &text,
            std::shared_ptr<LWidget> parent = nullptr,
            LFlags<LWindowType> f = 0);

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSizeHint() const override;
    virtual int heightForWidth(int w) const override;
    virtual void setSizePolicy(LSizePolicy sp) override;

    void setDefault(bool);
    bool isDefault() const;

    bool autoDefault() const;
    void setAutoDefault(bool);
protected:
    LButton(LSizePolicy::ControlType type,
            std::shared_ptr<LWidget> parent = nullptr,
            LFlags<LWindowType> f=0);

    enum AutoDefaultValue { Off = 0, On = 1, Auto = 2 };

    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    void initButton();

    virtual void updateLabel() override;
    virtual void initSpecific() override;
};


class LKWIDGETSDEF LMenuButton : public LButton
{
public:
    _LIVarTagEx(LMenuButton, LWidget)

    LMenuButton(std::shared_ptr<LWidget> parent = nullptr,
            LFlags<LWindowType> f=0);

    LMenuButton(const std::wstring &text,
                std::shared_ptr<LWidget> parent = nullptr,
                LFlags<LWindowType> f = 0);

    void setMenu(std::shared_ptr<LMenu> menu);
    std::shared_ptr<LMenu> menu() const;

    void setShowArrow(bool isShowArrow);
    inline bool isShowArrow() const{
        return m_hasMark;
    }
    virtual uint16_t uiElementState(LUiElementType type = LUiElementType::None) const override;
protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    virtual void setDown(bool) override;

    std::shared_ptr<LMenu> m_menu;
};

class LKWIDGETSDEF LCheckBox : public LButton
{
public:
    _LIVarTagEx(LCheckBox, LWidget)

    LCheckBox(std::shared_ptr<LWidget> parent = nullptr,
              LFlags<LWindowType> f=0);

    LCheckBox(const std::wstring &text,
              std::shared_ptr<LWidget> parent = nullptr,
              LFlags<LWindowType> f = 0);

protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
};


class LKWIDGETSDEF LRadioButton : public LButton
{
public:
    _LIVarTagEx(LRadioButton, LWidget)

    LRadioButton(std::shared_ptr<LWidget> parent = nullptr,
                LFlags<LWindowType> f=0);

    LRadioButton(const std::wstring &text,
                  std::shared_ptr<LWidget> parent = nullptr,
                  LFlags<LWindowType> f = 0);

protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
};


LK_NAMESPACE_END

#endif // LBUTTON_H
