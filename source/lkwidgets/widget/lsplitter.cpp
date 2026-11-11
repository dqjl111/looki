#include "lsplitter.h"
#include "lkwidgets/draw/lstyle.h"
LK_NAMESPACE_BEGIN

LSplitterLayoutItem::LSplitterLayoutItem(std::shared_ptr<LWidget> w) : m_widget(w), m_collapsible(2)
{
}

int LSplitterLayoutItem::getWidgetSize(LOrientation orient) const
{
    if (m_sizer == -1) {
        std::shared_ptr<LWidget> w = m_widget;
        if(w != nullptr){
            LSize<int> s = w->sizeHint();
            const int presizer = pick(s, orient);
            const int realsize = pick(w->size(), orient);
            int tSizer = 0;
            if (!s.isValid() || (w->testAttribute(LWidgetAttribute::Resized) && (realsize > presizer))) {
                tSizer = pick(w->size(), orient);
            } else {
                tSizer = presizer;
            }
            LSizePolicy p = w->sizePolicy();
            int sf = (orient == LOrientation::Horizontal ? p.m_horStretch : p.m_verStretch);
            if (sf > 1){
                tSizer *= sf;
            }
            const_cast<LSplitterLayoutItem*>(this)->m_sizer = tSizer;
        }
    }
    return m_sizer;
}

int LSplitterLayoutItem::getHandleSize(LOrientation orient) const
{
    if(m_handle != nullptr){
        return pick(m_handle->sizeHint(), orient);
    }
    return 0;
}


LSplitterHandle::LSplitterHandle(std::shared_ptr<LSplitter> parent): LWidget(parent)
{
    if(parent != nullptr){
        m_orient = parent->orientation();
    }
}

void LSplitterHandle::setOrientation(LOrientation o)
{
    m_orient = o;
    setCursor(m_orient == LOrientation::Horizontal ? LCursorShape::SizeHor : LCursorShape::SizeVer);
}

LOrientation LSplitterHandle::orientation() const
{
    return m_orient;
}

bool LSplitterHandle::opaqueResize() const
{
    std::shared_ptr<LSplitter> s = parentT<LSplitter>();
    if(s != nullptr){
        return s->opaqueResize();
    }
    return false;
}

std::shared_ptr<LSplitter> LSplitterHandle::splitter() const
{
    return parentT<LSplitter>();
}

LSize<int> LSplitterHandle::sizeHint() const
{
    std::shared_ptr<LSplitter> s = splitter();
    if(s != nullptr){
        int hw = s->handleWidth();
        return LSize<int>(hw, hw);
    }
    return LSize<int>();
}

bool LSplitterHandle::handleEvent(std::shared_ptr<LEvent> e)
{
    switch(e->type()) {
    case LEvent::HoverEnter:
        m_hover = true;
        update();
        break;
    case LEvent::HoverLeave:
        m_hover = false;
        update();
        break;
    default:
        break;
    }
    return LWidget::handleEvent(e);
}

void LSplitterHandle::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    uint16_t state = (uint16_t)LUiState::None;
    if (isEnabled()){
        if (m_hover){
            state = (uint16_t)LUiState::Hover;
        }else if (m_pressed){
            state = (uint16_t)LUiState::Sunken;
        }else{
            state = (uint16_t)LUiState::Enabled;
        }
    }else{
        state = (uint16_t)LUiState::Disabled;
    }

    if(m_orient == LOrientation::Horizontal){
        state |= (uint16_t)LUiState::Horizontal;
    }

    LPainter painter(paintDevice());
    std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(LUiElementType::None, state);
    if(rule !=nullptr){
        if(rule->hasDrawable()){
            rule->drawRule(&painter, rect());
            return;
        }
    }
    painter.eraseRect(rectF());
}

