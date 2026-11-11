#include "ledititemview.h"
#include "lkwidgets/core/lapplication.h"
#include "lkwidgets/util/lmultitextlineeditor.h"
#include "lkwidgets/util/lmultilineeditor.h"
#include "lkwidgets/widget/lmenu.h"
#include "lkcore/io/li18n.h"

LK_NAMESPACE_BEGIN

LEditItemView::LEditItemView(std::shared_ptr<LWidget> parent) :LFrame(parent)
{
    m_editor = std::make_shared<LMultiTextLineEditor>();
    m_editor->setCursorVisible(false);
    m_sizePolicy = LSizePolicy(LSizePolicy::Expanding, LSizePolicy::Expanding, LSizePolicy::Edit);
}

LEditItemView::LEditItemView(std::shared_ptr<LIEditor> editor, std::shared_ptr<LWidget> parent)
    : LFrame(parent), m_editor(editor)
{
    if(editor == nullptr){
        m_editor = std::make_shared<LMultiTextLineEditor>();
    }

    m_editor->setCursorVisible(false);
    m_sizePolicy = LSizePolicy(LSizePolicy::Expanding, LSizePolicy::Expanding, LSizePolicy::Edit);
}

void LEditItemView::setIsReadOnly(bool v)
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

void LEditItemView::initSpecific()
{
    setAttribute(LWidgetAttribute::InputMethodEnabled, m_isReadOnly == false);
    m_editor->sigUpdate.connect(this, (void (LEditItemView::*)())(&LEditItemView::updateScrollBars), ConnectionType::Queued);
    setFocusPolicy(LFocusPolicy::Strong);
    setCursor(LCursorShape::IBeam);
}

LSize<int> LEditItemView::sizeHint() const
{
    m_editor->checkUpdate();

    LFontMetrics fm(font());
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    const LMargins<double>& tm = base->margins();
    const LMargins<int>& bounds = boundWidths();
    double x = fm.horizontalAdvance('x') * 17;

    double h = std::max<double>(base->fullHeight(), x + tm.m_top + tm.m_bottom) + bounds.m_top+bounds.m_bottom;
    double w = std::max<double>(base->fullWidth(), x + tm.m_left + tm.m_right) +  bounds.m_right+bounds.m_left;
    double lw = lineWidth();
    return LSize<double>(w+2*lw, h+2*lw).toSize();
}

LSize<int> LEditItemView::minimumSizeHint() const
{
    LFontMetrics fm(font());
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    const LMargins<double>& tm = base->margins();
     const LMargins<int>& bounds = boundWidths();

    double h = fm.height() + fm.leading() + tm.m_top + tm.m_bottom + bounds.m_top+bounds.m_bottom;
    double w = fm.maxWidth() + tm.m_left + tm.m_right +  bounds.m_right+bounds.m_left;
    double lw = lineWidth();
    return LSize<double>(w+2*lw, h+2*lw).toSize();
}

void LEditItemView::setEditor(std::shared_ptr<LIEditor> e)
{
    if(e != m_editor){
        if(m_editor != nullptr){
            m_editor->sigUpdate.disconnect(this);
        }

        m_editor = e;
        m_editor->setCursorVisible(false);
        updateScrollBars();
        m_editor->sigUpdate.connect(this, (void (LEditItemView::*)())(&LEditItemView::updateScrollBars));
    }
}

LPoint<int> LEditItemView::contentsOffset() const
{
    const uint16_t hmargin = boundWidth(LDirection::ToLeft);
    const uint16_t vmargin = boundWidth(LDirection::ToTop);

    std::shared_ptr<LAbstractScrollArea> scrollArea = parentT<LAbstractScrollArea>();
    if(scrollArea != nullptr){
        LPoint<int> ret = scrollArea->contentsOffset();
        ret.m_x -= hmargin;
        ret.m_y -= vmargin;
        return ret;
    }
    return LPoint<int>(hmargin, vmargin);
}

