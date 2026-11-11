#include "llayout.h"
#include "lwidget.h"
#include "lkgui/lguiapplication.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN
static bool removeWidgetRecursively(std::shared_ptr<LLayoutItem> li, std::shared_ptr<LWidget> w)
{
    std::shared_ptr<LLayout> lay = li->layout();
    if (lay == nullptr){
        return false;
    }
    int i = 0;
    std::shared_ptr<LLayoutItem> child;
    while ((child = lay->itemAt(i))) {
        if (child->widget() == w) {
            lay->removeItem(i, true);
            return true;
        } else if (removeWidgetRecursively(child, w)) {
            return true;
        } else {
            ++i;
        }
    }
    return false;
}

LLayout::LLayout()
{
    m_enabled = true;
    m_activated = true;
}


std::shared_ptr<LWidget> LLayout::parentWidget() const
{
    std::shared_ptr<LWidget> ret;
    std::shared_ptr<LObject> p = parent();
    if (p != nullptr) {
        if(p->isWidgetType()){
            ret = std::dynamic_pointer_cast<LWidget>(p);
        }else{
            std::shared_ptr<LLayout> parentLayout = std::dynamic_pointer_cast<LLayout>(p);
            if (parentLayout != nullptr) {
                ret = parentLayout->parentWidget();
            }
        }
    }
    return ret;
}

bool LLayout::activate()
{
    std::shared_ptr<LObject> p = parent();
    if(p == nullptr){
        return false;
    }

    if(!p->isWidgetType()){
        std::shared_ptr<LLayout> pl = std::dynamic_pointer_cast<LLayout>(p);
        if (pl != nullptr) {
            return pl->activate();
        }
    }

    if (m_activated){
        return false;
    }

    std::shared_ptr<LWidget> pw = std::dynamic_pointer_cast<LWidget>(p);
    if (pw == nullptr) {
        return false;
    }

    std::shared_ptr<LLayout> layout = getPtrT<LLayout>();
    activateRecursiveHelper(layout);

    uint32_t explMin, explMax;
    if(pw->m_pExtraData != nullptr){
        explMin = pw->m_pExtraData->explicitMinSize;
        explMax = pw->m_pExtraData->explicitMaxSize;
    }else{
        explMin = 0;
        explMax = 0;
    }

    switch (m_constraint) {
    case SetFixedSize:{
        pw->setFixedSize(totalSizeHint());
        break;
    }
    case SetMinimumSize:{
        pw->setMinimumSize(totalMinimumSize());
        break;
    }
    case SetMaximumSize:{
        pw->setMaximumSize(totalMaximumSize());
        break;
    }
    case SetMinAndMaxSize:{
        pw->setMinimumSize(totalMinimumSize());
        pw->setMaximumSize(totalMaximumSize());
        break;
    }
    case SetDefaultConstraint: {
        bool widthSet = explMin & (uint8_t)LOrientation::Horizontal;
        bool heightSet = explMin & (uint8_t)LOrientation::Vertical;
        if (pw->isWindow()) {
            LSize<int> ms = totalMinimumSize();
            if (widthSet){
                ms.setWidth(pw->minimumWidth());
            }
            if (heightSet){
                ms.setHeight(pw->minimumHeight());
            }
            pw->setMinimumSize(ms);
        } else if (!widthSet || !heightSet) {
            LSize<int> ms = pw->minimumSize();
            if (!widthSet){
                ms.setWidth(0);
            }
            if (!heightSet){
                ms.setHeight(0);
            }
            pw->setMinimumSize(ms);
        }
        break;
    }
    case SetNoConstraint:
        break;
    }

    doResize();
    if (pw->m_pExtraData != nullptr) {
        pw->m_pExtraData->explicitMinSize = explMin;
        pw->m_pExtraData->explicitMaxSize = explMax;
    }
    // ideally only if sizeHint() or sizePolicy() has changed
    pw->updateGeometry();
    return true;
}

void LLayout::invalidate()
{
    m_rect = LRect<int>();
    update();
}

void LLayout::paint(LPainter *painter)
{
    LColor orgColor = painter->pen()->color();
    painter->setPen(LColorID::Red);
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->addRect(geometry().toRectF());
    for (LLayoutPosition& pos : m_positions) {
        std::shared_ptr<LLayoutItem> item  = pos.m_item;
        if(item != nullptr){
            path->addRect(item->geometry().toRectF());
        }
    }
    painter->drawPath(path);
    painter->setPen(orgColor);
}