void LSplitterHandle::mouseMoveEvent(std::shared_ptr<LMouseEvent> e)
{
    if (!m_pressed){
        return;
    }

    std::shared_ptr<LSplitter> p = parentT<LSplitter>();
    if(p != nullptr){
        const int pos = pick(p->mapFromGlobal(e->globalPos())) - m_mouseOffset;
        if (opaqueResize()) {
            moveSplitter(pos);
        }
    }
}

void LSplitterHandle::mousePressEvent(std::shared_ptr<LMouseEvent> e)
{
    if (e->button() == LMouseButton::Left) {
        m_mouseOffset = pick(e->pos());
        m_pressed = true;
        update();
    }
}

void LSplitterHandle::mouseReleaseEvent(std::shared_ptr<LMouseEvent> e)
{
    if (!m_pressed){
        return;
    }

    if (!opaqueResize()) {
        std::shared_ptr<LSplitter> p = parentT<LSplitter>();
        if(p != nullptr){
            const int pos = pick(p->mapFromGlobal(e->globalPos())) - m_mouseOffset;
            moveSplitter(pos);
        }
    }
    m_pressed = false;
    update();
}

void LSplitterHandle::resizeEvent(std::shared_ptr<LResizeEvent> e)
{
    std::shared_ptr<LSplitter> p = parentT<LSplitter>();
    if(p != nullptr){
        // Ensure the actual grab area is at least 4 or 5 pixels
        const int handleMargin = (5 - p->handleWidth()) / 2;
        const bool useTinyMode = handleMargin > 0;
        setAttribute(LWidgetAttribute::MouseNoMask, useTinyMode);
        if (useTinyMode) {
            if (orientation() == LOrientation::Horizontal){
                setContentsMargins(handleMargin, 0, handleMargin, 0);
            }else{
                setContentsMargins(0, handleMargin, 0, handleMargin);
            }
            std::shared_ptr<LRegion> r= std::make_shared<LRegion>(contentsRect());
            setMask(r);
        } else {
            setContentsMargins(0, 0, 0, 0);
            clearMask();
        }
    }
    LWidget::resizeEvent(e);
}

void LSplitterHandle::initSpecific()
{
   setCursor(m_orient == LOrientation::Horizontal ? LCursorShape::SizeHor : LCursorShape::SizeVer);
}

void LSplitterHandle::moveSplitter(int pos)
{
    std::shared_ptr<LSplitter> p = parentT<LSplitter>();
    if(p != nullptr){
        if (p->isRightToLeft() && m_orient == LOrientation::Horizontal){
            pos = p->contentsRect().width() - pos;
        }
        std::shared_ptr<LSplitterHandle> h = getPtrT<LSplitterHandle>();
        p->moveSplitter(pos, p->indexOfItem(h));
    }
}

int LSplitterHandle::closestLegalPosition(int pos)
{
    std::shared_ptr<LSplitter> p = parentT<LSplitter>();
    if(p != nullptr){
        std::shared_ptr<LSplitterHandle> h = getPtrT<LSplitterHandle>();
        if (p->isRightToLeft() && m_orient == LOrientation::Horizontal) {
            int w = p->contentsRect().width();
            return w - p->closestLegalPosition(w - pos, p->indexOfItem(h));
        }
        return p->closestLegalPosition(pos, p->indexOfItem(h));
    }
    return -1;
}


LSplitter::LSplitter(LOrientation dir, std::shared_ptr<LWidget> parent): LFrame(parent), m_orient(dir){
    if(m_orient == LOrientation::Horizontal){
        m_sizePolicy = LSizePolicy(LSizePolicy::Expanding, LSizePolicy::Preferred, LSizePolicy::DefaultType);
    }else{
        m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Expanding, LSizePolicy::DefaultType);
    }
    m_opaque = true;
    m_childrenCollapsible = true;
    m_firstShow = true;
}


void LSplitter::initSpecific()
{
    setAttribute(LWidgetAttribute::WStateOwnSizePolicy, false);
}