void LEditItemView::setCursorVisible(bool v)
{
    if(m_editor->isCursorVisible() != v)
    {
        m_editor->resetBlink();
        m_editor->setCursorVisible(v);
        update(adjustedControlRect(m_editor->cursorRect()));
    }
}

void LEditItemView::blink()
{
    if(m_editor->isCursorVisible()){
        m_editor->blink();
        update(adjustedControlRect(m_editor->cursorRect()));
    }else{
        setCursorVisible(true);
    }
}

void LEditItemView::deselect()
{
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    base->deSelect();
    update();
}

void LEditItemView::selectAll()
{
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    base->selectAll();
    update();
}

void LEditItemView::copy()
{
    std::wstring s;
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    if(base->isText()){
        std::shared_ptr<LEditText> text = std::dynamic_pointer_cast<LEditText>(base);
        s = text->selectText();
        if(!s.empty()){
            LGuiApplication::clipboard()->setText(s);
        }
    }else{
        LCbor cbor = base->toCbor(true);
        if(cbor.isValid()){
            LGuiApplication::clipboard()->setCbor(cbor, std::string(NAMEOF_SHORT_TYPE(LEditBase)));
        }
    }

}

void LEditItemView::cut()
{
    if(m_isReadOnly){
        return;
    }

    std::wstring s;
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    if(base->isText()){
        std::shared_ptr<LEditText> text = std::dynamic_pointer_cast<LEditText>(base);
        s = text->selectText();
        if(!s.empty()){
            LGuiApplication::clipboard()->setText(s);
            del(true);
        }
    }else{
        LCbor cbor = base->toCbor(true);
        if(cbor.isValid()){
            LGuiApplication::clipboard()->setCbor(cbor, std::string(NAMEOF_SHORT_TYPE(LEditBase)));
            del(true);
        }
    }
}

void LEditItemView::paste()
{
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    if(base->isText()){
        std::wstring s = LGuiApplication::clipboard()->text();
        if(!s.empty()){
            input(s);
        }
    }else{
        LCbor cbor = LGuiApplication::clipboard()->cbor(std::string(NAMEOF_SHORT_TYPE(LEditBase)));
        if(cbor.isValid()){
            inputCbor(cbor);
        }
    }
}

void LEditItemView::undo()
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

void LEditItemView::redo()
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