void LLayout::maxExpCalc(int &max, bool &exp, bool &empty, int boxmax, bool boxexp, bool boxempty)
{
    if (exp) {
        if (boxexp){
            max = std::max(max, boxmax);
        }
    } else {
        if (boxexp || (empty && (!boxempty || max == 0))){
            max = boxmax;
        }
        else if (empty == boxempty){
            max = std::min(max, boxmax);
        }
    }
    exp = exp || boxexp;
    empty = empty && boxempty;
}

void LLayout::setEnabled(bool v)
{
    m_enabled = v;
}

bool LLayout::isEnabled() const
{
    return m_enabled;
}

void LLayout::widgetEvent(std::shared_ptr<LEvent> e)
{
    if (!m_enabled){
        return;
    }

    switch (e->type()) {
    case LEvent::Resize:{
        if (m_activated){
            doResize();
        }else{
            activate();
        }
        break;
    }
    case LEvent::ChildRemoved:{
        std::shared_ptr<LChildEvent> c = std::dynamic_pointer_cast<LChildEvent>(e);
        if(c != nullptr){
            std::shared_ptr<LWidget> child = std::dynamic_pointer_cast<LWidget>(c->child());
            if (child != nullptr && child->isWidgetType()) {
                removeWidgetRecursively(getPtrT<LLayout>(), child);
            }
        }
        break;
    }
    case LEvent::LayoutRequest:{
        std::shared_ptr<LWidget> p = parentWidget();
        if(p != nullptr){
            if(p->isVisible()){
                activate();
            }
        }
        break;
    }
    default:{
        break;
    }
    }
}

void LLayout::raise()
{
    for (const LLayoutPosition& p: m_positions){
        if(p.m_item != nullptr){
            p.m_item->raise();
        }
    }
}

void LLayout::lower()
{
    for (const LLayoutPosition& p: m_positions){
        if(p.m_item != nullptr){
            p.m_item->lower();
        }
    }
}

void LLayout::show()
{
    m_isHide = false;
    invalidate();
}

void LLayout::hide()
{
    m_isHide = true;
    invalidate();
}

void LLayout::clearFocus()
{
    for (const LLayoutPosition& p: m_positions){
        if(p.m_item != nullptr){
            p.m_item->clearFocus();
        }
    }
}

bool LLayout::containWidget(std::shared_ptr<LWidget> w)
{
    for (const LLayoutPosition& p: m_positions){
        if(p.m_item != nullptr){
            if(p.m_item->containWidget(w)){
                return true;
            }
        }
    }
    return false;
}

bool LLayout::setFocus()
{
    if(m_isHide){
        return false;
    }

    std::shared_ptr<LWidget> pw = parentWidget();
    if(pw != nullptr){
        const std::list<std::shared_ptr<LTreeObj> >& ch = pw->children();
        for(const std::shared_ptr<LTreeObj>& c : ch){
            std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(c);
            if(w!= nullptr && w->isVisible() && w->isEnabled()){
                if(containWidget(w)){
                    w->setFocus();
                    return true;
                }
            }
        }
    }else{
        for (const LLayoutPosition& p: m_positions){
            if(p.m_item != nullptr){
                if(p.m_item->setFocus()){
                    return true;
                }
            }
        }
    }
    return false;
}

int LLayout::totalHeightForWidth(int w) const
{
    int side=0, top=0;
    if (m_topLevel) {
        std::shared_ptr<LWidget> pw = parentWidget();
        if(pw != nullptr){
            side += pw->m_leftMargin + pw->m_rightMargin;
            top += pw->m_topMargin + pw->m_bottomMargin;
        }
    }
    return heightForWidth(w - side) + top;
}

LSize<int> LLayout::totalSizeHint() const
{
    int side=0, top=0;
    if (m_topLevel) {
        std::shared_ptr<LWidget> pw = parentWidget();
        if(pw != nullptr){
            side += pw->m_leftMargin + pw->m_rightMargin;
            top += pw->m_topMargin + pw->m_bottomMargin;
        }
    }

    LSize<int> s = sizeHint();
    if (hasHeightForWidth()){
        s.setHeight(heightForWidth(s.width() + side));
    }
    return s + LSize<int>(side, top);
}

LSize<int> LLayout::totalMinimumSize() const
{
    int side=0, top=0;
    if (m_topLevel) {
        std::shared_ptr<LWidget> pw = parentWidget();
        if(pw != nullptr){
            side += pw->m_leftMargin + pw->m_rightMargin;
            top += pw->m_topMargin + pw->m_bottomMargin;
        }
    }

    LSize<int>  s = minimumSize();
    return s + LSize<int>(side, top);
}

