#include "labstractbutton.h"
#include "lkwidgets/core/lapplication.h"
#include "lkwidgets/dialog/ldialog.h"

LK_NAMESPACE_BEGIN

LAbstractButton::LAbstractButton(LSizePolicy::ControlType type,
                                 std::shared_ptr<LWidget> parent,
                                 LFlags<LWindowType> f)
    : LWidget(parent, f)
{
    m_sizePolicy.setControlType(type);
    setAttribute(LWidgetAttribute::WStateOwnSizePolicy, false);
    setForegroundRole(LPalette::ButtonText);
    setBackgroundRole(LPalette::Button);
}


bool LAbstractButton::handleEvent(std::shared_ptr<LEvent> e)
{
    if (!isEnabled()) {
        switch(e->type()) {
        case LEvent::TabletPress:
        case LEvent::TabletRelease:
        case LEvent::TabletMove:
        case LEvent::MouseButtonPress:
        case LEvent::MouseButtonRelease:
        case LEvent::MouseButtonDblClick:
        case LEvent::MouseMove:
        case LEvent::HoverMove:
        case LEvent::HoverEnter:
        case LEvent::HoverLeave:
        case LEvent::ContextMenu:
        case LEvent::Wheel:{
            return true;
        }
        default:{
            break;
        }
        }
    }

    if (e->type() == LEvent::Shortcut) {
        std::shared_ptr<LShortcutEvent> se = std::dynamic_pointer_cast<LShortcutEvent>(e);
        if (m_shortcutId != se->shortcutId()){
            return false;
        }
        if (!se->isAmbiguous()) {
            click();
        } else {
            if (focusPolicy() != LFocusPolicy::No){
                setFocus(LFocusReason::Shortcut);
            }
            window()->setAttribute(LWidgetAttribute::KeyboardFocusChange);
        }
        return true;
    }
    return LWidget::handleEvent(e);

}

void LAbstractButton::setDown(bool down)
{
    if (m_down == down){
        return;
    }
    m_down = down;
    refresh();
}

bool LAbstractButton::isDown() const
{
    return m_down;
}

void LAbstractButton::setChecked(bool checked)
{
    if (!m_checkable || m_checked == checked) {
        if (!m_blockRefresh){
            checkStateSet();
        }
        return;
    }

    if (!m_checked) {
        std::shared_ptr<LAbstractButton> t = queryCheckedButton();
        if(t.get()== this){
            // the checked button of an exclusive or autoexclusive group cannot be unchecked
            if (m_autoExclusive){
                return;
            }
        }
    }

    m_checked = checked;
    if (!m_blockRefresh){
        checkStateSet();
    }
    refresh();

    if (checked){
        notifyChecked();
    }
    sigToggled.emt(checked);

}

void LAbstractButton::click()
{
    if (!isEnabled()){
        return;
    }

    if(m_down == false){
        m_down = true;
        sigPressed.emt();
    }

    m_down = false;
    nextCheckState();
    sigReleased.emt();
    sigClicked.emt(m_checked);
}

void LAbstractButton::setCheckable(bool checkable)
{
    if (m_checkable == checkable){
        return;
    }

    m_checkable = checkable;
    m_checked = false;
}

bool LAbstractButton::isCheckable() const
{
    return m_checkable;
}

void LAbstractButton::setAutoExclusive(bool autoExclusive)
{
    m_autoExclusive = autoExclusive;
}

bool LAbstractButton::autoExclusive() const
{
    return m_autoExclusive;
}

uint16_t LAbstractButton::uiElementState(LUiElementType type) const
{
    if(isEnabled()){
        if(m_pressed){
            return uint16_t(LUiState::Sunken);
        }
    }
    return LWidget::uiElementState(type);
}

void LAbstractButton::keyPressEvent(std::shared_ptr<LKeyEvent> e)
{
    bool next = true;
    switch ((LKey)e->key()) {
    case LKey::Enter:
    case LKey::Return:{
        e->ignore();
        break;
    }
    case LKey::Select:
    case LKey::Space:
        if (!e->isAutoRepeat()) {
            setDown(true);
            repaint();
            sigPressed.emt();
        }
        break;
    case LKey::Up:{
        next = false;
        //No break;
    }
    case LKey::Left:
    case LKey::Right:
    case LKey::Down: {
        if (m_autoExclusive) {
            moveFocus(e->key());
            if (hasFocus()){ // nothing happend, propagate
                e->ignore();
            }
        } else {
            std::shared_ptr<LWidget> w = parentT<LWidget>();
            if(w == nullptr){
                w =  getPtrT<LWidget>();
            }

            bool reverse = (w->layoutDirection() == LLayoutDirection::RightToLeft);
            if (((LKey)e->key() == LKey::Left && !reverse)
                || ((LKey)e->key() == LKey::Right && reverse)) {
                next = false;
            }
            focusNextPrevChild(next);
        }
        break;
    }
    default:{
        e->ignore();
    }
    }
}

