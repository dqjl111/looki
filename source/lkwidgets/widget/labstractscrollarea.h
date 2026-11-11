#ifndef LABSTRACTSCROLLAREA_H
#define LABSTRACTSCROLLAREA_H
#include "lkwidgets/widget/lframe.h"
#include "lkwidgets/widget/lscrollbar.h"

LK_NAMESPACE_BEGIN
class LKWIDGETSDEF LAbstractScrollArea : public LFrame
{
public:
    LAbstractScrollArea(std::shared_ptr<LWidget> parent = nullptr);

    enum SizeAdjustPolicy {
        AdjustIgnored,
        AdjustToContentsOnFirstShow,
        AdjustToContents
    };

    std::shared_ptr<LWidget> viewport() const;
    LSize<int> maximumViewportSize() const;
    LPoint<int> contentsOffset() const;

    LSize<int> minimumSizeHint() const override;
    LSize<int> sizeHint() const override;
    SizeAdjustPolicy sizeAdjustPolicy() const;
    void setSizeAdjustPolicy(SizeAdjustPolicy policy);

    LScrollPolicy verticalScrollBarPolicy() const;
    void setVerticalScrollBarPolicy(LScrollPolicy policy);

    LScrollPolicy horizontalScrollBarPolicy() const;
    void setHorizontalScrollBarPolicy(LScrollPolicy policy);

    std::shared_ptr<LScrollBar> verticalScrollBar() const;
    void setVerticalScrollBar(std::shared_ptr<LScrollBar> scrollBar);

    std::shared_ptr<LScrollBar> horizontalScrollBar() const;
    void setHorizontalScrollBar(std::shared_ptr<LScrollBar> scrollBar);

    virtual bool handleEvent(std::shared_ptr<LEvent> e) override;
    virtual bool eventFilter(std::shared_ptr<LObject> o, std::shared_ptr<LEvent> e) override;
    virtual void blink() override;
protected:
    void initScroll();
    virtual void initSpecific() override;
    virtual void scrollBarPolicyChanged(LOrientation, LScrollPolicy) {}
    virtual void initViewport();
    virtual void keyPressEvent(std::shared_ptr<LKeyEvent> ev) override;
    virtual void inputMethodEvent(std::shared_ptr<LInputMethodEvent> ev) override;
    virtual bool viewportEvent(std::shared_ptr<LEvent> e);

    void hSlide(int x);
    void vSlide(int y);
    void hRangeChange(int, int);
    void vRangeChange(int, int);

    void layoutChildren();
    void layoutChildrenHelper(bool *needHBar, bool *needVBar);
    virtual LSize<int> viewportSizeHint() const;
    virtual void scrollContentsBy(int dx, int dy);
    bool canStartScrollingAt(const LPoint<int> &startPos) const;

    void replaceScrollBar(std::shared_ptr<LScrollBar> scrollBar, LOrientation orientation);


    std::shared_ptr<LWidget> m_viewport;
    std::shared_ptr<LScrollBar> m_hBar;
    std::shared_ptr<LScrollBar> m_vBar;

    LScrollPolicy m_hPolicy = LScrollPolicy::AsNeeded;
    LScrollPolicy m_vPolicy = LScrollPolicy::AsNeeded;

    bool m_shownOnce = false;
    bool m_inResize = false;
    mutable LSize<int> m_sizeHint;
    SizeAdjustPolicy m_sizeAdjustPolicy;

    std::shared_ptr<LWidget> m_cornerWidget;
    LRect<int> m_cornerPaintingRect;

    LMargins<int> m_viewMargin; // viewport margin
    LPoint<int> m_offset;
    LPoint<int> m_overshoot;
};

LK_NAMESPACE_END

#endif // LABSTRACTSCROLLAREA_H
