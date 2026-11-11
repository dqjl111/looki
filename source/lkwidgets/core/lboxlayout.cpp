#include "lboxlayout.h"
#include "lkwidgets/core/lwidget.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

static inline bool horz(LBoxLayout::Direction dir)
{
    return dir == LBoxLayout::RightToLeft || dir == LBoxLayout::LeftToRight;
}


LBoxLayout::LBoxLayout(Direction dir) : m_dir(dir)
{
    m_dirty = true;
}

LSize<int> LBoxLayout::sizeHint() const
{
    if(m_dirty){
        const_cast<LBoxLayout*>(this)->setupGeom();
    }
    return m_sizeHint;
}

LSize<int> LBoxLayout::minimumSize() const
{
    if(m_dirty){
        const_cast<LBoxLayout*>(this)->setupGeom();
    }
    return m_minSize;
}

LSize<int> LBoxLayout::maximumSize() const
{
    if(m_dirty){
        const_cast<LBoxLayout*>(this)->setupGeom();
    }
    LSize<int> s = m_maxSize.boundedTo(LSize<int>(LLAYOUTSIZE_MAX, LLAYOUTSIZE_MAX));

    if (m_alignment & LAlignment::HorizontalMask){
        s.setWidth(LLAYOUTSIZE_MAX);
    }
    if (m_alignment & LAlignment::VerticalMask){
        s.setHeight(LLAYOUTSIZE_MAX);
    }
    return s;
}

LFlags<LOrientation> LBoxLayout::expandingDirections() const
{
    if (m_dirty){
        const_cast<LBoxLayout*>(this)->setupGeom();
    }
    return m_expanding;
}

void LBoxLayout::setGeometry(const LRect<int> &r)
{
    if (m_dirty || r != geometry()) {
        LRect<int> oldRect = geometry();
        LLayout::setGeometry(r);
        if (m_dirty){
            setupGeom();
        }
        LRect<int> cr = m_alignment ? alignmentRect(r) : r;
        LRect<int> s(cr.m_x + m_leftMargin, cr.m_y + m_topMargin,
                     LSize<int>(cr.m_width - (m_leftMargin + m_rightMargin),
                                cr.m_height - (m_topMargin + m_bottomMargin)));

        bool isHorz = horz(m_dir);
        int pos = isHorz ? s.m_x : s.m_y;
        int space = isHorz ? s.width() : s.height();

        if (m_hasHfw && (isHorz == false)) {
            for (LLayoutPosition& pos : m_positions) {
                std::shared_ptr<LLayoutItem> item = pos.m_item;
                if(item != nullptr){
                    std::shared_ptr<LLayoutItemData> box = std::dynamic_pointer_cast<LLayoutItemData>(pos.m_posData);
                    if(box != nullptr){
                        if (pos.m_item->hasHeightForWidth()) {
                            int width = lBound(pos.m_item->minimumSize().width(), s.width(), pos.m_item->maximumSize().width());
                            box->sizeHint = box->minimumSize = item->heightForWidth(width);
                        }
                    }
                }
            }
        }

        Direction visualDir = m_dir;
        std::shared_ptr<LWidget> parent = parentWidget();
        if (parent!= nullptr){
            if(parent->isRightToLeft()) {
                if (m_dir == LeftToRight){
                    visualDir = RightToLeft;
                }
                else if (m_dir == RightToLeft){
                    visualDir = LeftToRight;
                }
            }
        }

        std::vector<std::shared_ptr<LLayoutItemData>> datas;
        fetchValidItemDatas(datas);
        LLayoutItemData::geomCalc(datas, 0, datas.size(), pos, space);

        bool reverse = (horz(visualDir)
                            ? ((r.x2() > oldRect.x2()) != (visualDir == RightToLeft))
                            : r.y2() > oldRect.y2());

        int n = m_positions.size();
        for (int j = 0; j < n; j++) {
            int i = reverse ? n-j-1 : j;
            LLayoutPosition& pos = m_positions.at(i);
            std::shared_ptr<LLayoutItem> item = pos.m_item;
            if(item != nullptr){
                std::shared_ptr<LLayoutItemData> box = std::dynamic_pointer_cast<LLayoutItemData>(pos.m_posData);
                if(box != nullptr){
                    switch (visualDir) {
                    case LeftToRight:{
                        item->setGeometry(LRect<int>(box->pos, s.m_y, box->size, s.height(), false));
                        break;
                    }
                    case RightToLeft:{
                        item->setGeometry(LRect<int>(s.m_x + s.x2() - box->pos - box->size + 1,
                                                     s.m_y,
                                                     box->size,
                                                     s.height(),
                                                     false));
                        break;
                    }
                    case TopToBottom:{
                        item->setGeometry(LRect<int>(s.m_x, box->pos, s.width(), box->size, false));
                        break;
                    }
                    case BottomToTop:{
                        item->setGeometry(LRect<int>(s.m_x,
                                                     s.m_y + s.y2() - box->pos - box->size + 1,
                                                     s.width(),
                                                     box->size,
                                                     false));
                    }
                    default:{
                        break;
                    }
                    }
                }
            }else{
                lWarn() << "Unexpect box layout item without data to set geometry!";
            }
        }
    }
}


