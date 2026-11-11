#include "labstractlineedit.h"
#include "lkwidgets/draw/lstyle.h"
#include "lkwidgets/core/lapplication.h"
#include "lkwidgets/widget/lmenu.h"
#include "lkcore/io/li18n.h"

LK_NAMESPACE_BEGIN

LAbstractLineEdit::LAbstractLineEdit(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f): LFrame(parent, f)
{
}

int LAbstractLineEdit::maxLength() const
{
    return m_editor->maxLength();
}

void LAbstractLineEdit::setMaxLength(int len)
{
    m_editor->setMaxLength(len);
}

void LAbstractLineEdit::setTextMargins(int left, int top, int right, int bottom)
{
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    base->setMargins(LMargins<double>(left, top, right, bottom));
    updateGeometry();
    update();
}

void LAbstractLineEdit::setTextMargins(const LMargins<int> &margins)
{
    setTextMargins(margins.m_left, margins.m_top, margins.m_right, margins.m_bottom);
}

void LAbstractLineEdit::setIsReadOnly(bool v)
{
    if(((bool)m_isReadOnly) != v){
        m_isReadOnly = v? 1: 0;
        setAttribute(LWidgetAttribute::InputMethodEnabled, m_isReadOnly == false);
        setCursor(m_isReadOnly ? LCursorShape::Arrow : LCursorShape::IBeam);
        std::shared_ptr<LEvent> event = std::make_shared<LEvent>(LEvent::ReadOnlyChange);
        std::shared_ptr<LObject> q = getPtrT<LObject>();
        LCoreApplication::sendEvent(q, event);
        update();
    }
}

void LAbstractLineEdit::setCursorVisible(bool v)
{
    if(m_editor->isCursorVisible() != v)
    {
        m_editor->resetBlink();
        m_editor->setCursorVisible(v);
        update(adjustedEditRect(m_editor->cursorRect()));
    }
}

LRect<int> LAbstractLineEdit::cursorRect() const
{
    return adjustedEditRect(m_editor->cursorRect());
}

void LAbstractLineEdit::mousePressEvent(std::shared_ptr<LMouseEvent> e)
{
    if(e->button() == LMouseButton::Left){
        m_mousePressed = true;
        m_editor->doMouseClick(scrollEditPoint(e->localPos()));
    }
    e->accept();
}

void LAbstractLineEdit::mouseMoveEvent(std::shared_ptr<LMouseEvent> e)
{
    if (e->buttons() & LMouseButton::Left){
        if(m_mousePressed){
            m_editor->doMouseSelectMove(scrollEditPoint(e->localPos()));
        }
    }
    e->accept();
}

void LAbstractLineEdit::mouseReleaseEvent(std::shared_ptr<LMouseEvent> e)
{
    if (e->button() == LMouseButton::Left){
        if(m_mousePressed){
            m_editor->doMouseRelease(scrollEditPoint(e->localPos()));
            if (!isReadOnly() && editRect().contains(e->pos())){
                handleSoftwareInputPanel(e->button(), m_mousePressed);
            }
        }
    }
    m_mousePressed = false;
    e->accept();
}

void LAbstractLineEdit::mouseDoubleClickEvent(std::shared_ptr<LMouseEvent> e)
{
    if (e->button() == LMouseButton::Left){
        m_editor->doMouseClick(scrollEditPoint(e->localPos()));
        m_mousePressed = false;
    }
    e->accept();
}

void LAbstractLineEdit::keyPressEvent(std::shared_ptr<LKeyEvent> e)
{
    if(!isEnabled()){
        return;
    }

    if (e->matches(LKeySequence::SelectAll)) {
        e->accept();
        selectAll();
        return;
    }
    else if (e->matches(LKeySequence::Copy)) {
        e->accept();
        copy();
        return;
    }

    if (cursorMoveKeyEvent(e)){
        goto accept;
    }

    if (isReadOnly()) {
        e->ignore();
        return;
    }

    if (e->key() == (uint32_t)LKey::Backspace) {
        del(false);
        goto accept;
    }

    if (e->key() == (uint32_t)LKey::Delete) {
        del(true);
        goto accept;
    }

    if (e->matches(LKeySequence::Undo)) {
        undo();
    }
    else if (e->matches(LKeySequence::Redo)) {
        redo();
    }
    else if (e->matches(LKeySequence::Cut)) {
        cut();
    }
    else if (e->matches(LKeySequence::Paste)) {
        paste();
    }
    else if (e->matches(LKeySequence::Delete)
               || e->matches(LKeySequence::DeleteEndOfWord)
               || e->matches(LKeySequence::DeleteEndOfLine)){
        del(true);
    }else if(e->matches(LKeySequence::Backspace)
               || e->matches(LKeySequence::DeleteStartOfWord)) {
        del(false);
    }
    else {
        goto process;
    }
    goto accept;

process:
{
    if (m_editor->isAcceptableInput(e)) {
        input(e->text());
    } else {
        e->ignore();
        return;
    }
}

accept:
    e->accept();
}