void LEditItemView::del(bool isForward)
{
    if(m_isReadOnly){
        return;
    }

    std::vector<int> b = m_editor->activeIndexes();
    std::list<std::shared_ptr<LEditOp> > ops = m_editor->doDelete(isForward);

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

void LEditItemView::input(const std::wstring &text)
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

void LEditItemView::inputCbor(const LCbor &cbor)
{
    if(m_isReadOnly){
        return;
    }

    std::vector<int> b = m_editor->activeIndexes();
    std::list<std::shared_ptr<LEditOp>> ops;
    if(m_editor->hasSelectContent()){
        ops = m_editor->doDelete();
    }
    ops.append_range(m_editor->doInputCbor(cbor));
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

int LEditItemView::maxLength() const
{
    return m_editor->maxLength();
}

void LEditItemView::setMaxLength(int len)
{
    m_editor->setMaxLength(len);
}

void LEditItemView::enableUndoStack(std::shared_ptr<LUndoStack> stack)
{
    if(stack == nullptr){
        m_undoStack = std::make_shared<LUndoStack>();
    }else{
        m_undoStack = stack;
    }
}

void LEditItemView::disableUndoStack()
{
    m_undoStack = nullptr;
}

bool LEditItemView::hasUndoStack() const
{
    return m_undoStack != nullptr;
}

void LEditItemView::clearUndoStack()
{
    if(m_undoStack != nullptr){
        m_undoStack->clear();
    }
}

std::wstring LEditItemView::text(std::wstring lineSeperator) const
{
    if(lineSeperator.empty()){
        return m_editor->content();
    }else{
        std::wstring temp = m_editor->content();
        std::wstring editpSep;
        editpSep.push_back(wchar_t(lchar::LineSeparator));
        temp = LStringUtil::replace(editpSep, lineSeperator, temp);
        return temp;
    }
}

void LEditItemView::setText(std::wstring &text)
{
    m_editor->setContent(text);
    if(m_undoStack != nullptr){
        m_undoStack->clear();
    }
}

std::wstring LEditItemView::selectText(std::wstring lineSeperator) const
{
    if(lineSeperator.empty()){
        return m_editor->selectContent();
    }else{
        std::wstring temp = m_editor->selectContent();
        std::wstring editpSep;
        editpSep.push_back(wchar_t(lchar::LineSeparator));
        temp = LStringUtil::replace(editpSep, lineSeperator, temp);
        return temp;
    }
}

LVariant LEditItemView::inputMethodQuery(LInputMethodQuery property, const LVariant &args) const
{
    switch(property) {
    case LInputMethodQuery::ImEnabled:{
        return isEnabled() && !isReadOnly();
    }
    case LInputMethodQuery::ImCursorRectangle:{
        return adjustedControlRect(m_editor->cursorRect()).toCbor();
    }
    case LInputMethodQuery::ImAnchorRectangle:{
        return adjustedControlRect(m_editor->anchorRect()).toCbor();
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
        return LVariant(maxLength());
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


void LEditItemView::paintEvent(std::shared_ptr<LPaintEvent> ev){

    m_editor->checkUpdate();
    LPainter p(paintDevice());
    std::shared_ptr<LPalette> pal = palette();

    LRect<double> r =  contentsRect().toRectF();

    if(isEnabled()){
        p.fillRect(r.adjusted(m_lineWidth, m_lineWidth, -m_lineWidth, -m_lineWidth), pal->brush(LPalette::Base));
    }
    std::shared_ptr<LStyle> pStyle = style();
    std::shared_ptr<LWidget> f = getPtrT<LWidget>();
    pStyle->drawFrame(f, &p, *this);

    int flags = (int)LEditBase::DrawFlags::Text;
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    if (base->hasSelect()){
        flags |= (int)LEditBase::DrawFlags::Selections;
    }

    m_editor->setPalette(pal);
    if (!m_isReadOnly){
        if(m_editor->isCursorVisible()){
            flags |= (int)LEditBase::DrawFlags::Cursor;
        }
    }

    m_editor->doDraw(&p, -contentsOffset().toPointF(), flags, r);
}


void LEditItemView::mousePressEvent(std::shared_ptr<LMouseEvent> e)
{
    if (e->button() == LMouseButton::Left){
        m_mousePressed = true;
        m_editor->doMouseClick(scrollEditPoint(e->localPos()));
    }
    e->accept();
}

void LEditItemView::mouseMoveEvent(std::shared_ptr<LMouseEvent> e)
{
    if (e->buttons() & LMouseButton::Left){
        if(m_mousePressed){
            m_editor->doMouseSelectMove(scrollEditPoint(e->localPos()));
        }
    }
    e->accept();
}

void LEditItemView::mouseReleaseEvent(std::shared_ptr<LMouseEvent> e)
{
    if (e->button() == LMouseButton::Left){
        if(m_mousePressed){
            m_editor->doMouseRelease(scrollEditPoint(e->localPos()));
            if (!isReadOnly() && rect().contains(e->pos())){
                handleSoftwareInputPanel(e->button(), m_mousePressed);
            }
        }
    }
    m_mousePressed = false;
    e->accept();
}

void LEditItemView::mouseDoubleClickEvent(std::shared_ptr<LMouseEvent> e)
{
    if (e->button() == LMouseButton::Left){
        m_editor->doMouseClick(scrollEditPoint(e->localPos()));
    }
    m_mousePressed = false;
    e->accept();
}

void LEditItemView::keyPressEvent(std::shared_ptr<LKeyEvent> e)
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

void LEditItemView::keyReleaseEvent(std::shared_ptr<LKeyEvent> ev)
{
    if (!isReadOnly()){
        handleSoftwareInputPanel();
    }
}

void LEditItemView::inputMethodEvent(std::shared_ptr<LInputMethodEvent> e)
{
    input(e->commitString());
    e->accept();
}


void LEditItemView::focusInEvent(std::shared_ptr<LFocusEvent> e)
{
    setCursorVisible(true);
    e->accept();
}

void LEditItemView::focusOutEvent(std::shared_ptr<LFocusEvent> e)
{
    std::shared_ptr<LAbstractScrollArea> scrollArea = parentT<LAbstractScrollArea>();
    if(scrollArea != nullptr){
        std::shared_ptr<LWidget> focus = LApplication::focusWidget();
        while(focus != nullptr){
            if(focus == scrollArea){
                return;
            }
            focus = focus->parentT<LWidget>();
        }

        setCursorVisible(false);
    }
    e->accept();
}

void LEditItemView::contextMenuEvent(std::shared_ptr<LContextMenuEvent> e)
{
    std::shared_ptr<LMenu> menu = createContextMenu();
    if(menu != nullptr){
        menu->setAttribute(LWidgetAttribute::ReleaseParentOnClose);
        menu->popup(e->globalPos());
    }
    e->accept();
}

std::shared_ptr<LMenu> LEditItemView::createContextMenu()
{
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    std::shared_ptr<LMenu> menu = makeInit<LMenu>(q);

    std::shared_ptr<LSignalAction> selectAll = std::make_shared<LSignalAction>(LI18n::tr(L"全选")
                                                                               +L"\t" + LKeySequence(LKeySequence::SelectAll).toString());
    menu->addAction(selectAll);
    selectAll->sigTrigger.connect(this, &LEditItemView::selectAll);
    selectAll->setEabled(m_editor->hasContent());

    std::shared_ptr<LSignalAction> copy = std::make_shared<LSignalAction>(LI18n::tr(L"复制")
                                                                          +L"\t" + LKeySequence(LKeySequence::Copy).toString());
    menu->addAction(copy);
    copy->sigTrigger.connect(this, &LEditItemView::copy);
    copy->setEabled(m_editor->hasSelectContent());

    if(m_isReadOnly == false){
        std::shared_ptr<LSignalAction> cut = std::make_shared<LSignalAction>(LI18n::tr(L"剪切")
                                                                             +L"\t" + LKeySequence(LKeySequence::Cut).toString());
        menu->addAction(cut);
        cut->sigTrigger.connect(this, &LEditItemView::cut);
        cut->setEabled(m_editor->hasSelectContent());

        std::shared_ptr<LSignalAction> del = std::make_shared<LSignalAction>(LI18n::tr(L"删除")
                                                                             +L"\t" + LKeySequence(LKeySequence::Delete).toString());
        menu->addAction(del);
        del->sigTrigger.connect(this, &LEditItemView::del);
        del->setEabled(m_editor->hasSelectContent());

        std::shared_ptr<LSignalAction> paste = std::make_shared<LSignalAction>(LI18n::tr(L"粘贴")
                                                                               +L"\t" + LKeySequence(LKeySequence::Paste).toString());
        menu->addAction(paste);
        paste->sigTrigger.connect(this, &LEditItemView::paste);

        if(m_undoStack != nullptr){
            std::shared_ptr<LSignalAction> undo = std::make_shared<LSignalAction>(LI18n::tr(L"撤消")
                                                                                  +L"\t" + LKeySequence(LKeySequence::Undo).toString());
            menu->addAction(undo);
            undo->sigTrigger.connect(this, &LEditItemView::undo);
            undo->setEabled(m_undoStack->canUndo());

            std::shared_ptr<LSignalAction> redo = std::make_shared<LSignalAction>(LI18n::tr(L"恢复")
                                                                                  +L"\t" + LKeySequence(LKeySequence::Redo).toString());
            menu->addAction(redo);
            paste->sigTrigger.connect(this, &LEditItemView::redo);
            redo->setEabled(m_undoStack->canUndo());
        }
    }
    return menu;
}

bool LEditItemView::checkOpResult()
{
    return true;
}


std::shared_ptr<LEditText> LEditItemView::editText() const
{
    LIndexItem item = m_editor->indexItem(m_editor->activeIndexes());
    if(item.baseItem != nullptr){
        if(item.baseItem->isText()){
            return std::dynamic_pointer_cast<LEditText>(item.baseItem);
        }
    }
    return nullptr;
}


bool LEditItemView::cursorMoveKeyEvent(std::shared_ptr<LKeyEvent> e)
{
    if(!isEnabled()){
        return false;
    }
    return m_editor->doMovePosition(e);
}

LRect<int> LEditItemView::adjustedControlRect(const LRect<double> &r) const
{
    if(r.isEmpty()){
        return rect();
    }else{
        return r.translated(-contentsOffset().toPointF()).toAlignedRect();
    }
}

LPoint<double> LEditItemView::scrollEditPoint(const LPoint<double> &p) const
{
    LPoint<int> offset = contentsOffset();
    return LPoint<double>(p.m_x+offset.m_x, p.m_y+offset.m_y);
}

void LEditItemView::rebuildScrollBars()
{
    std::shared_ptr<LAbstractScrollArea> scrollArea = parentT<LAbstractScrollArea>();
    if(scrollArea != nullptr){

        LSize<int> p = size();
        LSize<int> m = scrollArea->maximumViewportSize();
        LSize<int> v = sizeHint();
        if(m.expandedTo(v) == m){
            p = m; // no scroll bars needed
        }

        std::shared_ptr<LEditBase> base = m_editor->editBase();
        int scrollStep = base->fullHeight()/m_editor->contentRowNum();

        std::shared_ptr<LScrollBar> bar = scrollArea->horizontalScrollBar();
        bar->setRange(0, v.width() - p.width());
        bar->setPageStep(p.width());
        bar->setSingleStep(scrollStep);

        bar = scrollArea->verticalScrollBar();
        bar->setRange(0, v.height() - p.height());
        bar->setPageStep(p.height());
        bar->setSingleStep(scrollStep);
    }
}

void LEditItemView::updateScrollBars(bool force)
{
    if(force || m_editor->checkUpdate()){
        rebuildScrollBars();
        updateEditPosition();
    }
    update();
}

void LEditItemView::updateEditPosition()
{
    std::shared_ptr<LAbstractScrollArea> scrollArea = parentT<LAbstractScrollArea>();
    if(scrollArea != nullptr){
        LRect<int> eRect =  adjustedControlRect(m_editor->cursorRect());
        if(eRect.isEmpty()){
            return;
        }
        LRect<int> viewRect = contentsRect();
        if(viewRect.isEmpty()){
            return;
        }

        if(!viewRect.contains(eRect)){
            const uint16_t hmargin = boundWidth(LDirection::ToLeft);
            const uint16_t vmargin = boundWidth(LDirection::ToTop);

            if(eRect.m_x < viewRect.m_x + hmargin){
                std::shared_ptr<LScrollBar> bar = scrollArea->horizontalScrollBar();
                bar->setSliderPosition(bar->sliderPosition() + eRect.m_x - viewRect.m_x + hmargin);

            }else if((eRect.x2() + hmargin) > (viewRect.x2()-hmargin)){
                std::shared_ptr<LScrollBar> bar = scrollArea->horizontalScrollBar();
                bar->setSliderPosition(bar->sliderPosition() + eRect.x2() - viewRect.x2()+ 2*hmargin + eRect.width());
            }

            if(eRect.m_y < viewRect.m_y + vmargin){
                std::shared_ptr<LScrollBar> bar = scrollArea->verticalScrollBar();
                bar->setSliderPosition(bar->sliderPosition() + eRect.m_y - viewRect.m_y + vmargin);

            }else if((eRect.y2()+vmargin) > viewRect.y2()-vmargin){
                std::shared_ptr<LScrollBar> bar = scrollArea->verticalScrollBar();
                bar->setSliderPosition(bar->sliderPosition() + eRect.y2() - viewRect.y2()+2*vmargin + eRect.height());
            }
        }
    }
}

LK_NAMESPACE_END
