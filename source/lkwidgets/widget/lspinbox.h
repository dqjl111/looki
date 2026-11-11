#ifndef LSPINBOX_H
#define LSPINBOX_H

#include "lkwidgets/widget/labstractlineedit.h"
#include "lkwidgets/widget/llineedit.h"
#include "lkcore/time/ltimer.h"
#include "lkwidgets/widget/labstractslider.h"

LK_NAMESPACE_BEGIN


class LKWIDGETSDEF LSpinBoxData
{
public:
    LSpinBoxData();

    double m_minimum = 0;
    double m_maximum = 99;
    double m_value = 0;
    double m_step = 1;
    uint16_t m_decimals =0;

    union{
        uint8_t m_spinBoxFlags = 0;
        struct{
            uint8_t m_tracking : 1;
            uint8_t m_blocktracking :1;
            uint8_t m_pressed : 1;
            uint8_t m_invertedAppearance : 1;
            uint8_t m_invertedControls : 1;
            uint8_t m_sameSideSpinButton : 1;
        };
    };
};

class LKWIDGETSDEF LSpinBox : public LAbstractLineEdit, protected LSpinBoxData
{
public:
    _LIVarTagEx(LSpinBox, LWidget)

    LSpinBox(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f =0);

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSizeHint() const override;

    double value() const;
    void setValue(double value);
    inline uint16_t decimals() const{
        return m_decimals;
    }
    void setDecimals(uint16_t dec);
    void setRange(double min, double max);

    void setSingleStep(double step);
    inline double singleStep()const{
        return m_step;
    }

    LSignal<double> sigValueChanged;
    LSignal<double, double> sigRangeChanged;

    virtual LVariant inputMethodQuery(LInputMethodQuery, const LVariant& args = LVariant()) const override;
protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseMoveEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void keyPressEvent(std::shared_ptr<LKeyEvent> ev) override;
    virtual void focusOutEvent(std::shared_ptr<LFocusEvent> ev) override;
    virtual void wheelEvent(std::shared_ptr<LWheelEvent> event) override;
    virtual bool enterSubItem(const LPoint<int>& p) override;

    virtual void initSpecific() override;
    virtual uint16_t uiElementState(LUiElementType type = LUiElementType::None) const override;
    virtual LRect<int> editRect() const override;
    virtual bool checkOpResult() override;
    void endInput();

    void setRepeatAction(LAbstractSlider::SliderAction action, int thresholdTime = 500, int repeatTime = 50);
    void doRepeat();
    void triggerAction(LAbstractSlider::SliderAction action);
    void stopRepeatAction();

    void activateControl(LUiSubCtrl control, int threshold= 500);

    void doUp(){
        setValue(m_value+m_step);
    }

    void doDown(){
        setValue(m_value-m_step);
    }

    LUiSubCtrl m_pressedCtrl;
    LUiSubCtrl m_hoverCtrl;

    std::shared_ptr<LTimer> m_repeatTimer;
    int m_repeatActionTime  = 0;
    LAbstractSlider::SliderAction m_repeatAction= LAbstractSlider::SliderNoAction;
};


LK_NAMESPACE_END
#endif // LSPINBOX_H