void LAbstractLineEdit::keyReleaseEvent(std::shared_ptr<LKeyEvent> ev)
{
    if (!isReadOnly()){
        handleSoftwareInputPanel();
    }
}

void LAbstractLineEdit::focusInEvent(std::shared_ptr<LFocusEvent> e)
{
    setCursorVisible(true);
    e->accept();
}

void LAbstractLineEdit::focusOutEvent(std::shared_ptr<LFocusEvent> e)
{
    deselect();
    setCursorVisible(false);
    e->accept();
}

void LAbstractLineEdit::paintEditLine(LPainter& p, std::shared_ptr<LPalette> pal)
{
    m_editor->checkUpdate();
    LRect<double> r =  editRect().toRectF();
    LFontMetrics fm = fontMetrics();
    uint32_t va = m_align;

    switch (va & (uint32_t)LAlignment::VerticalMask) {
    case (uint32_t)LAlignment::Bottom:{
        m_vscroll = r.height() - fm.height()-1;
        break;
    }
    case (uint32_t)LAlignment::Top:{
        m_vscroll = 0;
        break;
    }
    default:{
        //center
        m_vscroll = (r.height() - fm.height() + 1) / 2;
        break;
    }
    }
    LRect<double> lineRect(r.m_x,  r.m_y + m_vscroll, r.width(), fm.height()+1, false);


    if (m_isShowPlaceholderText && m_editor->isContentEmpty()) {
        if (!m_placeholderText.empty()) {
            const LColor col = pal->placeholderText()->m_color;
            const LColor oldColor = p.pen()->color();
            p.setPen(col);

            const std::wstring elidedText = fm.elidedText(m_placeholderText, LTextElide::Right, lineRect.width());
            p.drawText(lineRect, elidedText, m_align);
            p.setPen(oldColor);
        }
    }

    double cix = 0;
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    if(base->isText()){
        std::shared_ptr<LMultiTextLineEditor> text = std::dynamic_pointer_cast<LMultiTextLineEditor>(base);
        cix = text->cursorToX();
    }else{
        std::shared_ptr<LLineEditor> row = std::dynamic_pointer_cast<LLineEditor>(base);
        cix = row->cursorToX();
    }

    double widthUsed = base->fullWidth() + 1;
    if (widthUsed <= lineRect.width()) {
        // text fits in lineRect; use hscroll for alignment
        switch (va & (uint32_t)LAlignment::HorizontalMask) {
        case (uint32_t)LAlignment::Right:{
            m_hscroll = widthUsed - lineRect.width() + 1;
            break;
        }
        case (uint32_t)LAlignment::HCenter:{
            m_hscroll = (widthUsed - lineRect.width()) / 2;
            break;
        }
        default:{
            m_hscroll = 0;
            break;
        }
        }
    } else if (cix - m_hscroll >= lineRect.width()) {
        // text doesn't fit, cursor is to the right of lineRect (scroll right)
        m_hscroll = cix - lineRect.width() + 1;
    } else if (cix - m_hscroll < 0 && m_hscroll < widthUsed) {
        // text doesn't fit, cursor is to the left of lineRect (scroll left)
        m_hscroll = cix;
    } else if (widthUsed - m_hscroll < lineRect.width()) {
        // text doesn't fit, text document is to the left of lineRect; align
        // right
        m_hscroll = widthUsed - lineRect.width() + 1;
    } else {
        //in case the text is bigger than the lineedit, the hscroll can never be negative
        m_hscroll = std::max<double>(0, m_hscroll);
    }

    LPoint<double> topLeft = lineRect.topLeft()-LPoint<double>(m_hscroll, 0);

    bool isLineEdit = m_sizePolicy.controlType() == LSizePolicy::LineEdit;
    uint16_t state = uiElementState(isLineEdit ? LUiElementType::None: LUiElementType::Text);
    bool hasColor = false;
    std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(isLineEdit ? LUiElementType::None: LUiElementType::Text, state);
    if(rule != nullptr){
        hasColor = rule->applyForeground(&p) & 0x01;
    }

    if(!hasColor){
        p.setPen(pal->text()->m_color);
    }

    int flags = (int)LEditBase::DrawFlags::Text;
    if (m_editor->hasSelectContent()){
        flags |= (int)LEditBase::DrawFlags::Selections;
    }

    m_editor->setPalette(pal);
    if (!m_isReadOnly && (LApplication::focusWidget().get() == this)){
        flags |= (int)LEditBase::DrawFlags::Cursor;
    }
    m_editor->doDraw(&p, topLeft, flags, r);
}

