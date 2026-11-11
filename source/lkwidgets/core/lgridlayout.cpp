#include "lgridlayout.h"
#include "lwidget.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN
/*
  Modify total maximum (max), total expansion (exp), and total empty
  when adding boxmax/boxexp.

  Expansive boxes win over non-expansive boxes.
  Non-empty boxes win over empty boxes.
*/
static inline void lMaxExpCalc(int & max, bool &exp, bool &empty,
                               int boxmax, bool boxexp, bool boxempty)
{
    if (exp) {
        if (boxexp){
            max = std::max<int>(max, boxmax);
        }
    } else {
        if (boxexp || (empty && (!boxempty || max == 0))){
            max = boxmax;
        }else if (empty == boxempty){
            max = std::min<int>(max, boxmax);
        }
    }
    exp = exp || boxexp;
    empty = empty && boxempty;
}

static void initEmptyMultiBox(std::vector<std::shared_ptr<LLayoutItemData> > &chain, int start, int end)
{
    for (int i = start; i <= end; i++) {
        std::shared_ptr<LLayoutItemData> data = chain.at(i);
        if (data->empty && data->maximumSize == 0){ // truly empty box
            data->maximumSize = LWIDGETSIZE_MAX;
        }
        data->empty = false;
    }
}


struct LGridLayoutSizeTriple
{
    LSize<int> minS;
    LSize<int> hint;
    LSize<int> maxS;
};

class LGridItemData : public LIVar
{
public:
    LGridItemData(){}

    int row = 0, col = 0;
    int torow = -1, tocol = -1;

    inline int toRow(int rr) const { return torow >= 0 ? torow : rr - 1; }
    inline int toCol(int cc) const { return tocol >= 0 ? tocol : cc - 1; }

    int hStretch(std::shared_ptr<LLayoutItem> item) {
        if(item != nullptr){
            std::shared_ptr<LWidget> w = item->widget();
            if(w != nullptr){
                return w->sizePolicy().m_horStretch;
            }
        }
        return 0;
    }

    int vStretch(std::shared_ptr<LLayoutItem> item) {
        if(item != nullptr){
            std::shared_ptr<LWidget> w = item->widget();
            if(w != nullptr){
                return w->sizePolicy().m_verStretch;
            }
        }
        return 0;
    }
};

static std::shared_ptr<LLayoutItem>& gridAt(std::vector<std::shared_ptr<LLayoutItem> >& grid,
                                           int r, int c, int cc,
                                           LOrientation orientation = LOrientation::Vertical)
{
    if (orientation == LOrientation::Horizontal){
        std::swap(r, c);
    }
    return grid[(r * cc) + c];
}

static void distributeMultiBox(std::vector<std::shared_ptr<LLayoutItemData> > &chain,
                               int start, int end, int minSize,
                               int sizeHint, std::vector<int> &stretchArray, int stretch)
{
    int i;
    int w = 0;
    int wh = 0;
    int max = 0;

    for (i = start; i <= end; i++) {
        std::shared_ptr<LLayoutItemData> data = chain[i];
        w += data->minimumSize;
        wh += data->sizeHint;
        max += data->maximumSize;
        if (stretchArray.at(i) == 0){
            data->stretch = std::max<int>(data->stretch, stretch);
        }

        if (i != end) {
            int spacing = data->spacing;
            w += spacing;
            wh += spacing;
            max += spacing;
        }
    }

    if (max < minSize) { // implies w < minSize
        /*
          We must increase the maximum size of at least one of the
          items. geomCalc() will put the extra space in between the
          items. We must recover that extra space and put it
          somewhere. It does not really matter where, since the user
          can always specify stretch factors and avoid this code.
        */
        LLayoutItemData::geomCalc(chain, start, end - start + 1, 0, minSize, -1);
        int pos = 0;
        for (i = start; i <= end; i++) {
            std::shared_ptr<LLayoutItemData> data = chain[i];
            int nextPos = (i == end) ? minSize : chain.at(i + 1)->pos;
            int realSize = nextPos - pos;
            if (i != end){
                realSize -= data->spacing;
            }
            if (data->minimumSize < realSize){
                data->minimumSize = realSize;
            }
            if (data->maximumSize < data->minimumSize){
                data->maximumSize = data->minimumSize;
            }
            pos = nextPos;
        }
    } else if (w < minSize) {
        LLayoutItemData::geomCalc(chain, start, end - start + 1, 0, minSize, -1);
        for (i = start; i <= end; i++) {
            std::shared_ptr<LLayoutItemData> data = chain[i];
            if (data->minimumSize < data->size){
                data->minimumSize = data->size;
            }
        }
    }

    if (wh < sizeHint) {
        LLayoutItemData::geomCalc(chain, start, end - start + 1, 0, sizeHint, -1);
        for (i = start; i <= end; i++) {
            std::shared_ptr<LLayoutItemData> data = chain[i];
            if (data->sizeHint < data->size){
                data->sizeHint = data->size;
            }
        }
    }
}