void LSplitter::setOrientation(LOrientation o)
{
    if (m_orient == o)
        return;

    if (!testAttribute(LWidgetAttribute::WStateOwnSizePolicy)) {
        setSizePolicy(sizePolicy().transposed());
        setAttribute(LWidgetAttribute::WStateOwnSizePolicy, false);
    }

    m_orient = o;
    for (const LSplitterLayoutItem& item : m_items) {
        if(item.m_handle != nullptr){
            item.m_handle->setOrientation(o);
        }
    }
    recalc(isVisible());
}

LOrientation LSplitter::orientation() const
{
    return m_orient;
}

void LSplitter::setCollapsible(int index, bool collapse)
{
    if (L_UNLIKELY(index < 0 || index >= m_items.size())) {
        return;
    }
    m_items.at(index).m_collapsible = collapse ? 1 : 0;
}

bool LSplitter::isCollapsible(int index) const
{
    if (L_UNLIKELY(index < 0 || index >= m_items.size())) {
        return false;
    }
    return m_items.at(index).m_collapsible;
}

void LSplitter::setOpaqueResize(bool opaque)
{
    m_opaqueResizeSet = true;
    m_opaque = opaque;
}

bool LSplitter::opaqueResize() const
{
    return m_opaqueResizeSet ? m_opaque : true;
}

int LSplitter::handleWidth() const
{
    return m_handleWidth;
}

void LSplitter::setHandleWidth(int width)
{
    m_handleWidth = width;
    recalc(isVisible());
}

LSize<int> LSplitter::sizeHint() const
{
    int l = 0;
    int t = 0;
    for (const LSplitterLayoutItem& item : m_items) {
        std::shared_ptr<LWidget> w = item.m_widget;
        if(w != nullptr){
            if (w->isHidden()){
                continue;
            }
            LSize<int> s = w->sizeHint();
            if (s.isValid()) {
                l += pick(s);
                t = std::max<int>(t, trans(s));
            }
        }
    }
    return orientation() == LOrientation::Horizontal ? LSize<int>(l, t) : LSize<int>(t, l);
}

LSize<int> LSplitter::minimumSizeHint() const
{
    int l = 0;
    int t = 0;

    for (const LSplitterLayoutItem& item : m_items) {
        std::shared_ptr<LWidget> w = item.m_widget;
        if(w != nullptr){
            if (w->isHidden()){
                continue;
            }
            LSize<int> widgetSize = LLayoutItem::smartMinSize(w);
            if (widgetSize.isValid()) {
                l += pick(widgetSize);
                t = std::max<int>(t, trans(widgetSize));
            }

            if(item.m_handle != nullptr){
                if (item.m_handle->isHidden()){
                    continue;
                }

                LSize<int> splitterSize = item.m_handle->sizeHint();
                if (splitterSize.isValid()) {
                    l += pick(splitterSize);
                    t = std::max<int>(t, trans(splitterSize));
                }
            }
        }
    }
    return orientation() == LOrientation::Horizontal ? LSize<int>(l, t) : LSize<int>(t, l);
}

std::list<int> LSplitter::sizes() const
{
    std::list<int> ret;
    for (const LSplitterLayoutItem& item : m_items) {
        ret.push_back(pick(item.m_rect.size()));
    }
    return ret;
}

void LSplitter::setSizes(const std::list<int> &ss)
{
    setSizesHelper(ss, true);
}

int LSplitter::removeWidget(std::shared_ptr<LWidget> child, bool update)
{
    if(child != nullptr){
        //不允许重复只会有一个
        int iPos = 0;
        for (LSplitterLayoutItem& pos : m_items) {
            if(pos.m_widget == child){
                pos.m_widget = nullptr;
                removeChild(child);
                if(update){
                    clearInvalidItem(false);
                    LFrame::update();
                }
                return iPos;
            }
            iPos++;
        }
    }
    return -1;
}