void LAbstractLineEdit::inputMethodEvent(std::shared_ptr<LInputMethodEvent> e)
{
    input(e->commitString());
    e->accept();
}

void LAbstractLineEdit::contextMenuEvent(std::shared_ptr<LContextMenuEvent> e)
{
    std::shared_ptr<LMenu> menu = createContextMenu();
    if(menu != nullptr){
        menu->setAttribute(LWidgetAttribute::ReleaseParentOnClose);
        menu->popup(e->globalPos());
    }
    e->accept();
}

std::shared_ptr<LMenu> LAbstractLineEdit::createContextMenu()
{
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    std::shared_ptr<LMenu> menu = makeInit<LMenu>(q);

    std::shared_ptr<LSignalAction> selectAll = std::make_shared<LSignalAction>(LI18n::tr(L"全选")
                                                                               +L"\t" + LKeySequence(LKeySequence::SelectAll).toString());
    menu->addAction(selectAll);
    selectAll->sigTrigger.connect(this, &LAbstractLineEdit::selectAll);
    selectAll->setEabled(m_editor->hasContent());

    std::shared_ptr<LSignalAction> copy = std::make_shared<LSignalAction>(LI18n::tr(L"复制")
                                                                          +L"\t" + LKeySequence(LKeySequence::Copy).toString());
    menu->addAction(copy);
    copy->sigTrigger.connect(this, &LAbstractLineEdit::copy);
    copy->setEabled(m_editor->hasSelectContent());

    if(m_isReadOnly == false){
        std::shared_ptr<LSignalAction> cut = std::make_shared<LSignalAction>(LI18n::tr(L"剪切")
                                                                             +L"\t" + LKeySequence(LKeySequence::Cut).toString());
        menu->addAction(cut);
        cut->sigTrigger.connect(this, &LAbstractLineEdit::cut);
        cut->setEabled(m_editor->hasSelectContent());

        std::shared_ptr<LSignalAction> del = std::make_shared<LSignalAction>(LI18n::tr(L"删除")
                                                                             +L"\t" + LKeySequence(LKeySequence::Delete).toString());
        menu->addAction(del);
        del->sigTrigger.connect(this, &LAbstractLineEdit::del);
        del->setEabled(m_editor->hasSelectContent());

        std::shared_ptr<LSignalAction> paste = std::make_shared<LSignalAction>(LI18n::tr(L"粘贴")
                                                                               +L"\t" + LKeySequence(LKeySequence::Paste).toString());
        menu->addAction(paste);
        paste->sigTrigger.connect(this, &LAbstractLineEdit::paste);

        if(m_undoStack != nullptr){
            std::shared_ptr<LSignalAction> undo = std::make_shared<LSignalAction>(LI18n::tr(L"撤消")
                                                                                  +L"\t" + LKeySequence(LKeySequence::Undo).toString());
            menu->addAction(undo);
            undo->sigTrigger.connect(this, &LAbstractLineEdit::undo);
            undo->setEabled(m_undoStack->canUndo());

            std::shared_ptr<LSignalAction> redo = std::make_shared<LSignalAction>(LI18n::tr(L"恢复")
                                                                                  +L"\t" + LKeySequence(LKeySequence::Redo).toString());
            menu->addAction(redo);
            paste->sigTrigger.connect(this, &LAbstractLineEdit::redo);
            redo->setEabled(m_undoStack->canUndo());
        }
    }
    return menu;
}

void LAbstractLineEdit::initSpecific()
{
    m_editor->sigUpdate.connect(this, (void (LAbstractLineEdit::*)())(&LAbstractLineEdit::update), ConnectionType::Queued);
    m_lineWidth = 1;
    m_shape = Panel;
    m_shadow = Sunken;
    setFocusPolicy(LFocusPolicy::Strong);
    setCursor(LCursorShape::IBeam);
}

bool LAbstractLineEdit::checkOpResult()
{
    return true;
}