bool LBoxLayout::hasHeightForWidth() const
{
    if (m_dirty){
        const_cast<LBoxLayout*>(this)->setupGeom();
    }
    return m_hasHfw;
}

int LBoxLayout::heightForWidth(int w) const
{
    if (!hasHeightForWidth()){
        return -1;
    }

    w -= m_leftMargin + m_rightMargin;
    if (w != m_hfwWidth){
        const_cast<LBoxLayout*>(this)->calcHfw(w);
    }

    return m_hfwHeight + m_topMargin + m_bottomMargin;
}

int LBoxLayout::minimumHeightForWidth(int w) const
{
    if (!hasHeightForWidth()){
        return -1;
    }

    w -= m_leftMargin + m_rightMargin;
    if (w != m_hfwWidth){
        const_cast<LBoxLayout*>(this)->calcHfw(w);
    }
    return m_hfwMinHeight + m_topMargin + m_bottomMargin;
}

void LBoxLayout::invalidate()
{
    setDirty();
    LLayout::invalidate();
}

void LBoxLayout::setSpacing(uint32_t spacing)
{
    m_spacing = spacing;
    invalidate();
}

void LBoxLayout::setupGeom()
{
    if (!m_dirty){
        return;
    }

    int maxw = horz(m_dir) ? 0 : LLAYOUTSIZE_MAX;
    int maxh = horz(m_dir) ? LLAYOUTSIZE_MAX : 0;
    int minw = 0;
    int minh = 0;
    int hintw = 0;
    int hinth = 0;

    bool horexp = false;
    bool verexp = false;

    m_hasHfw = false;
    uint32_t fixedSpacing = spacing();
    std::shared_ptr<LLayoutItemData> previousBox;

    for (LLayoutPosition& pos : m_positions) {
        std::shared_ptr<LLayoutItem> item  = pos.m_item;
        if(item == nullptr){
            continue;
        }

        std::shared_ptr<LLayoutItemData> box = std::dynamic_pointer_cast<LLayoutItemData>(pos.m_posData);
        if(box == nullptr){
            box = std::make_shared<LLayoutItemData>();
            pos.m_posData = box;
        }
        LSize<int> max = item->maximumSize();
        LSize<int> min = item->minimumSize();
        LSize<int> hint = item->sizeHint();
        LFlags<LOrientation>  exp = item->expandingDirections();
        bool empty = item->isEmpty();
        uint32_t tSpacing = 0;

        if (!empty) {
            tSpacing = (previousBox != nullptr) ? fixedSpacing : 0;

            if (previousBox != nullptr){
                previousBox->spacing = tSpacing;
            }
            previousBox = box;
        }

        bool ignore = empty && item->widget(); // ignore hidden widgets
        bool dummy = true;
        if (horz(m_dir)) {
            bool expand = (exp & LOrientation::Horizontal || box->stretch > 0);
            horexp = horexp || expand;
            maxw += tSpacing + max.width();
            minw += tSpacing + min.width();
            hintw += tSpacing + hint.width();
            if (!ignore){
                maxExpCalc(maxh, verexp, dummy,
                           max.height(), exp & LOrientation::Vertical, item->isEmpty());
            }
            minh = std::max(minh, min.height());
            hinth = std::max(hinth, hint.height());

            box->sizeHint = hint.width();
            box->maximumSize = max.width();
            box->minimumSize = min.width();
            box->expansive = expand;
            if(box->stretch == 0){
                box->stretch= box->hStretch(item);
            }
        } else {
            bool expand = (exp & LOrientation::Vertical || box->stretch > 0);
            verexp = verexp || expand;
            maxh += tSpacing + max.height();
            minh += tSpacing + min.height();
            hinth += tSpacing + hint.height();
            if (!ignore){
                maxExpCalc(maxw, horexp, dummy,
                           max.width(), exp & LOrientation::Horizontal, item->isEmpty());
            }
            minw = std::max(minw, min.width());
            hintw = std::max(hintw, hint.width());

            box->sizeHint = hint.height();
            box->maximumSize = max.height();
            box->minimumSize = min.height();
            box->expansive = expand;
            if(box->stretch == 0){
                box->stretch = box->vStretch(item);
            }
        }

        box->empty = empty;
        box->spacing = 0;   // might be initialized with a non-zero value in a later iteration
        m_hasHfw = m_hasHfw || item->hasHeightForWidth();
    }

    m_expanding = (horexp ? (uint8_t)LOrientation::Horizontal : 0);
    m_expanding |= (verexp ? (uint8_t)LOrientation::Vertical : 0);

    m_minSize = LSize<int>(minw, minh);
    m_maxSize = LSize<int>(maxw, maxh).expandedTo(m_minSize);
    m_sizeHint = LSize<int>(hintw, hinth).expandedTo(m_minSize).boundedTo(m_maxSize);

    LSize<int> extra(m_leftMargin + m_rightMargin, m_topMargin + m_bottomMargin);

    m_minSize += extra;
    m_maxSize += extra;
    m_sizeHint += extra;

    m_dirty = false;

}