std::shared_ptr<LWidget> LSplitter::removeWidget(uint32_t pos, bool update)
{
    if(pos <m_items.size()){
        std::shared_ptr<LWidget> child = m_items[pos].m_widget;
        if(child != nullptr){
            m_items[pos].m_widget = nullptr;
            removeChild(child);
            if(update){
                clearInvalidItem(false);
                LFrame::update();
            }
        }
        return child;
    }
    return nullptr;
}

int LSplitter::addWidget(std::shared_ptr<LWidget> child, int pos, bool update)
{
    if(child == nullptr || child.get() == this){
        return -1;
    }

    int i=0;
    bool hasRemove = false;
    for (const LSplitterLayoutItem& item : m_items) {
        if(item.m_widget == child){
            if(i == pos){
                return pos;
            }else{
                hasRemove = true;
                removeWidget(i, false);
                break;
            }
        }
        ++i;
    }

    std::shared_ptr<LSplitter> tThis = getPtrT<LSplitter>();
    child->setParent(tThis);
    pos = addWidgetHelper(child, pos);
    if(update){
        if(hasRemove){
            clearInvalidItem(false);
        }
        LFrame::update();
    }
    return pos;
}

void LSplitter::clearInvalidItem(bool update)
{
    std::vector<LSplitterLayoutItem> t;
    for (const LSplitterLayoutItem& pos : m_items) {
        if(pos.m_widget == nullptr){
            t.push_back(pos);
        }
    }

    if(t.size() != m_items.size()){
        m_items.swap(t);
        if(update){
            LFrame::update();
        }
    }
}

int LSplitter::indexOfItem(std::shared_ptr<LWidget> child) const
{
    int i=0;
    for (const LSplitterLayoutItem& item : m_items) {
        if(item.m_widget == child || item.m_handle == child){
            return i;
        }
        ++i;
    }
    return -1;
}

bool LSplitter::handleEvent(std::shared_ptr<LEvent> e)
{
    switch (e->type()) {
    case LEvent::Hide:{
        // Reset firstShow to false here since things can be done to the splitter in between
        if (!m_firstShow){
            m_firstShow = true;
        }
        break;
    }
    case LEvent::Show:{
        if (!m_firstShow){
            break;
        }
        m_firstShow = false;
        _FALLTHROUGH;
    }
    case LEvent::HideToParent:
    case LEvent::ShowToParent:
    case LEvent::LayoutRequest:{
        recalc(isVisible());
        break;
    }
    default:{
        break;
    }
    }
    return LFrame::handleEvent(e);
}

void LSplitter::resizeEvent(std::shared_ptr<LResizeEvent> e)
{
    doResize();
}


void LSplitter::moveSplitter(int pos, int index)
{
    if(index<0 || index>= m_items.size()){
        return;
    }

    const LSplitterLayoutItem& item = m_items[index];
    int farMin = 0;
    int min = 0;
    int max = 0;
    int farMax = 0;


    pos = adjustPos(pos, index, &farMin, &min, &max, &farMax);
    int oldP = pick(item.m_rect.topLeft());


    std::vector<int> poss(m_items.size(), 0);
    std::vector<int> ws(m_items.size(), 0);
    bool upLeft;

    doMove(false, pos, index, +1, (collapsible(item) && (pos > max)), poss.data(), ws.data());
    doMove(true, pos, index - 1, +1, (collapsible(index - 1) && (pos < min)), poss.data(), ws.data());
    upLeft = (pos < oldP);

    int wid, delta, count = (int)m_items.size();
    if (upLeft) {
        wid = 0;
        delta = 1;
    } else {
        wid = count - 1;
        delta = -1;
    }
    for (; wid >= 0 && wid < count; wid += delta) {
        LSplitterLayoutItem& iItem = m_items[wid];
        if(iItem.m_widget != nullptr){
            if (!iItem.m_widget->isHidden()){
                lWarn() << "Move:" << wid << ", pos:" << poss[wid] << ", width:" << ws[wid];
                setGeo(iItem, poss[wid], ws[wid], true);
            }
        }
    }
}

