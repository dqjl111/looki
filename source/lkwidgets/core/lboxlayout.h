#ifndef LBOXLAYOUT_H
#define LBOXLAYOUT_H
#include "llayout.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LBoxLayout : public LLayout
{
public:
    enum Direction { LeftToRight, RightToLeft, TopToBottom, BottomToTop,
                     Down = TopToBottom, Up = BottomToTop };

    LBoxLayout(Direction dir);

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSize() const override;
    virtual LSize<int> maximumSize() const override;
    virtual LFlags<LOrientation> expandingDirections() const override;
    virtual void setGeometry(const LRect<int>& r) override;

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int) const override;
    virtual int minimumHeightForWidth(int) const override;
    virtual void invalidate() override;

    inline uint32_t spacing() const{
        return m_spacing;
    }
    void setSpacing(uint32_t spacing);
protected:
    void setupGeom();
    void setDirty();
    void calcHfw(int w);
    void calcGeomForWidth(int w);

    void fetchValidItemDatas(std::vector<std::shared_ptr<LLayoutItemData>>& datas);


    uint32_t m_hfwWidth = 0;
    uint32_t m_hfwHeight = 0;
    uint32_t m_hfwMinHeight = 0;
    LSize<int>  m_sizeHint;
    LSize<int> m_minSize;
    LSize<int> m_maxSize;
    LFlags<LOrientation> m_expanding;
    uint32_t m_spacing = 0;
    union{
        uint8_t m_boxFlags = 0;
        struct{
            uint8_t m_hasHfw : 1;
            uint8_t m_dirty : 1;
            Direction m_dir: 6;
        };
    };
};

class LKWIDGETSDEF LHBoxLayout : public LBoxLayout
{
public:
    LHBoxLayout();
};

class LKWIDGETSDEF LVBoxLayout : public LBoxLayout
{
public:
    LVBoxLayout();
};

LK_NAMESPACE_END

#endif // LBOXLAYOUT_H