void LAbstractButton::keyReleaseEvent(std::shared_ptr<LKeyEvent> e)
{
    switch ((LKey)e->key()) {
    case LKey::Select:
    case LKey::Space:
        if (!e->isAutoRepeat() && m_down){
            innerClick();
        }
        break;
    default:{
        e->ignore();
    }
    }
}

void LAbstractButton::mousePressEvent(std::shared_ptr<LMouseEvent> e)
{
    if (e->button() != LMouseButton::Left) {
        e->ignore();
        return;
    }
    if (hitButton(e->pos())) {
        setDown(true);
        m_pressed = true;
        repaint();
        sigPressed.emt();
        e->accept();
    } else {
        e->ignore();
    }
}

void LAbstractButton::mouseReleaseEvent(std::shared_ptr<LMouseEvent> e)
{
    if (e->button() != LMouseButton::Left) {
        e->ignore();
        return;
    }

    m_pressed = false;

    if (!m_down) {
        refresh();
        e->ignore();
        return;
    }

    if (hitButton(e->pos())) {
        innerClick();
        e->accept();
    } else {
        setDown(false);
        e->ignore();
    }
}

void LAbstractButton::mouseMoveEvent(std::shared_ptr<LMouseEvent> e)
{
    if (!(e->buttons() & LMouseButton::Left) || !m_pressed) {
        e->ignore();
        return;
    }

    bool isHit = hitButton(e->pos());
    if (isHit != m_down) {
        setDown(isHit);
        repaint();
        if (m_down){
            sigPressed.emt();
        }
        else{
            sigReleased.emt();
        }
        e->accept();
    } else if (!isHit) {
        e->ignore();
    }
}

void LAbstractButton::focusInEvent(std::shared_ptr<LFocusEvent> e)
{
    fixFocusPolicy();
    LWidget::focusInEvent(e);
}

void LAbstractButton::focusOutEvent(std::shared_ptr<LFocusEvent> e)
{
    if (e->reason() != LFocusReason::Popup && m_down) {
        m_down = false;
        sigReleased.emt();
    }
    LWidget::focusOutEvent(e);
}

void LAbstractButton::changeEvent(std::shared_ptr<LEvent> e)
{
    switch (e->type()) {
    case LEvent::EnabledChange:{
        if (!isEnabled() && m_down) {
            m_down = false;
            sigReleased.emt();
        }
        break;
    }
    default:{
        break;
    }
    }
    LWidget::changeEvent(e);
}

bool LAbstractButton::hitButton(const LPoint<int> &pos) const
{
    return rect().contains(pos);
}

void LAbstractButton::checkStateSet()
{

}

void LAbstractButton::nextCheckState()
{
    if (m_checkable){
        setChecked(!m_checked);
    }
}

void LAbstractButton::notifyChecked()
{
    if (m_autoExclusive) {
        if (std::shared_ptr<LAbstractButton> b = queryCheckedButton()){
            if(b.get() != this){
                b->setChecked(false);
            }
        }
    }
}

void LAbstractButton::refresh()
{
    if (m_blockRefresh){
        return;
    }
    update();
}

void LAbstractButton::innerClick()
{
    m_down = false;
    m_blockRefresh = true;
    bool changeState = true;
    if (m_checked ) {
        std::shared_ptr<LAbstractButton> t = queryCheckedButton();
        if(t.get()== this){
            if (m_autoExclusive){
                changeState = false;
            }
        }
    }
    if (changeState) {
        nextCheckState();
    }
    m_blockRefresh = false;
    refresh();
    repaint();
    sigReleased.emt();
    sigClicked.emt(m_checked);
}

std::list<std::shared_ptr<LAbstractButton> > LAbstractButton::queryButtonList() const
{
    std::list<std::shared_ptr<LAbstractButton> > ret;
    std::shared_ptr<LTreeObj> p = parent();

    if(p!= nullptr){
        ret = p->findChildren<LAbstractButton>(m_groupId);
        if (m_autoExclusive) {
            auto isNoMemberOfMyAutoExclusiveGroup = [](std::shared_ptr<LAbstractButton> candidate) {
                return !candidate->m_autoExclusive;
            };
            ret.erase(std::remove_if(ret.begin(), ret.end(),isNoMemberOfMyAutoExclusiveGroup), ret.end());
        }
    }
    return ret;
}