void LBoxLayout::setDirty()
{
    for (LLayoutPosition& pos : m_positions) {
        std::shared_ptr<LLayoutItemData> box = std::dynamic_pointer_cast<LLayoutItemData>(pos.m_posData);
        if(box != nullptr){
            box->stretch = 0;
        }
    }
    m_dirty = true;
}

void LBoxLayout::calcHfw(int w)
{
    int h = 0;
    int mh = 0;
    if (horz(m_dir)) {
        calcGeomForWidth(w);
        for (LLayoutPosition& pos : m_positions) {
            std::shared_ptr<LLayoutItem> item  = pos.m_item;
            if(item != nullptr){
                std::shared_ptr<LLayoutItemData> box = std::dynamic_pointer_cast<LLayoutItemData>(pos.m_posData);
                if(box != nullptr){
                    h = std::max(h, item->hfw(box->size));
                    mh = std::max(mh, item->mhfw(box->size));
                }else{
                    h = std::max(h, item->hfw(0));
                    mh = std::max(mh, item->mhfw(0));
                }
            }
        }
    }else{
        for (LLayoutPosition& pos : m_positions) {
            std::shared_ptr<LLayoutItem> item  = pos.m_item;
            if(item != nullptr){
                h +=  item->hfw(w);
                mh += item->mhfw(w);

                std::shared_ptr<LLayoutItemData> box = std::dynamic_pointer_cast<LLayoutItemData>(pos.m_posData);
                if(box != nullptr){
                    h +=  box->spacing;
                    mh += box->spacing;
                }
            }
        }
    }

    m_hfwWidth = w;
    m_hfwHeight = h;
    m_hfwMinHeight = mh;
}

void LBoxLayout::calcGeomForWidth(int w)
{
    std::vector<std::shared_ptr<LLayoutItemData>> datas;
    fetchValidItemDatas(datas);
    LLayoutItemData::geomCalc(datas, 0, datas.size(), 0, w);
}

void LBoxLayout::fetchValidItemDatas(std::vector<std::shared_ptr<LLayoutItemData>>& datas)
{
    for (LLayoutPosition& pos : m_positions) {
        if(pos.m_item != nullptr){
            std::shared_ptr<LLayoutItemData> box = std::dynamic_pointer_cast<LLayoutItemData>(pos.m_posData);
            if(box != nullptr){
                datas.push_back(box);
            }
        }
    }
}


LHBoxLayout::LHBoxLayout(): LBoxLayout(LeftToRight)
{

}

LVBoxLayout::LVBoxLayout(): LBoxLayout(TopToBottom)
{

}

LK_NAMESPACE_END