LRect<int> LAbstractLineEdit::adjustedEditRect(const LRect<double> &r) const
{
    if(r.isEmpty()){
        return rect();
    }else{
        LRect<int> erect = editRect();
        double cix = erect.m_x - m_hscroll;
        double ciy = erect.m_y + m_vscroll;
        return r.translated(LPoint<double>(cix, ciy)).toAlignedRect();
    }
}

LPoint<double> LAbstractLineEdit::scrollEditPoint(const LPoint<double> &p) const
{
    LRect<int> erect = editRect();
    return LPoint<double>(p.m_x-erect.m_x+m_hscroll, 0);
}

bool LAbstractLineEdit::cursorMoveKeyEvent(std::shared_ptr<LKeyEvent> e)
{
    if(!isEnabled()){
        return false;
    }
    return m_editor->doMovePosition(e);
}

std::shared_ptr<LEditText> LAbstractLineEdit::editText() const
{
    LIndexItem item = m_editor->indexItem(m_editor->activeIndexes());
    if(item.baseItem != nullptr){
        if(item.baseItem->isText()){
            return std::dynamic_pointer_cast<LEditText>(item.baseItem);
        }
    }
    return nullptr;
}


void LAbstractLineEdit::blink()
{
    if(m_editor->isCursorVisible()){
        m_editor->blink();
        update(adjustedEditRect(m_editor->cursorRect()));
    }
}

void LAbstractLineEdit::deselect()
{
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    base->deSelect();
    update();
}

void LAbstractLineEdit::selectAll()
{
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    base->selectAll();
    update();
}

void LAbstractLineEdit::copy()
{
    std::wstring s;
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    if(base->isText()){
        std::shared_ptr<LMultiTextLineEditor> text = std::dynamic_pointer_cast<LMultiTextLineEditor>(base);
        s = text->selectText();
    }else{
        std::shared_ptr<LLineEditor> row = std::dynamic_pointer_cast<LLineEditor>(base);
        s = row->selectText();
    }

    if(!s.empty()){
        LGuiApplication::clipboard()->setText(s);
    }
}

void LAbstractLineEdit::cut()
{
    if(m_isReadOnly){
        return;
    }

    std::wstring s;
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    if(base->isText()){
        std::shared_ptr<LMultiTextLineEditor> text = std::dynamic_pointer_cast<LMultiTextLineEditor>(base);
        s = text->selectText();
    }else{
        std::shared_ptr<LLineEditor> row = std::dynamic_pointer_cast<LLineEditor>(base);
        s = row->selectText();
    }

    if(!s.empty()){
        LGuiApplication::clipboard()->setText(s);
        del(true);
    }
}

void LAbstractLineEdit::paste()
{
    if(m_isReadOnly){
        return;
    }

    std::wstring s = LGuiApplication::clipboard()->text();
    if(!s.empty()){
        input(s);
    }
}

void LAbstractLineEdit::undo()
{
    if(m_isReadOnly){
        return;
    }

    if(m_undoStack != nullptr){
        if(m_undoStack->canUndo()){
            m_undoStack->undo(m_editor);
            if(checkOpResult() == false){
                m_undoStack->redo(m_editor);
                m_undoStack->clear();
            }
        }
    }
}

void LAbstractLineEdit::redo()
{
    if(m_isReadOnly){
        return;
    }

    if(m_undoStack != nullptr){
        if(m_undoStack->canRedo()){
            m_undoStack->redo(m_editor);
            if(checkOpResult() == false){
                m_undoStack->undo(m_editor);
                m_undoStack->clear();
            }
        }
    }
}


void LAbstractLineEdit::del(bool isForward)
{
    if(m_isReadOnly){
        return;
    }

    std::vector<int> b = m_editor->activeIndexes();
    std::list<std::shared_ptr<LEditOp>> ops = m_editor->doDelete(isForward);
    if(!ops.empty()){
        std::vector<int> e = m_editor->activeIndexes();
        std::shared_ptr<LMultiEditOp> cmd = std::make_shared<LMultiEditOp>(b, e, ops, true);
        if(checkOpResult()){
            if(m_undoStack != nullptr){
                m_undoStack->push(cmd);
            }
        }else{
            cmd->undo(m_editor);
        }
    }
}