int LSplitter::closestLegalPosition(int pos, int index)
{
    int x = 0;
    int i = 0;
    int n = 0;
    int u = 0;
    return adjustPos(pos, index, &u, &n, &i, &x);
}


void LSplitter::getRange(int index, int *farMin, int *min, int *max, int *farMax) const
{
    int n = (int)m_items.size();
    if (index <= 0 || index >= n){
        return;
    }

    int collapsibleSizeBefore = 0;
    int idJustBefore = findWidgetJustBeforeOrJustAfter(index, -1, collapsibleSizeBefore);

    int collapsibleSizeAfter = 0;
    int idJustAfter = findWidgetJustBeforeOrJustAfter(index, +1, collapsibleSizeAfter);

    int minBefore = 0;
    int minAfter = 0;
    int maxBefore = 0;
    int maxAfter = 0;
    int i;

    for (i = 0; i < index; ++i){
        addContribution(i, &minBefore, &maxBefore, i == idJustBefore);
    }
    for (i = index; i < n; ++i){
        addContribution(i, &minAfter, &maxAfter, i == idJustAfter);
    }

    LRect<int> r = contentsRect();
    int farMinVal;
    int minVal;
    int maxVal;
    int farMaxVal;

    int smartMinBefore = std::max(minBefore, pick(r.size()) - maxAfter);
    int smartMaxBefore = std::min(maxBefore, pick(r.size()) - minAfter);

    minVal = pick(r.topLeft()) + smartMinBefore;
    maxVal = pick(r.topLeft()) + smartMaxBefore;

    farMinVal = minVal;
    if (minBefore - collapsibleSizeBefore >= pick(r.size()) - maxAfter){
        farMinVal -= collapsibleSizeBefore;
    }
    farMaxVal = maxVal;
    if (pick(r.size()) - (minAfter - collapsibleSizeAfter) <= maxBefore){
        farMaxVal += collapsibleSizeAfter;
    }

    if (farMin){
        *farMin = farMinVal;
    }
    if (min){
        *min = minVal;
    }
    if (max){
        *max = maxVal;
    }
    if (farMax){
        *farMax = farMaxVal;
    }
}

int LSplitter::adjustPos(int pos, int index, int *farMin, int *min, int *max, int *farMax) const
{
    const int Threshold = 40;

    getRange(index, farMin, min, max, farMax);

    if (pos >= *min) {
        if (pos <= *max) {
            return pos;
        } else {
            int delta = pos - *max;
            int width = *farMax - *max;

            if (delta > width / 2 && delta >= std::min<int>(Threshold, width)) {
                return *farMax;
            } else {
                return *max;
            }
        }
    } else {
        int delta = *min - pos;
        int width = *min - *farMin;

        if (delta > width / 2 && delta >= std::min<int>(Threshold, width)) {
            return *farMin;
        } else {
            return *min;
        }
    }
}

void LSplitter::addContribution(int index, int *min, int *max, bool mayCollapse) const
{
    if(index >=0 && index < m_items.size()){
        const LSplitterLayoutItem& item = m_items[index];
        if(item.m_widget != nullptr){
            if (!item.m_widget->isHidden()) {
                if(item.m_handle != nullptr){
                    if (!item.m_handle->isHidden()) {
                        *min += item.getHandleSize(m_orient);
                        *max += item.getHandleSize(m_orient);
                    }
                }

                if (mayCollapse || !item.m_collapsed){
                    *min += pick(LLayoutItem::smartMinSize(item.m_widget));
                }
                *max += pick(LLayoutItem::smartMaxSize(item.m_widget));
            }
        }
    }
}