LGridLayout::LGridLayout()
{

}

LSize<int> LGridLayout::sizeHint() const
{
    LSize<int> result= findSize(&LLayoutItemData::sizeHint, horizontalSpacing(), verticalSpacing());
    result += LSize(m_leftMargin + m_rightMargin, m_topMargin + m_bottomMargin);
    return result;
}

LSize<int> LGridLayout::minimumSize() const
{
    LSize<int> result= findSize(&LLayoutItemData::minimumSize, horizontalSpacing(), verticalSpacing());
    result += LSize(m_leftMargin + m_rightMargin, m_topMargin + m_bottomMargin);
    return result;
}

LSize<int> LGridLayout::maximumSize() const
{
    LSize<int> result= findSize(&LLayoutItemData::maximumSize, horizontalSpacing(), verticalSpacing());
    result += LSize(m_leftMargin + m_rightMargin, m_topMargin + m_bottomMargin);
    return result;
}

LFlags<LOrientation> LGridLayout::expandingDirections() const
{
    const_cast<LGridLayout*>(this)->setupLayoutData(horizontalSpacing(), verticalSpacing());
    LFlags<LOrientation> ret;

    for (uint32_t i=0; i<m_rowNum; ++i) {
        if (m_rowData[i]->expansive) {
            ret |= LOrientation::Vertical;
            break;
        }
    }
    for (uint32_t i=0; i<m_colNum; ++i) {
        if (m_colData[i]->expansive) {
            ret |= LOrientation::Horizontal;
            break;
        }
    }
    return ret;
}

void LGridLayout::setGeometry(const LRect<int> &rect)
{
    if (isDirty() || rect != geometry()) {
        LRect<int> cr = m_alignment ? alignmentRect(rect) : rect;
        distribute(cr, horizontalSpacing(), verticalSpacing());
        LLayout::setGeometry(rect);
    }
}

bool LGridLayout::hasHeightForWidth() const
{
    const_cast<LGridLayout*>(this)->setupLayoutData(horizontalSpacing(), verticalSpacing());
    return m_hasHfw;
}

int LGridLayout::heightForWidth(int w) const
{
    return const_cast<LGridLayout*>(this)->heightForWidth(w, const_cast<LGridLayout*>(this)->m_hfwHeight);
}

int LGridLayout::minimumHeightForWidth(int w) const
{
    return const_cast<LGridLayout*>(this)->heightForWidth(w, const_cast<LGridLayout*>(this)->m_hfwMinHeight);
}

void LGridLayout::invalidate()
{
    setDirty();
    LLayout::invalidate();
}

uint32_t LGridLayout::colNum() const
{
    return m_colNum;
}

uint32_t LGridLayout::rowNum() const
{
    return m_rowNum;
}

void LGridLayout::setHorizontalSpacing(int spacing)
{
    m_horizontalSpacing = spacing;
    invalidate();
}

void LGridLayout::setVerticalSpacing(int spacing)
{
    m_verticalSpacing = spacing;
    invalidate();
}

void LGridLayout::setSpacing(int spacing)
{
    m_horizontalSpacing = m_verticalSpacing = spacing;
    invalidate();
}