LSize<int> LLayout::totalMaximumSize() const
{
    int side=0, top=0;
    if (m_topLevel) {
        std::shared_ptr<LWidget> pw = parentWidget();
        if(pw != nullptr){
            side += pw->m_leftMargin + pw->m_rightMargin;
            top += pw->m_topMargin + pw->m_bottomMargin;
        }
    }

    LSize<int> s = maximumSize();
    if (m_topLevel){
        s = LSize<int>(std::min(s.width() + side, LLAYOUTSIZE_MAX),
                       std::min(s.height() + top, LLAYOUTSIZE_MAX));
    }
    return s;
}

void LLayout::setContentsMargins(int left, int top, int right, int bottom)
{
    m_leftMargin = left;
    m_topMargin = top;
    m_rightMargin = right;
    m_bottomMargin = bottom;

}

void LLayout::setContentsMargins(const LMargins<int> &margins)
{
    setContentsMargins(margins.m_left, margins.m_top, margins.m_right, margins.m_bottom);
}

const LMargins<int> LLayout::contentsMargins() const
{
    return LMargins<int>(m_leftMargin, m_topMargin, m_rightMargin, m_bottomMargin);
}

LRect<int> LLayout::contentsRect() const
{
    return m_rect.adjusted(m_leftMargin, m_topMargin, -m_rightMargin, -m_bottomMargin);
}

void LLayout::reparentChildWidgets(std::shared_ptr<LWidget> mw)
{
    bool mwVisible = mw!=nullptr && mw->isVisible();
    for (const LLayoutPosition& pos : m_positions) {
        if(pos.m_item != nullptr){
            std::shared_ptr<LWidget> w = pos.m_item->widget();
            if (w != nullptr) {
                std::shared_ptr<LWidget> pw = w->parentT<LWidget>();
                bool needShow = mwVisible && !(w->isHidden() && w->testAttribute(LWidgetAttribute::WStateExplicitShowHide));
                if (pw != mw){
                    w->setParent(mw);
                }

                if (needShow){
                    std::shared_ptr<LBaseInvoker<>>  invoker = makeInvoker(w.get(),
                                                                              &LWidget::showIfNotHidden,
                                                                              ConnectionType::Queued);
                    invoker->invoke();
                }
            } else {
                std::shared_ptr<LLayout> l = pos.m_item->layout();
                if (l != nullptr) {
                    l->reparentChildWidgets(mw);
                }
            }
        }
    }
}

void LLayout::update()
{
    std::shared_ptr<LLayout> layout = getPtrT<LLayout>();
    while ((layout!=nullptr) && layout->m_activated) {
        layout->m_activated = false;
        std::shared_ptr<LObject> p = layout->parent();
        if (p != nullptr) {
            if(p->isWidgetType()){
                // lWarn() << "Post LEvent::LayoutRequest";
                std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(p);
                std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::LayoutRequest);
                LCoreApplication::postEvent(w, e);
                break;
            }else{
                layout = std::dynamic_pointer_cast<LLayout>(p);
            }
        }else{
            break;
        }
    }
}


LSize<int> LLayout::minimumSize() const
{
    return LSize<int>(0, 0);
}

LSize<int> LLayout::maximumSize() const
{
    return LSize<int>(LLAYOUTSIZE_MAX, LLAYOUTSIZE_MAX);
}

LFlags<LOrientation> LLayout::expandingDirections() const
{
    return LOrientation::Horizontal | LOrientation::Vertical;
}

void LLayout::setGeometry(const LRect<int> &r)
{
    m_rect = r;
    // lWarn() << "obj:" << name() <<",w:" << r.width() << ",h:" <<r.height();
}

LRect<int> LLayout::geometry() const
{
    return m_rect;
}

bool LLayout::isEmpty() const
{
    for (const LLayoutPosition& pos : m_positions) {
        if(!pos.isEmpty()){
            return false;
        }
    }
    return true;
}

uint32_t LLayout::controlTypes() const
{
    uint32_t types = 0;
    for (const LLayoutPosition& pos : m_positions) {
        if(pos.m_item != nullptr){
            types |=  pos.m_item->controlTypes();
        }
    }
    return types;
}

void LLayout::holdPosition(int posId, bool isHold)
{
    if(posId <0){
        for (LLayoutPosition& pos : m_positions) {
            pos.m_isHoldPos = isHold;
        }
    }else{
        if(posId <= m_positions.size()){
            m_positions[posId].m_isHoldPos = isHold;
        }
    }
}