std::shared_ptr<LAbstractButton> LAbstractButton::queryCheckedButton() const
{
    std::list<std::shared_ptr<LAbstractButton> > buttonList = queryButtonList();
    if (!m_autoExclusive || buttonList.size() == 1){ // no group
        return nullptr;
    }

    for (std::shared_ptr<LAbstractButton> b : buttonList) {
        if (b->m_checked && b.get()!= this){
            return b;
        }
    }
    return m_checked  ? getPtrT<LAbstractButton>() : nullptr;
}

void LAbstractButton::fixFocusPolicy()
{
    if (!m_autoExclusive){
        return;
    }

    std::list<std::shared_ptr<LAbstractButton> > buttonList = queryButtonList();
    for (const std::shared_ptr<LAbstractButton>& b : buttonList) {
        if (!b->isCheckable()){
            continue;
        }
        b->setFocusPolicy((LFocusPolicy) ((b.get() == this || !isCheckable())
                                              ? ((uint8_t)b->focusPolicy() | uint8_t(LFocusPolicy::Tab))
                                              :  ((uint8_t)b->focusPolicy() & ~uint8_t(LFocusPolicy::Tab))));
    }
}

void LAbstractButton::moveFocus(int key)
{
    std::list<std::shared_ptr<LAbstractButton> > buttonList = queryButtonList();
    bool exclusive = m_autoExclusive;
    std::shared_ptr<LWidget> f = LApplication::focusWidget();
    std::shared_ptr<LAbstractButton> fb = std::dynamic_pointer_cast<LAbstractButton>(f);
    if(fb == nullptr){
        return;
    }
    if (std::find(buttonList.begin(), buttonList.end(), fb) == buttonList.end()){
        return;
    }

    std::shared_ptr<LAbstractButton> candidate = nullptr;
    int bestScore = -1;
    LRect<int> target = f->rect().translated(f->mapToGlobal(LPoint<int>(0,0)));
    LPoint<int> goal = target.center();
    uint8_t focusFlag = LGuiApplication::tabAllWidgets() ? (uint8_t)LFocusPolicy::Tab : (uint8_t)LFocusPolicy::Strong;

    for (const std::shared_ptr<LAbstractButton>& button : buttonList) {
        if (button != f && button->window() == f->window() && button->isEnabled() && !button->isHidden() &&
            (exclusive || ((uint8_t)button->focusPolicy() & focusFlag) == focusFlag)) {
            LRect<int> buttonRect = button->rect().translated(button->mapToGlobal(LPoint<int>(0,0)));
            LPoint<int> p = buttonRect.center();

            //Priority to widgets that overlap on the same coordinate.
            //In that case, the distance in the direction will be used as significant score,
            //take also in account orthogonal distance in case two widget are in the same distance.
            int score;
            if ((buttonRect.m_x < target.x2() && target.m_x < buttonRect.x2())
                && (key == (int)LKey::Up || key == (int)LKey::Down)) {
                //one item's is at the vertical of the other
                score = (std::abs(p.m_y - goal.m_y) << 16) + std::abs(p.m_x - goal.m_x);
            } else if ((buttonRect.m_y < target.y2() && target.m_y < buttonRect.y2())
                       && (key == (int)LKey::Left || key == (int)LKey::Right) ) {
                //one item's is at the horizontal of the other
                score = (std::abs(p.m_x - goal.m_x) << 16) + std::abs(p.m_y - goal.m_y);
            } else {
                score = (1 << 30) + (p.m_y - goal.m_y) * (p.m_y - goal.m_y) + (p.m_x - goal.m_x) * (p.m_x - goal.m_x);
            }

            if (score > bestScore && candidate){
                continue;
            }

            switch(key) {
            case (int)LKey::Up:{
                if (p.m_y < goal.m_y) {
                    candidate = button;
                    bestScore = score;
                }
                break;
            }
            case (int)LKey::Down:{
                if (p.m_y > goal.m_y) {
                    candidate = button;
                    bestScore = score;
                }
                break;
            }
            case (int)LKey::Left:{
                if (p.m_x < goal.m_x) {
                    candidate = button;
                    bestScore = score;
                }
                break;
            }
            case (int)LKey::Right:{
                if (p.m_x > goal.m_x) {
                    candidate = button;
                    bestScore = score;
                }
                break;
            }
            default:{
                break;
            }
            }
        }
    }



    if (candidate != nullptr) {
        if (exclusive
            && fb->m_checked
            && candidate->m_checkable){
            candidate->click();
        }

        if (key == (int)LKey::Up || key == (int)LKey::Left){
            candidate->setFocus(LFocusReason::Backtab);
        }
        else{
            candidate->setFocus(LFocusReason::Tab);
        }
    }
}


LK_NAMESPACE_END