bool LGridLayout::setPositionData(uint32_t posId,
                                  uint32_t row, uint32_t column,
                                  uint32_t rowSpan, uint32_t columnSpan,
                                  bool isHold)
{
    uint32_t tRowSpan = rowSpan > 0 ? rowSpan : 1;
    uint32_t tColSpan = columnSpan > 0 ? columnSpan : 1;

    LRect<int> gridRect(row, column, tRowSpan, tColSpan, false);

    uint32_t checkPosId = 0;
    for(const LLayoutPosition& pos : m_positions){
        std::shared_ptr<LGridItemData> box = std::dynamic_pointer_cast<LGridItemData>(pos.m_posData);
        if(box != nullptr){
            if(checkPosId != posId){
                tRowSpan = box->torow > box->row ? box->torow - box->row + 1 : 1;
                tColSpan = box->tocol > box->col ? box->tocol - box->col +1 : 1;
                LRect<int> tRect(box->row, box->col, tRowSpan, tColSpan, false);
                if(gridRect.isIntersects(tRect)){
                    lWarn() << "Cannont set grid position " << posId
                            << " at (" << row << ',' << column
                            << ") with span (" << rowSpan  << ',' << columnSpan
                            << ") for cell already in use! ";
                    return false;
                }
            }
        }
        checkPosId++;
    }

    if(posId >= m_positions.size()){
        m_positions.resize(posId+1);
    }
    LLayoutPosition& pos = m_positions[posId];
    std::shared_ptr<LGridItemData> box = std::dynamic_pointer_cast<LGridItemData>(pos.m_posData);
    if(box == nullptr){
        box = std::make_shared<LGridItemData>();
        pos.m_posData = box;
    }
    box->row = row;
    box->col = column;
    if(rowSpan>0){
        box->torow = row + rowSpan -1;
    }else{
        box->torow = -1;
    }

    if(columnSpan >0){
        box->tocol = column + columnSpan -1;
    }else{
        box->tocol = -1;
    }
    pos.m_isHoldPos = isHold;
    invalidate();
    return true;
}

int LGridLayout::cellPos(uint32_t row, uint32_t column)
{
    int posId = 0;
    uint32_t tRowSpan, tColSpan;
    for(const LLayoutPosition& pos : m_positions){
        std::shared_ptr<LGridItemData> box = std::dynamic_pointer_cast<LGridItemData>(pos.m_posData);
        if(box != nullptr){
            tRowSpan = box->torow > box->row ? box->torow - box->row + 1 : 1;
            tColSpan = box->tocol > box->col ? box->tocol - box->col +1 : 1;
            LRect<int> tRect(box->row, box->col, tRowSpan, tColSpan, false);
            if(tRect.contains(row, column)){
                return posId;
            }
        }
        posId ++;
    }
    return -1;
}

LRect<int> LGridLayout::posCell(uint32_t posId)
{
    if(posId<m_positions.size()){
        std::shared_ptr<LGridItemData> box = std::dynamic_pointer_cast<LGridItemData>(m_positions[posId].m_posData);
        if(box != nullptr){
            uint32_t tRowSpan, tColSpan;
            tRowSpan = box->torow > box->row ? box->torow - box->row + 1 : 1;
            tColSpan = box->tocol > box->col ? box->tocol - box->col +1 : 1;
            return LRect<int>(box->row, box->col, tRowSpan, tColSpan, false);
        }
    }
    return LRect<int>(-1,-1,0,0);
}

LSize<int> LGridLayout::findSize(int LLayoutItemData::* size, uint32_t hSpacing, uint32_t vSpacing) const
{
    const_cast<LGridLayout*>(this)->setupLayoutData(hSpacing, vSpacing);

    int w = 0;
    int h = 0;

    for (uint32_t i =0; i<m_rowNum; ++i){
        const std::shared_ptr<LLayoutItemData>& d = m_rowData[i];
        h += (*d).*size + d->spacing;
    }
    for (uint32_t i =0; i<m_colNum; ++i){
        const std::shared_ptr<LLayoutItemData>& d = m_colData[i];
        w += (*d).*size + d->spacing;
    }

    w = std::min(LLAYOUTSIZE_MAX, w);
    h = std::min(LLAYOUTSIZE_MAX, h);

    return LSize<int>(w, h);
}

