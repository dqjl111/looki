#ifndef LABSTRACTSLIDER_H
#define LABSTRACTSLIDER_H

#include "lkwidgets/core/lwidget.h"
#include "lkcore/time/ltimer.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LSliderData
{
public:
    LSliderData(LOrientation orient = LOrientation::Horizontal);

    int m_minimum = 0;
    int m_maximum = 99;
    int m_pageStep = 10;
    int m_value = 0;
    int m_position = 0;
    int m_singleStep = 1;

    union{
        uint8_t m_sliderFlags = 0;
        struct{
            uint8_t m_tracking : 1;
            uint8_t m_blocktracking :1;
            uint8_t m_pressed : 1;
            uint8_t m_invertedAppearance : 1;
            LOrientation m_orientation : 2;
        };
    };

    bool upsideDown(LLayoutDirection dir) const;
};

class LKWIDGETSDEF LAbstractSlider :  public LWidget, protected LSliderData
{
public:
    LAbstractSlider(std::shared_ptr<LWidget> parent = nullptr,
                    LOrientation orient = LOrientation::Horizontal);

    inline int value() const{
        return m_value;
    }
    void setValue(int v);

    inline LOrientation orientation() const{
        return m_orientation;
    }
    void setOrientation(LOrientation);

    void setRange(int min, int max);

    void setMinimum(int);
    inline int minimum() const{
        return m_minimum;
    }

    void setMaximum(int);
    inline int maximum() const{
        return m_maximum;
    }

    void setSingleStep(int);
    inline int singleStep() const{
        return m_singleStep;
    }

    void setPageStep(int);
    inline int pageStep() const{
        return m_pageStep;
    }

    void setSteps(int single, int page);

    void setSliderDown(bool);
    inline bool isSliderDown() const{
        return m_pressed;
    }

    void setSliderPosition(int);
    inline int sliderPosition() const{
        return m_position;
    }

    inline void setTracking(bool enable){
        m_tracking = enable;
    }
    inline bool hasTracking() const{
        return m_tracking;
    }

    enum SliderAction {
        SliderNoAction,
        SliderSingleStepAdd,
        SliderSingleStepSub,
        SliderPageStepAdd,
        SliderPageStepSub,
        SliderToMinimum,
        SliderToMaximum,
        SliderMove
    };
    void triggerAction(SliderAction action);

    LSignal<int> sigValueChanged;
    LSignal<int, int> sigRangeChanged;
protected:
    int overflowSafeAdd(int add) const;

    void setRepeatAction(SliderAction action, int thresholdTime = 500, int repeatTime = 50);
    void doRepeat();

    int m_repeatActionTime  = 0;
    SliderAction m_repeatAction= SliderNoAction;
    std::shared_ptr<LTimer> m_repeatTimer;
};

LK_NAMESPACE_END

#endif // LABSTRACTSLIDER_H