int LSplitter::findWidgetJustBeforeOrJustAfter(int index, int delta, int &collapsibleSize) const
{
    if (delta < 0){
        index += delta;
    }

    while(index >=0 && index < m_items.size()){
        std::shared_ptr<LWidget> w = m_items[index].m_widget;
        if (!w->isHidden()) {
            if (collapsible(m_items[index])){
                collapsibleSize = pick(LLayoutItem::smartMinSize(w));
            }
            return index;
        }
        index += delta;
    }
    return -1;
}

void LSplitter::doMove(bool backwards, int hPos, int index, int delta, bool mayCollapse, int *positions, int *widths)
{
    if (index < 0 || index >= m_items.size()){
        return;
    }


    const LSplitterLayoutItem& item = m_items[index];
    std::shared_ptr<LWidget> w = item.m_widget;

    int nextId = backwards ? index - delta : index + delta;

    if (w == nullptr  || w->isHidden()) {
        doMove(backwards, hPos, nextId, delta, collapsible(nextId), positions, widths);
    } else {

        int hs = 0;
        if(item.m_handle != nullptr){
            if(!item.m_handle->isHidden()){
                hs = item.getHandleSize(m_orient);
            }
        }

        int  ws = backwards ? hPos - pick(item.m_rect.topLeft()) : pick(item.m_rect.bottomRight()) - hPos -hs;
        if (ws > 0 || (!item.m_collapsed && !mayCollapse)) {
            ws = std::min<int>(ws, pick(LLayoutItem::smartMaxSize(w)));
            ws = std::max<int>(ws, pick(LLayoutItem::smartMinSize(w)));
        } else {
            ws = 0;
        }
        positions[index] = backwards ? hPos - ws : hPos + hs;
        widths[index] = ws;
        doMove(backwards, backwards ? hPos - ws - hs : hPos + hs + ws, nextId, delta,
               collapsible(nextId), positions, widths);
    }
}

void LSplitter::recalc(bool update)
{
    /*
      Splitter handles before the first visible widget or right
      before a hidden widget must be hidden.
    */
    bool first = true;
    bool allInvisible = !m_items.empty();
    for (const LSplitterLayoutItem& item : m_items) {
        std::shared_ptr<LWidget> w = item.m_widget;
        bool widgetHidden = true;
        if(w != nullptr){
            widgetHidden = w->isHidden();
        }
        if (allInvisible && !widgetHidden && !item.m_collapsed){
            allInvisible = false;
        }
        if(item.m_handle != nullptr){
            item.m_handle->setHidden(first || widgetHidden);
        }
        if (!widgetHidden){
            first = false;
        }
    }

    if (allInvisible)
        for (LSplitterLayoutItem& item : m_items) {
            std::shared_ptr<LWidget> w = item.m_widget;
            if(w != nullptr){
                if (!w->isHidden()) {
                    item.m_collapsed = false;
                    break;
                }
            }
        }

    int fi = 2 * frameWidth();
    int maxl = fi;
    int minl = fi;
    int maxt = LWIDGETSIZE_MAX;
    int mint = fi;
    /*
      calculate min/max sizes for the whole splitter
    */
    bool empty = true;
    for (const LSplitterLayoutItem& item : m_items) {
        std::shared_ptr<LWidget> w = item.m_widget;
        if(w != nullptr){
            if (!w->isHidden()) {
                empty = false;
                if (!w->isHidden()) {
                    minl += item.getHandleSize(m_orient);
                    maxl += item.getHandleSize(m_orient);
                }

                LSize<int> minS = LLayoutItem::smartMinSize(w);
                minl += pick(minS);
                maxl += pick(LLayoutItem::smartMaxSize(w));
                mint = std::max(mint, trans(minS));
                int tm = trans(LLayoutItem::smartMaxSize(w));
                if (tm > 0)
                    maxt = std::min<int>(maxt, tm);
            }
        }
    }

    if (empty) {
        std::shared_ptr<LSplitter> s = parentT<LSplitter>();
        if (s != nullptr) {
            // nested splitters; be nice
            maxl = maxt = 0;
        } else {
            maxl = LWIDGETSIZE_MAX;
        }
    } else {
        maxl = std::min<int>(maxl, LWIDGETSIZE_MAX);
    }
    if (maxt < mint){
        maxt = mint;
    }

    if (update) {
        if (m_orient == LOrientation::Horizontal) {
            setMaximumSize(maxl, maxt);
            if (isWindow()){
                setMinimumSize(minl,mint);
            }
        } else {
            setMaximumSize(maxt, maxl);
            if (isWindow()){
                setMinimumSize(mint,minl);
            }
        }
        doResize();
        updateGeometry();
    } else {
        m_firstShow = true;
    }
}