void LGridLayout::setupLayoutData(uint32_t hSpacing, uint32_t vSpacing)
{
    if (!m_needRecalc){
        return;
    }

    m_hasHfw = false;
    uint32_t i, rr, cc;
    buildGridRowCol(rr, cc);
    setSize(rr, cc);

    for (i = 0; i < rr; i++) {
        m_rowData[i]->init(m_rStretch.at(i), m_rMinHeights.at(i));
        m_rowData[i]->maximumSize = m_rStretch.at(i) ? LLAYOUTSIZE_MAX : m_rMinHeights.at(i);
    }
    for (i = 0; i < cc; i++) {
        m_colData[i]->init(m_cStretch.at(i), m_cMinWidths.at(i));
        m_colData[i]->maximumSize = m_cStretch.at(i) ? LLAYOUTSIZE_MAX : m_cMinWidths.at(i);
    }

    uint32_t n = (uint32_t)m_positions.size();
    std::vector<LGridLayoutSizeTriple> sizes(n);

    bool hasMulti = false;

    /*
        Grid of items. We use it to determine which items are
        adjacent to which and compute the spacings correctly.
    */
    std::vector<std::shared_ptr<LLayoutItem> > gridData;
    gridData.resize(rr*cc);
    /*
        Initialize 'sizes' and 'grid' data structures, and insert
        non-spanning items to our row and column data structures.
    */
    for (i = 0; i < n; ++i) {
        LLayoutPosition& pos = m_positions.at(i);
        std::shared_ptr<LLayoutItem>  item = pos.m_item;
        if(item != nullptr){
            std::shared_ptr<LGridItemData> box = std::dynamic_pointer_cast<LGridItemData>(pos.m_posData);
            if(box != nullptr){
                LGridLayoutSizeTriple& sizeItem = sizes.at(i);

                sizeItem.minS = item->minimumSize();
                sizeItem.hint = item->sizeHint();
                sizeItem.maxS = item->maximumSize();

                if (item->hasHeightForWidth()){
                    m_hasHfw = true;
                }

                if (box->row == box->toRow(rr)) {
                    addData(item, box, sizeItem, true, false);
                } else {
                    initEmptyMultiBox(m_rowData, box->row, box->toRow(rr));
                    hasMulti = true;
                }

                if (box->col == box->toCol(cc)) {
                    addData(item, box, sizeItem, false, true);
                } else {
                    initEmptyMultiBox(m_colData, box->col, box->toCol(cc));
                    hasMulti = true;
                }

                for (int r = box->row; r <= box->toRow(rr); ++r) {
                    for (int c = box->col; c <= box->toCol(cc); ++c) {
                        gridAt(gridData, r, c, cc) = item;
                    }
                }
            }
        }
    }

    setupSpacings(m_colData, gridData, hSpacing, LOrientation::Horizontal);
    setupSpacings(m_rowData, gridData, vSpacing, LOrientation::Vertical);

    /*
        Insert multicell items to our row and column data structures.
        This must be done after the non-spanning items to obtain a
        better distribution in distributeMultiBox().
    */
    if (hasMulti) {
        for (i = 0; i < n; ++i) {
            const LLayoutPosition& pos = m_positions.at(i);
            std::shared_ptr<LLayoutItem>  item = pos.m_item;
            if(item != nullptr){
                std::shared_ptr<LGridItemData> box = std::dynamic_pointer_cast<LGridItemData>(pos.m_posData);
                if(box != nullptr){
                    if (box->row != box->toRow(rr)){
                        LGridLayoutSizeTriple& sizeItem = sizes.at(i);

                        distributeMultiBox(m_rowData, box->row, box->toRow(rr), sizeItem.minS.height(),
                                           sizeItem.hint.height(), m_rStretch, box->vStretch(item));
                    }
                    if (box->col != box->toCol(cc)){
                        LGridLayoutSizeTriple& sizeItem = sizes.at(i);

                        distributeMultiBox(m_colData, box->col, box->toCol(cc), sizeItem.minS.width(),
                                           sizeItem.hint.width(), m_cStretch, box->hStretch(item));
                    }
                }
            }
        }
    }

    for (i = 0; i < m_rowNum; i++){
        std::shared_ptr<LLayoutItemData>& d= m_rowData.at(i);
        d->expansive = d->expansive || d->stretch > 0;
    }
    for (i = 0; i < m_colNum; i++){
        std::shared_ptr<LLayoutItemData>& d= m_colData.at(i);
        d->expansive = d->expansive || d->stretch > 0;
    }
    m_needRecalc = false;
}

