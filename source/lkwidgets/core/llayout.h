#ifndef LLAYOUT_H
#define LLAYOUT_H

#include "lkwidgets/core/llayoutitem.h"
#include "lkcore/lobject.h"
#include <list>

LK_NAMESPACE_BEGIN

struct LLayoutPosition
{
    std::shared_ptr<LLayoutItem> m_item;
    std::shared_ptr<LIVar> m_posData;
    bool m_isHoldPos = false; //无Item时是否保留位置

    LLayoutPosition(){}
    LLayoutPosition(std::shared_ptr<LLayoutItem> item,
                    std::shared_ptr<LIVar> posData = nullptr,
                    bool isHoldPos = false)
        :m_item(item)
        ,m_posData(posData)
        ,m_isHoldPos(isHoldPos)
    {
    }

    bool isEmpty() const{
        if(m_item == nullptr){
            return m_isHoldPos == false;
        }
        return m_item->isEmpty();
    }

    int hfw(int w) {
        if (m_item->hasHeightForWidth()) {
            return m_item->heightForWidth(w);
        } else {
            return m_item->sizeHint().height();
        }
    }
    int mhfw(int w) {
        if (m_item->hasHeightForWidth()) {
            return m_item->heightForWidth(w);
        } else {
            return m_item->minimumSize().height();
        }
    }
};

class LPainter;
class LKWIDGETSDEF LLayout : public LObject, public LLayoutItem
{
public:
    LLayout();

    enum SizeConstraint {
        SetDefaultConstraint,
        SetNoConstraint,
        SetMinimumSize,
        SetFixedSize,
        SetMaximumSize,
        SetMinAndMaxSize
    };

    std::shared_ptr<LWidget> parentWidget() const;

    bool activate();
    int totalHeightForWidth(int w) const;
    LSize<int> totalSizeHint() const;
    LSize<int> totalMinimumSize() const;
    LSize<int> totalMaximumSize() const;

    void setContentsMargins(int left, int top, int right, int bottom);
    void setContentsMargins(const LMargins<int> &margins);
    const LMargins<int> contentsMargins() const;
    LRect<int> contentsRect() const;

    void reparentChildWidgets(std::shared_ptr<LWidget> mw);
    void update();

    virtual std::shared_ptr<LLayout> layout() override;

    virtual LSize<int> minimumSize() const override;
    virtual LSize<int> maximumSize() const override;
    virtual LFlags<LOrientation> expandingDirections() const override;
    virtual void setGeometry(const LRect<int>& r) override;
    virtual LRect<int> geometry() const override;
    virtual bool isEmpty() const override;
    virtual uint32_t controlTypes() const override;

    void holdPosition(int posId = -1, bool isHold=true);


    inline std::shared_ptr<LObject> parent() const{
        return m_parent.lock();
    }

    template<class T>
    std::shared_ptr<T> parentT() const{
        std::shared_ptr<LObject> t = m_parent.lock();
        return std::dynamic_pointer_cast<T>(t);
    }

    inline int count() const{
        return (int)m_positions.size();
    }
    std::shared_ptr<LLayoutItem> itemAt(uint32_t i);
    int itemPos(std::shared_ptr<LLayoutItem> item);
    bool hasItem(uint32_t pos);

    void moveTothread(LIThread* thread) override;
    void setParentWidget(std::shared_ptr<LWidget> parent);
    int setParent(std::shared_ptr<LLayout> parent, int pos = -1);

    void clearInvalidPos(bool update = true);

    virtual int removeItem(std::shared_ptr<LLayoutItem> child, bool update = true);
    virtual void removeItem(uint32_t pos, bool update = true);
    virtual int addItem(std::shared_ptr<LLayoutItem> child, int pos = -1, bool update = true);

    virtual int addWidget(std::shared_ptr<LWidget> w,
                   LFlags<LAlignment> align = LAlignment::None,
                   int pos = -1,
                   bool update = true);
    int removeWidget(std::shared_ptr<LWidget> w, bool update = true);
    std::shared_ptr<LWidget> widget(int index);
    int widgetPos(const std::shared_ptr<LWidget> w);

    bool setAlignment(std::shared_ptr<LWidget> w, LFlags<LAlignment> align, bool update = true);
    bool setAlignment(std::shared_ptr<LLayoutItem> l, LFlags<LAlignment> align, bool update = true);

    virtual void invalidate() override;
    virtual void paint(LPainter* painter);

    static void maxExpCalc(int & max, bool &exp, bool &empty,
                           int boxmax, bool boxexp, bool boxempty);

    void setEnabled(bool);
    bool isEnabled() const;

    void widgetEvent(std::shared_ptr<LEvent> e);

    virtual void raise() override;
    virtual void lower() override;
    virtual void show() override;
    virtual void hide() override;
    virtual void clearFocus() override;
    virtual bool containWidget(std::shared_ptr<LWidget>) override;
    virtual bool setFocus() override;
protected:
    static void activateRecursiveHelper(std::shared_ptr<LLayout> item);
    void doResize();
    void initParentWidget(std::shared_ptr<LWidget> parent);
    int initParent(std::shared_ptr<LLayout> parent, int pos);

    void addChildWidget(std::shared_ptr<LWidget> w);

    int addItemHelper(std::shared_ptr<LLayoutItem> child, int pos);
    LRect<int> alignmentRect(const LRect<int>&) const;

    int m_leftMargin = 0;
    int m_topMargin = 0;
    int m_rightMargin = 0;
    int m_bottomMargin = 0;

    union{
        uint16_t m_flags = 0;
        struct{
            uint16_t m_topLevel : 1;
            uint16_t m_enabled : 1;
            uint16_t m_activated : 1;
            uint16_t m_autoNewChild : 1;
            uint16_t m_isHide;
            SizeConstraint m_constraint : 3;
        };
    };
    LRect<int> m_rect;

    std::weak_ptr<LObject> m_parent;
    std::vector<LLayoutPosition> m_positions;


};

LK_NAMESPACE_END

#endif // LLAYOUT_H