void LSplitter::doResize()
{
    LRect<int> r = contentsRect();
    std::vector<std::shared_ptr<LLayoutItemData> > a(m_items.size() * 2);

    bool noStretchFactorsSet = true;
    for (const LSplitterLayoutItem& item : m_items) {
        std::shared_ptr<LWidget> w = item.m_widget;
        if(w != nullptr){
            LSizePolicy p = w->sizePolicy();
            int sf = m_orient == LOrientation::Horizontal ? p.m_horStretch : p.m_verStretch;
            if (sf != 0) {
                noStretchFactorsSet = false;
                break;
            }
        }
    }

    int j=0;
    for (const LSplitterLayoutItem& item : m_items) {
        a[j] = std::make_shared<LLayoutItemData>();
        a[j]->init();
        if(item.m_handle != nullptr){
            if (item.m_handle->isHidden()) {
                a[j]->maximumSize = 0;
            } else {
                a[j]->sizeHint = a[j]->minimumSize = a[j]->maximumSize = item.getHandleSize(m_orient);
                a[j]->empty = false;
            }
        }else{
            a[j]->maximumSize = 0;
        }
        ++j;

        a[j] = std::make_shared<LLayoutItemData>();
        a[j]->init();
        std::shared_ptr<LWidget> w = item.m_widget;
        if(w != nullptr){
            if (w->isHidden() || item.m_collapsed) {
                a[j]->maximumSize = 0;
            } else {
                a[j]->minimumSize = pick(LLayoutItem::smartMinSize(w));
                a[j]->maximumSize = pick(LLayoutItem::smartMaxSize(w));
                a[j]->empty = false;

                bool stretch = noStretchFactorsSet;
                if (!stretch) {
                    LSizePolicy p = w->sizePolicy();
                    int sf = m_orient == LOrientation::Horizontal ? p.m_horStretch : p.m_verStretch;
                    stretch = (sf != 0);
                }
                if (stretch) {
                    a[j]->stretch = item.getWidgetSize(m_orient);
                    a[j]->sizeHint = a[j]->minimumSize;
                    a[j]->expansive = true;
                } else {
                    a[j]->sizeHint = std::max<int>(item.getWidgetSize(m_orient), a[j]->minimumSize);
                }
            }
        }else{
            a[j]->maximumSize = 0;
        }
        ++j;
    }

    LLayoutItemData::geomCalc(a, 0, (int)a.size(), pick(r.topLeft()), pick(r.size()), 0);

    int i =0;
    for (LSplitterLayoutItem& item : m_items) {
        setGeo(item, a[i*2+1]->pos, a[i*2+1]->size, false);
        ++i;
    }
}

