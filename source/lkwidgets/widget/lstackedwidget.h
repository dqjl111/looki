#ifndef LSTACKEDWIDGET_H
#define LSTACKEDWIDGET_H
#include "lkwidgets/core/lstackedlayout.h"
#include "lkwidgets/widget/lframe.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LStackedWidget : public LWidget
{
public:
    LStackedWidget(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);

    int addWidget(std::shared_ptr<LWidget> w);
    int insertWidget(int index, std::shared_ptr<LWidget> w);
    void removeWidget(std::shared_ptr<LWidget> w);

    std::shared_ptr<LWidget> currentWidget() const;
    int currentIndex() const;

    int indexOf(const std::shared_ptr<LWidget> ) const;
    std::shared_ptr<LWidget> widget(int) const;
    int count() const;

    void setCurrentIndex(int index);
    void setCurrentWidget(std::shared_ptr<LWidget> w);

    std::shared_ptr<LStackedLayout> stackedLayout();
protected:
    virtual void initSpecific() override;

};

LK_NAMESPACE_END

#endif // LSTACKEDWIDGET_H
