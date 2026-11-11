#ifndef LSTACKEDLAYOUT_H
#define LSTACKEDLAYOUT_H
#include "llayout.h"
#include "lkcore/lsignal.h"

LK_NAMESPACE_BEGIN


class LKWIDGETSDEF LStackedLayout : public LLayout
{
public:
    LStackedLayout(bool isStackAll = false);

    void setCurrentIndex(int index);
    inline int currentIndex() const{
        return m_current;
    }

    std::shared_ptr<LLayoutItem> currentItem();
    std::shared_ptr<LWidget> currentWidget();

    LSize<int> sizeHint() const override;
    LSize<int> minimumSize() const override;
    virtual void setGeometry(const LRect<int>& r) override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;

    virtual void raise() override;
    virtual void lower() override;
    virtual bool setFocus() override;

    virtual int addItem(std::shared_ptr<LLayoutItem> child, int pos = -1, bool update = true) override;
    virtual int addWidget(std::shared_ptr<LWidget> w,
                          LFlags<LAlignment> align = LAlignment::None,
                          int pos = -1,
                          bool update = true) override;

    virtual int removeItem(std::shared_ptr<LLayoutItem> child, bool update = true) override;
    virtual void removeItem(uint32_t pos, bool update = true) override;

    LSignal<int> sigCurrentChanged;
protected:
    int m_current=-1;
    bool m_isStackAll = false;
};

LK_NAMESPACE_END

#endif // LSTACKEDLAYOUT_H