void LSplitter::setGeo(LSplitterLayoutItem &item, int p, int s, bool allowCollapse)
{
    std::shared_ptr<LWidget> w = item.m_widget;
    if(w != nullptr){
        LRect<int> r;
        LRect<int> contents = contentsRect();
        if (m_orient == LOrientation::Horizontal) {
            r.setRect(p, contents.m_y, s, contents.height());
        } else {
            r.setRect(contents.m_x, p, contents.width(), s);
        }
        item.m_rect = r;

        int minSize = pick(LLayoutItem::smartMinSize(w));
        if (m_orient == LOrientation::Horizontal && isRightToLeft()){
            r.moveRight(contents.width() - r.m_x);
        }

        if (allowCollapse){
            item.m_collapsed = s <= 0 && minSize > 0 && !w->isHidden();
        }

        //   Hide the child widget, but without calling hide() so that
        //   the splitter handle is still shown.
        if (item.m_collapsed){
            r.moveTopLeft(LPoint<int>(-r.width(), -r.height()));
        }

        w->setGeometry(r);
        if(item.m_handle != nullptr){
            if (!item.m_handle->isHidden()) {
                LSize<int> hs = item.m_handle->sizeHint();
                const LMargins<int> m = item.m_handle->contentsMargins();
                if (m_orient==LOrientation::Horizontal) {
                    if (isRightToLeft()){
                        p = contents.width() - p + hs.width();
                    }
                    item.m_handle->setGeometry(p-hs.width() - m.m_left,
                                               contents.m_y,
                                               hs.width() + m.m_left + m.m_right,
                                               contents.height());
                } else {
                    item.m_handle->setGeometry(contents.m_x,
                                               p-hs.height() - m.m_top,
                                               contents.width(),
                                               hs.height() + m.m_top + m.m_bottom);
                }
            }
        }
    }
}

void LSplitter::setSizesHelper(const std::list<int> &sizes, bool clampNegativeSize)
{
    int i =0;
    for (const int v : sizes) {
        if(i < m_items.size()){
            LSplitterLayoutItem& item = m_items[i];

            item.m_collapsed = false;
            item.m_sizer = v;
            if (clampNegativeSize && item.m_sizer < 0){
                item.m_sizer = 0;
            }
            int smartMinSize = 0;
            std::shared_ptr<LWidget> w = item.m_widget;
            if(w != nullptr){
                smartMinSize = pick(LLayoutItem::smartMinSize(w));
            }
            // Make sure that we reset the collapsed state.
            if (item.m_sizer == 0) {
                if (collapsible(item) && smartMinSize > 0) {
                    item.m_collapsed = true;
                } else {
                    item.m_sizer = smartMinSize;
                }
            } else {
                if (item.m_sizer < smartMinSize){
                    item.m_sizer = smartMinSize;
                }
            }
        }else{
            break;
        }
        ++i;
    }
    doResize();
}

bool LSplitter::collapsible(const LSplitterLayoutItem &item) const
{
    if (item.m_collapsible < 2) {
        return (bool)item.m_collapsible;
    } else {
        return m_childrenCollapsible;
    }
}

int LSplitter::addWidgetHelper(std::shared_ptr<LWidget> w, int pos)
{
    if(pos>=0 && pos < (int)m_items.size()){
        if(m_items[pos].m_widget != nullptr){ //执行插入
            if(m_items[m_items.size()-1].m_widget != nullptr){
                m_items.push_back(LSplitterLayoutItem());
            }

            for(int k =(int)m_items.size()-1; k>pos; k--){
                m_items[k].m_widget = m_items[k-1].m_widget;
            }
        }
        m_items[pos].m_widget = w;
    }else if(pos >= (int)m_items.size()){
        m_items.resize(pos +1);
        m_items[pos].m_widget = w;
    }else{
        for(pos =0; pos < m_items.size(); ++pos){
            if(m_items[pos].m_widget == nullptr){
                m_items[pos].m_widget = w;
                return pos;
            }
        }

        m_items.push_back(LSplitterLayoutItem(w));
        pos = (int)m_items.size()-1;
    }

    if(m_items[pos].m_handle == nullptr){
        std::shared_ptr<LSplitter> s = getPtrT<LSplitter>();
        m_items[pos].m_handle = makeInit<LSplitterHandle>(s);
    }
    if(w != nullptr){
        w->lower();
    }

    if(isVisible()){
        m_items[pos].m_handle->show();
    }
    return pos;
}


LK_NAMESPACE_END
