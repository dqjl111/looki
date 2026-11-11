#ifndef LSCROLLAREA_H
#define LSCROLLAREA_H

#include "lkwidgets/widget/labstractscrollarea.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LScrollArea : public LAbstractScrollArea
{
public:
    _LIVarTagEx(LScrollArea, LWidget)

    LScrollArea(std::shared_ptr<LWidget> parent = nullptr);

    std::shared_ptr<LWidget> widget() const;
    void setWidget(std::shared_ptr<LWidget> widget);
    std::shared_ptr<LWidget> takeWidget();

    bool widgetResizable() const;
    void setWidgetResizable(bool resizable);

    LSize<int> sizeHint() const override;

    bool focusNextPrevChild(bool next) override;

    LFlags<LAlignment> alignment() const;
    void setAlignment(LFlags<LAlignment>);

    void ensureVisible(int x, int y, int xmargin = 50, int ymargin = 50);
    void ensureWidgetVisible(std::shared_ptr<LWidget> childWidget, int xmargin = 50, int ymargin = 50);
    virtual bool eventFilter(std::shared_ptr<LObject> o, std::shared_ptr<LEvent> e) override;
protected:
    virtual void initSpecific() override;
    virtual bool handleEvent(std::shared_ptr<LEvent> e) override;
    virtual void resizeEvent(std::shared_ptr<LResizeEvent> e) override;
    virtual void scrollContentsBy(int dx, int dy) override;
    virtual LSize<int> viewportSizeHint() const override;
    virtual bool viewportEvent(std::shared_ptr<LEvent> e) override;

    void updateWidgetPosition();
    void updateScrollBars();

    std::shared_ptr<LWidget> m_widget;
    mutable LSize<int> m_widgetSize;
    bool m_resizable = false;
    LFlags<LAlignment> m_alignment = LAlignment::Left|LAlignment::Top;
};

LK_NAMESPACE_END

#endif // LSCROLLAREA_H
