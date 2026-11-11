#ifndef LFRAME_H
#define LFRAME_H
#include "lkwidgets/core/lwidget.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LFrameData{
public:
    LFrameData();
    LFrameData(uint8_t shape, uint8_t shadow, uint8_t lineWidth, uint8_t midLineWidth);

    enum Shape : uint8_t{
        NoFrame, // no frame
        Box, // rectangular box
        Panel, // rectangular panel
        WinPanel, // rectangular panel (Windows)
        HLine, // horizontal line
        VLine, // vertical line
        StyledPanel
    };

    enum Shadow : uint8_t{
        Plain, // plain line
        Raised, // raised shadow effect
        Sunken // sunken shadow effect
    };


    Shape frameShape() const;
    void setFrameShape(Shape s);
    Shadow frameShadow() const;
    void setFrameShadow(Shadow s);

    uint8_t lineWidth() const;
    void setLineWidth(uint8_t w);

    uint8_t midLineWidth() const;
    void setMidLineWidth(uint8_t w);

    uint8_t frameWidth() const;

    uint8_t m_shape = NoFrame;
    uint8_t m_shadow = Plain;
    uint8_t m_lineWidth = 0;
    uint8_t m_midLineWidth = 0;
};

class LKWIDGETSDEF LFrame : public LWidget, protected LFrameData
{
public:
    _LIVarTagEx(LFrame, LWidget)

    LFrame(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);
protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
};


LK_NAMESPACE_END

#endif // LFRAME_H