std::shared_ptr<LLayoutItem> LLayout::itemAt(uint32_t i)
{
    if(i < m_positions.size()){
        return m_positions[i].m_item;
    }
    return nullptr;
}

int LLayout::itemPos(std::shared_ptr<LLayoutItem> item)
{
    if(item == nullptr){
        return -1;
    }

    int i =0;
    for (const LLayoutPosition& pos : m_positions) {
        if(pos.m_item == item){
            return i;
        }
        ++i;
    }
    return -1;
}

bool LLayout::hasItem(uint32_t pos)
{
    if(pos < m_positions.size()){
        return m_positions[pos].m_item != nullptr;
    }
    return false;
}

void LLayout::moveTothread(LIThread *thread)
{
    LObject::moveTothread(thread);
    for(const LLayoutPosition& pos : m_positions){
        std::shared_ptr<LObject> obj = std::dynamic_pointer_cast<LObject>(pos.m_item);
        if(obj != nullptr){
            obj->moveTothread(thread);
        }
    }

    std::shared_ptr<LObject> parent = m_parent.lock();
    if(parent != nullptr){
        if(parent->threadData() != m_data->m_threadData.lock()){
            if(parent->isWidgetType()){
                setParentWidget(nullptr);
            }else{
                setParent(nullptr);
            }
        }
    }
}

void LLayout::setParentWidget(std::shared_ptr<LWidget> o)
{
    m_topLevel = true;
    std::shared_ptr<LObject> parent = m_parent.lock();
    if (o == parent){
        return;
    }

    if (parent != nullptr) {
        if(!parent->isWidgetType()){
            std::shared_ptr<LLayout> p = std::dynamic_pointer_cast<LLayout>(parent);
            if(p != nullptr){
                std::shared_ptr<LLayoutItem> tThis = getPtrT<LLayoutItem>();
                p->removeItem(tThis);
            }
        }
    }
    // m_parent = o;
    initParentWidget(o);
}

int LLayout::setParent(std::shared_ptr<LLayout> o, int pos)
{
    m_topLevel = false;
    std::shared_ptr<LObject> parent = m_parent.lock();
    if (o == parent){
        return -1;
    }

    if (parent != nullptr) {
        if(!parent->isWidgetType()){
            std::shared_ptr<LLayout> p = std::dynamic_pointer_cast<LLayout>(parent);
            if(p != nullptr){
                std::shared_ptr<LLayoutItem> tThis = getPtrT<LLayoutItem>();
                p->removeItem(tThis);
            }
        }
    }
    // m_parent = o;
    return initParent(o, pos);
}

void LLayout::clearInvalidPos(bool update)
{
    std::vector<LLayoutPosition> t;
    for (const LLayoutPosition& pos : m_positions) {
        if(!pos.isEmpty()){
            t.push_back(LLayoutPosition(pos.m_item, pos.m_posData, pos.m_isHoldPos));
        }
    }

    if(t.size() != m_positions.size()){
        m_positions.swap(t);
        if(update){
            invalidate();
        }
    }
}

int LLayout::removeItem(std::shared_ptr<LLayoutItem> child, bool update)
{
    if(child != nullptr){
        //不允许重复只会有一个
        int iPos = 0;
        for (LLayoutPosition& pos : m_positions) {
            if(pos.m_item == child){
                pos.m_item = nullptr;

                std::shared_ptr<LLayout> chl = std::dynamic_pointer_cast<LLayout>(child);
                if(chl != nullptr){
                    std::shared_ptr<LLayout> tThis = getPtrT<LLayout>();
                    if(chl->parent() == tThis){
                        chl->m_parent.reset();
                    }
                }

                if(update){
                    clearInvalidPos(false);
                    invalidate();
                }
                return iPos;
            }
            iPos++;
        }
    }
    return -1;
}

void LLayout::removeItem(uint32_t pos, bool update)
{
    if(pos <m_positions.size()){
        std::shared_ptr<LLayoutItem> child = m_positions[pos].m_item;
        if(child != nullptr){
            m_positions[pos].m_item = nullptr;

            std::shared_ptr<LLayout> chl = child->layout();
            if(chl != nullptr){
                std::shared_ptr<LLayout> tThis = getPtrT<LLayout>();
                if(chl->parent() == tThis){
                    chl->m_parent.reset();
                }
            }
            if(update){
                clearInvalidPos(false);
                invalidate();
            }
        }
    }
}