void LGridLayout::setSize(uint32_t r, uint32_t c)
{
    uint32_t i = (uint32_t)m_rowData.size();
    if(i < r){
        m_rowData.resize(r);
        m_rStretch.resize(r);
        m_rMinHeights.resize(r);
        for (; i < r; i++) {
            if(m_rowData[i] == nullptr){
                m_rowData[i] = std::make_shared<LLayoutItemData>();
                m_rowData[i]->empty =true;
                m_rStretch[i] = 0;
                m_rMinHeights[i] = 0;
            }
        }
    }

    i = (uint32_t)m_colData.size();
    if(i < c){
        m_colData.resize(c);
        m_cStretch.resize(c);
        m_cMinWidths.resize(c);
        for (; i < c; i++) {
            if(m_colData[i] == nullptr){
                m_colData[i] = std::make_shared<LLayoutItemData>();
                m_colData[i]->empty = true;
                m_cStretch[i] = 0;
                m_cMinWidths[i] = 0;
            }
        }
    }

    m_rowNum = r;
    m_colNum = c;
    m_hfwData.clear();
    m_hfwWidth = 0;
}

int LGridLayout::heightForWidth(int w, uint32_t &paramHeight)
{
    setupLayoutData(horizontalSpacing(), verticalSpacing());
    if (!m_hasHfw){
        return 0;
    }

    int hMargins = m_leftMargin + m_rightMargin;
    if (w - hMargins != m_hfwWidth) {
        LLayoutItemData::geomCalc(m_colData, 0, m_colNum, 0, w - hMargins, -1);
        recalcHFW(w - hMargins);
    }
    return paramHeight + m_topMargin + m_bottomMargin;
}

void LGridLayout::addData(std::shared_ptr<LLayoutItem> item,
                          std::shared_ptr<LGridItemData> itemData,
                          const LGridLayoutSizeTriple& sizes,
                          bool r,
                          bool c)
{
    const std::shared_ptr<LWidget> widget = item->widget();

    if (item->isEmpty() && (widget != nullptr)){
        return;
    }

    if (c) {
        std::shared_ptr<LLayoutItemData> data = m_colData.at(itemData->col);
        if (!m_cStretch.at(itemData->col)){
            data->stretch = std::max<int>(data->stretch, itemData->hStretch(item));
        }
        data->sizeHint = std::max<int>(sizes.hint.width(), data->sizeHint);
        data->minimumSize = std::max<int>(sizes.minS.width(), data->minimumSize);

        lMaxExpCalc(data->maximumSize, data->expansive, data->empty, sizes.maxS.width(),
                    item->expandingDirections() & LOrientation::Horizontal, item->isEmpty());
    }
    if (r) {
        std::shared_ptr<LLayoutItemData> data = m_rowData.at(itemData->row);
        if (!m_rStretch.at(itemData->row)){
            data->stretch = std::max<int>(data->stretch, itemData->vStretch(item));
        }
        data->sizeHint = std::max<int>(sizes.hint.height(), data->sizeHint);
        data->minimumSize = std::max<int>(sizes.minS.height(), data->minimumSize);

        lMaxExpCalc(data->maximumSize, data->expansive, data->empty, sizes.maxS.height(),
                    item->expandingDirections() & LOrientation::Vertical, item->isEmpty());
    }
}

