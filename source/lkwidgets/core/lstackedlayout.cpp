#include "lstackedlayout.h"
#include "lkwidgets/core/lwidget.h"

LK_NAMESPACE_BEGIN

LStackedLayout::LStackedLayout(bool isStackAll):m_isStackAll(isStackAll){}

void LStackedLayout::setCurrentIndex(int index)
{
    if(index<0 || index >= m_positions.size()){
        return;
    }

    if(index == m_current){
        return;
    }

    std::shared_ptr<LLayoutItem> prev = currentItem();
    std::shared_ptr<LLayoutItem> next = itemAt(index);
    if (next == nullptr || next == prev){
        return;
    }

    bool reenableUpdates = false;
    std::shared_ptr<LWidget> parent = parentWidget();

    if (parent!=nullptr && parent->updatesEnabled()) {
        reenableUpdates = true;
        parent->setUpdatesEnabled(false);
    }

    std::shared_ptr<LWidget> fw = parent ? parent->window()->focusWidget() : nullptr;
    const bool focusWasOnOldPage = fw && (prev !=nullptr && prev->containWidget(fw));

    if (prev) {
        prev->clearFocus();
        if (m_isStackAll == false){
            prev->hide();
        }
    }

    m_current = index;
    if(m_isStackAll){
        next->raise();
    }
    next->show();

    // try to move focus onto the incoming widget if focus
    // was somewhere on the outgoing widget.

    if (parent) {
        if (focusWasOnOldPage) {
            std::shared_ptr<LWidget> nextW = next->widget();
            if(nextW != nullptr){
                std::shared_ptr<LWidget> nfw = nextW->focusWidget();
                if (nfw != nullptr)
                    nfw->setFocus();
                else {
                    // second best: first child widget in the focus chain
                    if (std::shared_ptr<LWidget> i = fw) {
                        std::shared_ptr<LWidget> t = i->nextInFocusChain();
                        while (t!=fw && t==nullptr) {
                            i = t;
                            if ((((uint8_t)i->focusPolicy() & (uint8_t)LFocusPolicy::Tab) == (uint8_t)LFocusPolicy::Tab)
                                && !i->focusProxy() && i->isVisibleTo(nextW) && i->isEnabled()
                                && nextW->isAncestorOf(i)) {
                                i->setFocus();
                                break;
                            }
                            t = i->nextInFocusChain();
                        }
                        // third best: incoming widget
                        if (i == fw ){
                            nextW->setFocus();
                        }
                    }
                }
            }else{
                // look for the best focus widget we can find
                next->setFocus();
            }
        }
    }
    if (reenableUpdates){
        parent->setUpdatesEnabled(true);
    }
    sigCurrentChanged.emt(index);
}

std::shared_ptr<LLayoutItem> LStackedLayout::currentItem()
{
    if(m_current>=0 && m_current< (int)m_positions.size()){
        const LLayoutPosition& p = m_positions[m_current];
        return p.m_item;
    }
    return nullptr;
}

std::shared_ptr<LWidget> LStackedLayout::currentWidget()
{
    if(m_current>=0 && m_current< (int)m_positions.size()){
        const LLayoutPosition& p = m_positions[m_current];
        return p.m_item->widget();
    }
    return nullptr;
}

LSize<int> LStackedLayout::sizeHint() const
{
    LSize<int> s(0, 0);
    for (const LLayoutPosition& p: m_positions){
        if(p.m_item != nullptr){
            s = s.expandedTo(p.m_item->sizeHint());
        }
    }
    return s;
}

LSize<int> LStackedLayout::minimumSize() const
{
    LSize s(0, 0);
    for (const LLayoutPosition& p: m_positions){
        if(p.m_item != nullptr){
            s = s.expandedTo(p.m_item->minimumSize());
        }
    }
    return s;
}

void LStackedLayout::setGeometry(const LRect<int> &r)
{
    if(m_isStackAll){
        for (const LLayoutPosition& p: m_positions){
            if(p.m_item != nullptr){
                p.m_item->setGeometry(r);
            }
        }
    }else{
        std::shared_ptr<LLayoutItem> item = currentItem();
        if(item != nullptr){
            item->setGeometry(r);
        }
    }
}

bool LStackedLayout::hasHeightForWidth() const
{
    for (const LLayoutPosition& p: m_positions){
        if(p.m_item != nullptr){
            if (p.m_item->hasHeightForWidth()){
                return true;
            }
        }
    }
    return false;
}

int LStackedLayout::heightForWidth(int width) const
{    
    int hfw = 0;
    for (const LLayoutPosition& p: m_positions){
        if(p.m_item != nullptr){
            if (p.m_item->hasHeightForWidth()){
                hfw = std::max<int>(hfw, p.m_item->heightForWidth(width));
            }
        }
    }
    hfw = std::max<int>(hfw, minimumSize().height());
    return hfw;
}

void LStackedLayout::raise()
{
    if(m_current>=0 && m_current< (int)m_positions.size()){
        const LLayoutPosition& p = m_positions[m_current];
        if(p.m_item != nullptr){
            p.m_item->raise();
        }
    }
}

void LStackedLayout::lower()
{
    if(m_current>=0 && m_current< (int)m_positions.size()){
        const LLayoutPosition& p = m_positions[m_current];
        if(p.m_item != nullptr){
            p.m_item->lower();
        }
    }
}

bool LStackedLayout::setFocus()
{
    if(m_current>=0 && m_current< (int)m_positions.size()){
        const LLayoutPosition& p = m_positions[m_current];
        if(p.m_item != nullptr){
            return p.m_item->setFocus();
        }
    }
    return false;
}

int LStackedLayout::addItem(std::shared_ptr<LLayoutItem> child, int pos, bool update)
{
    pos = LLayout::addItem(child, pos, update);
    if(pos >=0){
        if(m_current <0){
            setCurrentIndex(pos);
        }else{
            if(pos != m_current){
                child->hide();
            }else{
                if(m_isStackAll){
                    child->raise();
                }
                child->show();
            }
        }
    }
    return pos;
}

int LStackedLayout::addWidget(std::shared_ptr<LWidget> w, LFlags<LAlignment> align, int pos, bool update)
{
    pos = LLayout::addWidget(w, align, pos, update);
    if(pos >=0){
        if(m_current <0){
            setCurrentIndex(pos);
        }else{
            std::shared_ptr<LLayoutItem> child = LLayout::itemAt(pos);
            if(child != nullptr){
                if(pos != m_current){
                    child->hide();
                }else{
                    if(m_isStackAll){
                        child->raise();
                    }
                    child->show();
                }
            }
        }
    }
    return pos;
}

int LStackedLayout::removeItem(std::shared_ptr<LLayoutItem> child, bool update)
{
    int pos = LLayout::removeItem(child, update);
    if(pos == m_current){
        if(m_positions.empty()){
            m_current=-1;
            sigCurrentChanged.emt(m_current);
        }else{
            setCurrentIndex(0);
        }
    }
    return pos;
}

void LStackedLayout::removeItem(uint32_t pos, bool update)
{
    LLayout::removeItem(pos, update);
    if((int)pos == m_current){
        if(m_positions.empty()){
            m_current=-1;
            sigCurrentChanged.emt(m_current);
        }else{
            setCurrentIndex(0);
        }
    }
}

LK_NAMESPACE_END