int LLayout::addItem(std::shared_ptr<LLayoutItem> child,  int posId, bool update)
{
    if(child == nullptr || child.get() == this){
        return -1;
    }

    int i=0;
    bool hasRemove = false;
    for (const LLayoutPosition& pos : m_positions) {
        if(pos.m_item == child){
            if(i == posId){
                return posId;
            }else{
                hasRemove = true;
                removeItem(i, false);
                break;
            }
        }
        ++i;
    }


    std::shared_ptr<LLayout> chl = child->layout();
    if(chl != nullptr){
        std::shared_ptr<LLayout> tThis = getPtrT<LLayout>();
        posId = chl->setParent(tThis, posId);
    }else{
        std::shared_ptr<LWidget> chw = child->widget();
        if(chw != nullptr){
            addChildWidget(chw);
        }
        posId = addItemHelper(child, posId);
    }

    if(update){
        if(hasRemove){
            clearInvalidPos(false);
        }
        invalidate();
    }
    return posId;
}

int LLayout::addWidget(std::shared_ptr<LWidget> w,
                        LFlags<LAlignment> align,
                        int posId,
                        bool update)
{
    addChildWidget(w);
    std::shared_ptr<LWidgetItem> item = std::make_shared<LWidgetItem>(w);
    item->m_alignment = align;
    posId = addItemHelper(item, posId);
    if(update){
        invalidate();
    }
    return posId;
}

int LLayout::removeWidget(std::shared_ptr<LWidget> w, bool update)
{
    int posId = 0;
    for (LLayoutPosition& pos : m_positions) {
        if(pos.m_item!= nullptr){
            if (pos.m_item->widget() == w) {
                pos.m_item = nullptr;

                if(update){
                    clearInvalidPos();
                    invalidate();
                }
                return posId;
            }
        }
        posId++;
    }
    return -1;
}


std::shared_ptr<LWidget> LLayout::widget(int index)
{
    if(index>=0 && index< (int)m_positions.size()){
        const LLayoutPosition& p = m_positions[index];
        return p.m_item->widget();
    }
    return nullptr;
}

int LLayout::widgetPos(const std::shared_ptr<LWidget> w)
{
    if(w == nullptr){
        return -1;
    }

    int i =0;
    for (const LLayoutPosition& pos : m_positions) {
        if(pos.m_item!= nullptr){
            if(pos.m_item->widget() == w){
                return i;
            }

        }
        ++i;
    }
    return -1;
}

bool LLayout::setAlignment(std::shared_ptr<LWidget> w, LFlags<LAlignment> align, bool update)
{
    for (LLayoutPosition& pos : m_positions) {
        if(pos.m_item!= nullptr){
            if (pos.m_item->widget() == w) {
                pos.m_item->m_alignment = align;
                if(update){
                    invalidate();
                }
                return true;
            }
        }
    }
    return false;
}

bool LLayout::setAlignment(std::shared_ptr<LLayoutItem> l, LFlags<LAlignment> align, bool update)
{
    for (LLayoutPosition& pos : m_positions) {
        if(pos.m_item!= nullptr){
            if (pos.m_item == l) {
                pos.m_item->m_alignment = align;
                if(update){
                    invalidate();
                }
                return true;
            }
        }
    }
    return false;
}

std::shared_ptr<LLayout> LLayout::layout()
{
    return getPtrT<LLayout>();
}

void LLayout::activateRecursiveHelper(std::shared_ptr<LLayout> layout)
{
    layout->invalidate();
    for (const LLayoutPosition& pos : layout->m_positions) {
        if(pos.m_item != nullptr){
            std::shared_ptr<LLayout> iLayout = pos.m_item->layout();
            if(iLayout != nullptr){
                activateRecursiveHelper(iLayout);
            }
        }
    }
    layout->m_activated = true;
}

void LLayout::doResize()
{
    std::shared_ptr<LWidget> pw = parentWidget();
    if(pw != nullptr){
        setGeometry(pw->customRect());
    }
}

void LLayout::initParentWidget(std::shared_ptr<LWidget> parent)
{
    if (parent != nullptr) {
        if (m_data->m_threadData.lock() != parent->objData()->m_threadData.lock()) {
            lWarn() << "LLayout::initParent: cannot init widget parent is in a different thread";
            return;
        }
        m_parent = parent;
        reparentChildWidgets(parent);
    }
}