void LGridLayout::setupSpacings(std::vector<std::shared_ptr<LLayoutItemData> > &chain,
                                std::vector<std::shared_ptr<LLayoutItem> > &grid,
                                uint32_t fixedSpacing,
                                LOrientation orientation)
{
    int numRows = m_rowNum;       // or columns if orientation is horizontal
    int numColumns = m_colNum;    // or rows if orientation is horizontal

    if (orientation == LOrientation::Horizontal) {
        std::swap(numRows, numColumns);
    }

    for (int c = 0; c < numColumns; ++c) {
        std::shared_ptr<LLayoutItem> previousGd = nullptr;
        int previousRow = -1;       // previous *non-empty* row

        for (int r = 0; r < numRows; ++r) {
            if (chain.at(r)->empty){
                continue;
            }

            std::shared_ptr<LLayoutItem> gd = gridAt(grid, r, c, m_colNum, orientation);
            if (previousRow != -1 && (gd == nullptr || previousGd != gd)) {
                int spacing = fixedSpacing;

                if (orientation == LOrientation::Vertical) {
                    std::shared_ptr<LLayoutItem> sibling = m_vReversed ? previousGd : gd;
                    if (sibling) {
                        if (sibling->isEmpty()) {
                            spacing = 0;
                        } else {
                            std::shared_ptr<LWidget> wid = sibling->widget();
                            if (wid){
                                spacing = std::max<int>(spacing, sibling->geometry().m_y - wid->geometry().m_y);
                            }
                        }
                    }
                }

                if (spacing > chain.at(previousRow)->spacing){
                    chain[previousRow]->spacing = spacing;
                }
            }

            previousGd = gd;
            previousRow = r;
        }
    }
}

void LGridLayout::distribute(LRect<int> r, int hSpacing, int vSpacing)
{
    bool visualHReversed = m_hReversed;
    std::shared_ptr<LWidget> parent = parentWidget();
    if ((parent != nullptr)&& parent->isRightToLeft()){
        visualHReversed = !visualHReversed;
    }

    setupLayoutData(hSpacing, vSpacing);
    r.adjust(m_leftMargin, m_topMargin, -m_rightMargin, -m_bottomMargin);

    LLayoutItemData::geomCalc(m_colData, 0, m_colNum, r.m_x, r.width(), -1);
    std::vector<std::shared_ptr<LLayoutItemData> > *rDataPtr;
    if (m_hasHfw) {
        recalcHFW(r.width());
        LLayoutItemData::geomCalc(m_hfwData, 0, m_rowNum, r.m_y, r.height(), -1);
        rDataPtr = &m_hfwData;
    } else {
        LLayoutItemData::geomCalc(m_rowData, 0, m_rowNum, r.m_y, r.height(), -1);
        rDataPtr = &m_rowData;
    }
    std::vector<std::shared_ptr<LLayoutItemData> > &rData = *rDataPtr;
    int i;

    bool reverse = ((r.y2() > m_rect.y2()) || (r.y2() == m_rect.y2()
                                                     && ((r.x2() > m_rect.x2()) != visualHReversed)));

    int n = (int)m_positions.size();
    for (i = 0; i < n; ++i) {
        LLayoutPosition& pos = m_positions.at(reverse ? n-i-1 : i);
        std::shared_ptr<LLayoutItem>  item = pos.m_item;
        if(item != nullptr){
            std::shared_ptr<LGridItemData> box = std::dynamic_pointer_cast<LGridItemData>(pos.m_posData);
            if(box != nullptr){
                int r2 = box->toRow(m_rowNum);
                int c2 = box->toCol(m_colNum);

                int x = m_colData.at(box->col)->pos;
                int y = rData.at(box->row)->pos;
                int x2p = m_colData.at(c2)->pos + m_colData.at(c2)->size; // x2+1
                int y2p = rData.at(r2)->pos + rData.at(r2)->size;    // y2+1
                int w = x2p - x;
                int h = y2p - y;

                if (visualHReversed){
                    x = r.m_x + r.x2() - x - w + 1;
                }
                if (m_vReversed){
                    y = r.m_y + r.y2() - y - h + 1;
                }
                item->setGeometry(LRect<int>(x, y, w, h, false));
            }
        }
    }
}

