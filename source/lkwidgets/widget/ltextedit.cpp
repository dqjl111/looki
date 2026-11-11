#include "ltextedit.h"

LK_NAMESPACE_BEGIN

LTextEdit::LTextEdit(std::shared_ptr<LWidget> parent): LAbstractScrollArea(parent) {
    m_sizePolicy = LSizePolicy(LSizePolicy::Expanding, LSizePolicy::Expanding, LSizePolicy::Edit);
}

std::shared_ptr<LIEditor> LTextEdit::editor() const
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        return view->editor();
    }
    return nullptr;
}

void LTextEdit::setEditor(std::shared_ptr<LIEditor> e)
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        return view->setEditor(e);
    }
}

std::shared_ptr<LEditItemView> LTextEdit::editView()
{
    return std::dynamic_pointer_cast<LEditItemView>(m_viewport);
}

void LTextEdit::setText(std::wstring &text)
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        view->setText(text);
    }
}

std::wstring LTextEdit::text(std::wstring lineSeperator) const
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        return view->text(lineSeperator);
    }
    return L"";
}

std::wstring LTextEdit::selectText(std::wstring lineSeperator)
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        return view->selectText(lineSeperator);
    }
    return L"";
}

void LTextEdit::selectAll()
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        return view->selectAll();
    }
}

void LTextEdit::copy()
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        return view->copy();
    }
}

void LTextEdit::cut()
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        return view->cut();
    }
}

void LTextEdit::paste()
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        return view->paste();
    }
}

LVariant LTextEdit::inputMethodQuery(LInputMethodQuery property, const LVariant &args) const
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        return view->inputMethodQuery(property, args);
    }
    return LAbstractScrollArea::inputMethodQuery(property, args);
}

void LTextEdit::initViewport()
{
    std::shared_ptr<LWidget> q= getPtrT<LWidget>();
    m_viewport = makeInit<LEditItemView>(q);
    m_viewport->setBackgroundRole(LPalette::Base);
    m_viewport->setAutoFillBackground(true);
    m_viewport->addEventFilter(q);
}


bool LTextEdit::handleEvent(std::shared_ptr<LEvent> e)
{
    if (e->type() == LEvent::StyleChange || e->type() == LEvent::LayoutRequest) {
        std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
        if(view != nullptr){
            view->updateScrollBars(true);
        }
    }
    return LAbstractScrollArea::handleEvent(e);
}

void LTextEdit::resizeEvent(std::shared_ptr<LResizeEvent> e)
{
    std::shared_ptr<LEditItemView> view = std::dynamic_pointer_cast<LEditItemView>(m_viewport);
    if(view != nullptr){
        view->updateScrollBars(true);
    }
    e->accept();
}



LK_NAMESPACE_END
