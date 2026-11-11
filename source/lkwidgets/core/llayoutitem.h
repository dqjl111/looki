#ifndef LLAYOUTITEM_H
#define LLAYOUTITEM_H
#include "lkgui/lrect.h"
#include "lkgui/lguienums.h"
#include "lkcore/livar.h"
#include "lkwidgets/core/lsizepolicy.h"

LK_NAMESPACE_BEGIN
static const  int LLAYOUTSIZE_MAX = INT_MAX/256/16;

class LLayout;
class LWidget;
class LSpacerItem;
class LKWIDGETSDEF LLayoutItem
{
public:
    explicit LLayoutItem(LFlags<LAlignment> alignment = LFlags<LAlignment>());
    virtual ~LLayoutItem();

    LFlags<LAlignment>  m_alignment;
    virtual LSize<int> sizeHint() const = 0;
    virtual LSize<int> minimumSize() const = 0;
    virtual LSize<int> maximumSize() const = 0;
    virtual LFlags<LOrientation> expandingDirections() const = 0;
    virtual void setGeometry(const LRect<int>&) = 0;
    virtual LRect<int> geometry() const = 0;
    virtual bool isEmpty() const = 0;

    virtual bool hasHeightForWidth() const;
    virtual int heightForWidth(int) const;
    virtual int minimumHeightForWidth(int) const;
    virtual void invalidate();

    virtual void raise();
    virtual void lower();
    virtual void show();
    virtual void hide();
    virtual void clearFocus();
    virtual bool setFocus();

    virtual bool containWidget(std::shared_ptr<LWidget>);

    virtual std::shared_ptr<LWidget> widget() const;
    virtual std::shared_ptr<LLayout> layout();
    virtual std::shared_ptr<LSpacerItem> spacerItem();

    virtual uint32_t controlTypes() const;

    static LSize<int> smartMinSize(const LSize<int> &sizeHint, const LSize<int> &minSizeHint,
                                   const LSize<int> &minSize, const LSize<int> &maxSize,
                                   const LSizePolicy &sizePolicy);

    static LSize<int> smartMaxSize(const LSize<int> &sizeHint,
                                   const LSize<int> &minSize, const LSize<int> &maxSize,
                                   const LSizePolicy &sizePolicy, LFlags<LAlignment> align = LFlags<LAlignment>());


    static LSize<int> smartMinSize(const std::shared_ptr<LWidget> w);
    static LSize<int> smartMaxSize(const std::shared_ptr<LWidget> w, LFlags<LAlignment> align = LFlags<LAlignment>());

    inline int hfw(int w) {
        return hasHeightForWidth()? heightForWidth(w) : sizeHint().height();
    }

    inline int mhfw(int w) {
        return hasHeightForWidth()? heightForWidth(w) : minimumSize().height();
    }
};

class LKWIDGETSDEF LSpacerItem : public LLayoutItem, public LIVar
{
public:
    LSpacerItem(int w, int h,
                LSizePolicy::Policy hData = LSizePolicy::Minimum,
                LSizePolicy::Policy vData = LSizePolicy::Minimum)
        : m_width(w), m_height(h), m_sizeP(hData, vData) { }
    virtual ~LSpacerItem();

    void changeSize(int w, int h,
                    LSizePolicy::Policy hPolicy = LSizePolicy::Minimum,
                    LSizePolicy::Policy vPolicy = LSizePolicy::Minimum);

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSize() const override;
    virtual LSize<int> maximumSize() const override;
    virtual LFlags<LOrientation> expandingDirections() const override;
    virtual bool isEmpty() const override;
    virtual void setGeometry(const LRect<int>&) override;
    virtual LRect<int> geometry() const override;
    virtual std::shared_ptr<LSpacerItem> spacerItem() override;

    LSizePolicy sizePolicy() const { return m_sizeP; }
private:
    int m_width;
    int m_height;
    LSizePolicy m_sizeP;
    LRect<int> m_rect;
};

class LKWIDGETSDEF LWidgetItem : public LLayoutItem
{
public:
    explicit LWidgetItem(std::shared_ptr<LWidget> w) : m_wid(w) { }
    virtual ~LWidgetItem();

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSize() const override;
    virtual LSize<int> maximumSize() const override;
    virtual LFlags<LOrientation> expandingDirections() const override;
    virtual bool isEmpty() const override;
    virtual void setGeometry(const LRect<int>&) override;
    virtual LRect<int> geometry() const override;

    virtual std::shared_ptr<LWidget> widget() const override;

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int) const override;
    virtual uint32_t controlTypes() const override;

    virtual void raise() override;
    virtual void lower() override;
    virtual void show() override;
    virtual void hide() override;
    virtual void clearFocus() override;
    virtual bool containWidget(std::shared_ptr<LWidget>) override;
    virtual bool setFocus() override;
private:
    std::weak_ptr<LWidget> m_wid;
};


class LLayoutItemData : public LIVar
{
public:
    LLayoutItemData(){}

    inline void init(int stretchFactor = 0, int minSize = 0) {
        stretch = stretchFactor;
        minimumSize = sizeHint = minSize;
        maximumSize = LLAYOUTSIZE_MAX;
        expansive = false;
        empty = true;
        spacing = 0;
    }

    int smartSizeHint() {
        return (stretch > 0) ? minimumSize : sizeHint;
    }
    int effectiveSpacer(int uniformSpacer) const {
        return (uniformSpacer >= 0) ? uniformSpacer : spacing;
    }

    int hStretch(std::shared_ptr<LLayoutItem> item);
    int vStretch(std::shared_ptr<LLayoutItem> item);

    void copy(std::shared_ptr<LLayoutItemData> d);
    // parameters
    int stretch = 0;
    int sizeHint = 0;
    int maximumSize =0;
    int minimumSize = 0;
    int spacing = 0;
    bool expansive = false;
    bool empty = false;

    // temporary storage
    bool done = 0;

    // result
    int size = 0; //在屏幕中的大小
    int pos = 0;  //屏幕中的像素位置

    static void geomCalc(std::vector<std::shared_ptr<LLayoutItemData>>& chain,
                         int start, int count,  int pos, int space, int spacer=-1);
};


LK_NAMESPACE_END

#endif // LLAYOUTITEM_H