void LGridLayout::recalcHFW(int w)
{
    if (m_hfwData.size() < m_rowNum){
        uint32_t r = (uint32_t)m_hfwData.size();
        m_hfwData.resize(m_rowNum);
        for (; r < m_hfwData.size(); r++) {
            m_hfwData[r] = std::make_shared<LLayoutItemData>();
        }
    }

    setupHfwLayoutData();

    int h = 0;
    int mh = 0;
    for (const std::shared_ptr<LLayoutItemData>& d: m_hfwData) {
        h += d->sizeHint + d->spacing;
        mh += d->minimumSize + d->spacing;
    }

    m_hfwWidth = w;
    m_hfwHeight = std::min<int>(LLAYOUTSIZE_MAX, h);
    m_hfwMinHeight = std::min<int>(LLAYOUTSIZE_MAX, mh);
}

void LGridLayout::setupHfwLayoutData()
{
    uint32_t i= 0;
    for (std::shared_ptr<LLayoutItemData> d: m_hfwData) {
        d->copy(m_rowData.at(i));
        d->minimumSize = d->sizeHint = m_rMinHeights.at(i);
        ++i;
    }

    for (int pass = 0; pass < 2; ++pass) {
        for (i = 0; i < m_positions.size(); ++i) {
            const LLayoutPosition& pos = m_positions.at(i);
            std::shared_ptr<LLayoutItem>  item = pos.m_item;
            if(item != nullptr){
                std::shared_ptr<LGridItemData> box = std::dynamic_pointer_cast<LGridItemData>(pos.m_posData);
                if(box != nullptr){

                    int r1 = box->row;
                    int c1 = box->col;
                    int r2 = box->toRow(m_rowNum);
                    int c2 = box->toCol(m_colNum);
                    int w = m_colData.at(c2)->pos + m_colData.at(c2)->size - m_colData.at(c1)->pos;

                    if (r1 == r2) {
                        if (pass == 0){
                            addHfwData(item, box->row, w);
                        }
                    } else {
                        if (pass == 0) {
                            initEmptyMultiBox(m_hfwData, r1, r2);
                        } else {
                            LSize<int> hint = item->sizeHint();
                            LSize<int> min = item->minimumSize();
                            if (item->hasHeightForWidth()) {
                                int hfwh = item->heightForWidth(w);
                                if (hfwh > hint.height()){
                                    hint.setHeight(hfwh);
                                }
                                if (hfwh > min.height()){
                                    min.setHeight(hfwh);
                                }
                            }
                            distributeMultiBox(m_hfwData, r1, r2, min.height(), hint.height(),
                                               m_rStretch, box->vStretch(item));
                        }
                    }
                }
            }
        }
    }
    for (std::shared_ptr<LLayoutItemData>& d: m_hfwData) {
        d->expansive = d->expansive || d->stretch > 0;
    }
}

void LGridLayout::addHfwData(std::shared_ptr<LLayoutItem> item,
                             int row,
                             int width)
{
    std::shared_ptr<LLayoutItemData> layData = m_hfwData.at(row);
    if (item->hasHeightForWidth()) {
        int hint = item->heightForWidth(width);
        layData->sizeHint = std::max<int>(hint, layData->sizeHint);
        layData->minimumSize = std::max<int>(hint, layData->minimumSize);
    } else {
        LSize<int> hint = item->sizeHint();
        LSize<int> minS = item->minimumSize();
        layData->sizeHint = std::max<int>(hint.height(), layData->sizeHint);
        layData->minimumSize = std::max<int>(minS.height(), layData->minimumSize);
    }
}

void LGridLayout::buildGridRowCol(uint32_t &rowNum, uint32_t &colNum) const
{
    uint32_t maxRowId = 0;
    uint32_t maxColId = 0;

    for (const LLayoutPosition& pos : m_positions) {
        std::shared_ptr<LGridItemData> box = std::dynamic_pointer_cast<LGridItemData>(pos.m_posData);
        if(box != nullptr){
            maxRowId = std::max<int>(std::max<int>(box->row, box->torow), maxRowId);
            maxColId = std::max<int>(std::max<int>(box->col, box->tocol), maxColId);
        }
    }
    rowNum = maxRowId+1;
    colNum = maxColId+1;
}

LK_NAMESPACE_END