void LAbstractLineEdit::input(const std::wstring &text)
{
    if(m_isReadOnly){
        return;
    }

    std::vector<int> b = m_editor->activeIndexes();
    std::list<std::shared_ptr<LEditOp>> ops;
    if(m_editor->hasSelectContent()){
        ops = m_editor->doDelete();
    }
    ops.append_range(m_editor->doInput(text));
    if(!ops.empty()){
        std::vector<int> e = m_editor->activeIndexes();
        std::shared_ptr<LMultiEditOp> cmd = std::make_shared<LMultiEditOp>(b, e, ops, true);
        if(checkOpResult()){
            if(m_undoStack != nullptr){
                m_undoStack->push(cmd);
            }
        }else{
            cmd->undo(m_editor);
        }
    }
}

void LAbstractLineEdit::enableUndoStack(std::shared_ptr<LUndoStack> stack)
{
    if(stack == nullptr){
        m_undoStack = std::make_shared<LUndoStack>();
    }else{
        m_undoStack = stack;
    }
}

void LAbstractLineEdit::disableUndoStack()
{
    m_undoStack = nullptr;
}

bool LAbstractLineEdit::hasUndoStack() const
{
    return m_undoStack != nullptr;
}

void LAbstractLineEdit::clearUndoStack()
{
    if(m_undoStack != nullptr){
        m_undoStack->clear();
    }
}

LVariant LAbstractLineEdit::inputMethodQuery(LInputMethodQuery property, const LVariant &args) const
{
    switch(property) {
    case LInputMethodQuery::ImEnabled:{
        return isEnabled() && !isReadOnly();
    }
    case LInputMethodQuery::ImCursorRectangle:{
        return adjustedEditRect(m_editor->cursorRect()).toCbor();
    }
    case LInputMethodQuery::ImAnchorRectangle:{
        return adjustedEditRect(m_editor->anchorRect()).toCbor();
    }
    case LInputMethodQuery::ImFont:{
        std::shared_ptr<LIVar> v = font();
        return v;
    }
    case LInputMethodQuery::ImAbsolutePosition:
    case LInputMethodQuery::ImCursorPosition: {
        std::shared_ptr<LEditText> t = editText();
        if(t != nullptr){
            const LCbor* cbor = args.getCbor();
            if(cbor != nullptr){
                LPoint<double> pt;
                pt.fromCbor(cbor);
                pt = scrollEditPoint(pt);
                return t->xToPos(pt.m_x, LTextLine::CursorBetweenCharacters);
            }else{
                return LVariant(t->activePos());
            }
        }
        return LVariant();
    }
    case LInputMethodQuery::ImSurroundingText:{
        std::shared_ptr<LEditText> t = editText();
        if(t != nullptr){
            return LVariant(t->text());
        }
        return LVariant();
    }
    case LInputMethodQuery::ImCurrentSelection:{
        std::shared_ptr<LEditText> t = editText();
        if(t != nullptr){
            return LVariant(t->selectText());
        }
        return LVariant();
    }
    case LInputMethodQuery::ImMaximumTextLength:{
        return LVariant(m_editor->maxLength());
    }
    case LInputMethodQuery::ImAnchorPosition:{
        std::shared_ptr<LEditText> t = editText();
        if(t != nullptr){
            if (t->hasSelect() == false){
                return LVariant(t->activePos());
            }else{
                return LVariant(t->partSelectStartId());
            }
        }
        return LVariant();
    }
    case LInputMethodQuery::ImReadOnly:{
        return isReadOnly();
    }
    case LInputMethodQuery::ImTextBeforeCursor: {
        std::shared_ptr<LEditText> t = editText();
        if(t != nullptr){
            const LCbor* cbor = args.getCbor();
            if(cbor != nullptr){
                LPoint<double> pt;
                pt.fromCbor(cbor);
                pt = scrollEditPoint(pt);
                return t->text().substr(0, t->xToPos(pt.m_x, LTextLine::CursorBetweenCharacters));
            }else{
                return t->text().substr(0, t->activePos());
            }
        }
        return LVariant();
    }
    case LInputMethodQuery::ImTextAfterCursor: {
        std::shared_ptr<LEditText> t = editText();
        if(t != nullptr){
            const LCbor* cbor = args.getCbor();
            if(cbor != nullptr){
                LPoint<double> pt;
                pt.fromCbor(cbor);
                pt = scrollEditPoint(pt);
                return t->text().substr(t->xToPos(pt.m_x, LTextLine::CursorBetweenCharacters));
            }else{
                return t->text().substr(t->activePos());
            }
        }
        return LVariant();
    }
    default:{
        return LWidget::inputMethodQuery(property);
    }
    }
}

LK_NAMESPACE_END
