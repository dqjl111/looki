#ifndef LABSTRACTBUTTON_H
#define LABSTRACTBUTTON_H
#include "lkwidgets/widget/llabel.h"

LK_NAMESPACE_BEGIN
class LKWIDGETSDEF LAbstractButton  : public LWidget
{
public:
    LAbstractButton(LSizePolicy::ControlType type,
                    std::shared_ptr<LWidget> parent = nullptr,
                    LFlags<LWindowType> f=0);


    virtual bool handleEvent(std::shared_ptr<LEvent> event) override;
    bool isDown() const;

    void setChecked(bool);
    void click();

    void setCheckable(bool);
    bool isCheckable() const;

    void setAutoExclusive(bool autoExclusive);
    bool autoExclusive() const;
    virtual uint16_t uiElementState(LUiElementType type = LUiElementType::None) const override;

    LSignal<> sigPressed;
    LSignal<> sigReleased;
    LSignal<bool> sigClicked;
    LSignal<bool> sigToggled;
protected:
    void keyPressEvent(std::shared_ptr<LKeyEvent> e) override;
    void keyReleaseEvent(std::shared_ptr<LKeyEvent>  e) override;
    void mousePressEvent(std::shared_ptr<LMouseEvent> e) override;
    void mouseReleaseEvent(std::shared_ptr<LMouseEvent> e) override;
    void mouseMoveEvent(std::shared_ptr<LMouseEvent> e) override;
    void focusInEvent(std::shared_ptr<LFocusEvent> e) override;
    void focusOutEvent(std::shared_ptr<LFocusEvent> e) override;
    void changeEvent(std::shared_ptr<LEvent> e) override;

    virtual void setDown(bool);
    virtual bool hitButton(const LPoint<int> &pos) const;
    virtual void checkStateSet();
    virtual void nextCheckState();


    void notifyChecked();
    void refresh();
    void innerClick();
    std::list<std::shared_ptr<LAbstractButton> > queryButtonList() const;
    std::shared_ptr<LAbstractButton> queryCheckedButton() const;
    void fixFocusPolicy();
    void moveFocus(int key);

    union{
        uint16_t m_buttonFlags = 0;
        struct{
            uint16_t m_checkable :1;
            uint16_t m_checked :1;
            uint16_t m_autoExclusive :1;
            uint16_t m_down :1;
            uint16_t m_blockRefresh :1;
            uint16_t m_pressed : 1;
            uint16_t m_isDefault: 1;
            uint16_t m_autoDefault : 2;
        };
    };
    int m_shortcutId = 0;
};


LK_NAMESPACE_END

#endif // LABSTRACTBUTTON_H