int LLayout::initParent(std::shared_ptr<LLayout> parent, int pos)
{
    if (parent != nullptr) {
        if (m_data->m_threadData.lock() != parent->objData()->m_threadData.lock()) {
            lWarn() << "LLayout::initParent: cannot init layout parent is in a different thread";
            return -1;
        }

        m_parent = parent;
        std::shared_ptr<LWidget> mw = parentWidget();
        if(mw != nullptr){
            reparentChildWidgets(mw);
        }
        std::shared_ptr<LLayout> tThis = getPtrT<LLayout>();
        return parent->addItemHelper(tThis, pos);
    }
    return -1;
}

void LLayout::addChildWidget(std::shared_ptr<LWidget> w)
{
    if(w != nullptr){
        std::shared_ptr<LWidget> mw = parentWidget();
        std::shared_ptr<LWidget> pw = w->parentT<LWidget>();

        if(pw != nullptr){
            if(w->testAttribute(LWidgetAttribute::LaidOut)){
                std::shared_ptr<LLayout> l = pw->layout();
                if (l != nullptr ) {
                    removeWidgetRecursively(l, w);
                }
            }

            if (mw!=nullptr && pw != mw) {
                pw = nullptr;
            }
        }

        bool needShow = mw && mw->isVisible() && !(w->isHidden() && w->testAttribute(LWidgetAttribute::WStateExplicitShowHide));
        if (pw==nullptr && mw){
            w->setParent(mw);
        }

        w->setAttribute(LWidgetAttribute::LaidOut);
        if (needShow){
            std::shared_ptr<LBaseInvoker<>>  invoker = makeInvoker(w.get(),
                                                                  &LWidget::showIfNotHidden,
                                                                  ConnectionType::Queued);
            invoker->invoke();
        }
    }
}

int LLayout::addItemHelper(std::shared_ptr<LLayoutItem> child, int pos)
{
    if(pos>=0 && pos < (int)m_positions.size()){
        if(m_positions[pos].m_item != nullptr){ //执行插入
            if(m_positions[m_positions.size()-1].m_item != nullptr){
                m_positions.push_back(LLayoutPosition());
            }

            for(int k =(int)m_positions.size()-1; k>pos; k--){
                m_positions[k].m_item = m_positions[k-1].m_item;
            }
        }
        m_positions[pos].m_item = child;
        return pos;
    }else if(pos >= (int)m_positions.size()){
        m_positions.resize(pos +1);
        m_positions[pos].m_item = child;
        return pos;
    }else{
        for(pos =0; pos < m_positions.size(); ++pos){
            if(m_positions[pos].m_item == nullptr){
                m_positions[pos].m_item = child;
                return pos;
            }
        }

        m_positions.push_back(LLayoutPosition(child));
        return (int)m_positions.size()-1;
    }
}

LRect<int> LLayout::alignmentRect(const LRect<int> &r) const
{
    LSize<int> s = sizeHint();
    LFlags<LAlignment> a = m_alignment;

    LLayout *that = const_cast<LLayout *>(this);
    that->m_alignment = 0;
    LSize<int> ms = that->maximumSize();
    that->m_alignment = a;

    if ((expandingDirections() & LOrientation::Horizontal) ||
        !(a & LAlignment::HorizontalMask)) {
        s.setWidth(std::min<int>(r.m_width, ms.width()));
    }
    if ((expandingDirections() & LOrientation::Vertical) ||
        !(a & LAlignment::VerticalMask)) {
        s.setHeight(std::min<int>(r.height(), ms.height()));
    } else if (hasHeightForWidth()) {
        int hfw = heightForWidth(s.width());
        if (hfw < s.height()){
            s.setHeight(std::min<int>(hfw, ms.height()));
        }
    }

    s = s.boundedTo(r.size());
    int x = r.m_x;
    int y = r.m_y;

    if (a &LAlignment::Bottom){
        y += (r.height() - s.height());
    }else if (!(a & LAlignment::Top)){
        y += (r.height() - s.height()) / 2;
    }

    std::shared_ptr<LWidget> parent = parentWidget();
    if(parent != nullptr){
        a = LGuiApplication::visualAlignment(parent->layoutDirection(), a);
    }else{
        a = LGuiApplication::visualAlignment(LGuiApplication::layoutDirection(), a);
    }

    if (a & LAlignment::Right){
        x += (r.width() - s.width());
    }else if (!(a & LAlignment::Left)){
        x += (r.width() - s.width()) / 2;
    }

    return LRect<int>(x, y, s.width(), s.height(), false);
}


LK_NAMESPACE_END

