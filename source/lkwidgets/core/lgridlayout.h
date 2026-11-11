#ifndef LGRIDLAYOUT_H
#define LGRIDLAYOUT_H
#include "llayout.h"

LK_NAMESPACE_BEGIN

class LGridItemData;
struct LGridLayoutSizeTriple;

class LKWIDGETSDEF LGridLayout: public LLayout
{
public:
    LGridLayout();

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSize() const override;
    virtual LSize<int> maximumSize() const override;
    virtual LFlags<LOrientation> expandingDirections() const override;
    virtual void setGeometry(const LRect<int>& r) override;

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int) const override;
    virtual int minimumHeightForWidth(int) const override;
    virtual void invalidate() override;

    uint32_t colNum() const;
    uint32_t rowNum() const;

    void setHorizontalSpacing(int spacing);
    inline int horizontalSpacing() const{
        return m_horizontalSpacing;
    }
    void setVerticalSpacing(int spacing);
    int verticalSpacing() const{
        return m_verticalSpacing;
    }
    void setSpacing(int spacing);
    bool setPositionData(uint32_t posId,
                         uint32_t row, uint32_t column,
                         uint32_t rowSpan=1, uint32_t columnSpan=1,
                         bool isHold = true);

    int cellPos(uint32_t row, uint32_t column);
    LRect<int> posCell(uint32_t posId);
protected:
    LSize<int> findSize(int LLayoutItemData::*, uint32_t hSpacing, uint32_t vSpacing) const;
    void setupLayoutData(uint32_t hSpacing, uint32_t vSpacing);

    void setSize(uint32_t r, uint32_t c);
    inline void setDirty() { m_needRecalc = true; m_hfwWidth = -1; }
    inline bool isDirty() const { return m_needRecalc; }

    int heightForWidth(int w, uint32_t& paramHeight);

    std::vector<std::shared_ptr<LLayoutItemData> > m_rowData;
    std::vector<std::shared_ptr<LLayoutItemData> > m_colData;
    std::vector<std::shared_ptr<LLayoutItemData> > m_hfwData;
    std::vector<int> m_rStretch;
    std::vector<int> m_cStretch;
    std::vector<int> m_rMinHeights;
    std::vector<int> m_cMinWidths;


    uint32_t m_colNum = 0;
    uint32_t m_rowNum = 0;

    uint32_t m_hfwWidth = 0;
    uint32_t m_hfwHeight = 0;
    uint32_t m_hfwMinHeight = 0;

    uint32_t m_nextR = 0;
    uint32_t m_nextC = 0;

    uint32_t m_horizontalSpacing = 0;
    uint32_t m_verticalSpacing = 0;

    union{
        uint32_t m_flags = 0;
        struct{
            uint8_t m_hReversed : 1;
            uint8_t m_vReversed : 1;
            uint8_t m_needRecalc : 1;
            uint8_t m_hasHfw : 1;
            uint8_t m_addVertical : 1;
        };
    };

private:
    void addData(std::shared_ptr<LLayoutItem> item,
                 std::shared_ptr<LGridItemData> itemData,
                 const LGridLayoutSizeTriple& sizes,
                 bool r,
                 bool c);

    void setupSpacings(std::vector<std::shared_ptr<LLayoutItemData> >&chain,
                       std::vector<std::shared_ptr<LLayoutItem> >& grid,
                       uint32_t fixedSpacing,
                       LOrientation orientation);

    void distribute(LRect<int> r, int hSpacing, int vSpacing);

    void recalcHFW(int w);
    void setupHfwLayoutData();

    void addHfwData(std::shared_ptr<LLayoutItem> item,
                    int row,
                    int width);

    void buildGridRowCol(uint32_t& rowNum, uint32_t& colNum) const;
};


LK_NAMESPACE_END
#endif // LGRIDLAYOUT_H
