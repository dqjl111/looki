#include "lcombobox.h"
#include "lkwidgets/draw/lstyle.h"

LK_NAMESPACE_BEGIN

LComboBoxItem::LComboBoxItem(const std::wstring &text, std::shared_ptr<LTexture> icon, const LVariant &userData)
    :LActionBase(text, icon, userData)
{
    m_isListItem = true;
}

void LComboBoxItem::activate(ActionEvent event)
{
    LActionBase::activate(event);
    if(event == Trigger){
        std::shared_ptr<LCombBoxBase> box = m_box.lock();
        if(box != nullptr){
            std::shared_ptr<LComboBoxItem> q = getPtrT<LComboBoxItem>();
            box->setCurrentItem(q);
        }
    }
}

void LComboBoxItem::close(ActionEvent event)
{
    if(event == Trigger){
        std::shared_ptr<LCombBoxBase> box = m_box.lock();
        if(box != nullptr){
            std::shared_ptr<LAbstractMenu> m = box->menu();
            if(m->canCloseAction()){
                std::shared_ptr<LComboBoxItem> item = getPtrT<LComboBoxItem>();
                box->removeItem(item);
            }
        }
    }
}


LCombBoxBase::LCombBoxBase()
{
}


void LCombBoxBase::addItem(std::shared_ptr<LComboBoxItem> item)
{
    if(item != nullptr){
        item->m_box = initItem(item);
        if(item->m_box.expired()){
            lWarn() <<"Add invalid LComboBoxItem";
            return;
        }
        menu()->addAction(item);
        if(m_currentItem == nullptr){
            setCurrentItem(item);
        }
    }
}

void LCombBoxBase::insertItem(int index, std::shared_ptr<LComboBoxItem> item)
{
    if(item != nullptr){
        item->m_box = initItem(item);
        if(item->m_box.expired()){
            lWarn() <<"Insert invalid LComboBoxItem";
            return;
        }
        menu()->insertAction(index, item);
        if(m_currentItem == nullptr){
            setCurrentItem(item);
        }
    }
}

void LCombBoxBase::setCurrentItem(std::shared_ptr<LComboBoxItem> item)
{
    if(item == nullptr){
        return;
    }

    if(item == m_currentItem){
        return;
    }

    m_currentItem = item;
    sigCurrentIndexChanged.emt(itemIndex(m_currentItem));
    currrentItemUpdate();
}


std::shared_ptr<LComboBoxItem> LCombBoxBase::addItem(const std::wstring &text,
                                                  std::shared_ptr<LTexture> icon,
                                                  const LVariant &userData)
{
    std::shared_ptr<LComboBoxItem> item = std::make_shared<LComboBoxItem>(text, icon, userData);
    addItem(item);
    return item;
}

std::shared_ptr<LComboBoxItem> LCombBoxBase::insertItem(int index,
                                                     const std::wstring &text,
                                                     std::shared_ptr<LTexture> icon,
                                                     const LVariant &userData)
{
    std::shared_ptr<LComboBoxItem> item = std::make_shared<LComboBoxItem>(text, icon, userData);
    insertItem(index, item);
    return item;
}

void LCombBoxBase::removeItem(std::shared_ptr<LComboBoxItem> item)
{
    std::shared_ptr<LAbstractMenu> m = menu();
    m->removeAction(item);
    if(m_currentItem == item){
        if(m->actions().empty()){
            m_currentItem = nullptr;
            sigCurrentIndexChanged.emt(-1);
            currrentItemUpdate();
        }else{
            setCurrentIndex(0);
        }
    }
}

LVariant LCombBoxBase::itemData(int index) const
{
    std::shared_ptr<LComboBoxItem> t = item(index);
    if(t == nullptr){
        return LVariant();
    }
    return t->userData();
}

const std::wstring &LCombBoxBase::itemText(int index) const
{
    std::shared_ptr<LComboBoxItem> t = item(index);
    if(t == nullptr){
        return LStringUtil::wempty;
    }
    return t->text();
}

std::shared_ptr<LTexture> LCombBoxBase::itemIcon(int index)
{
    std::shared_ptr<LComboBoxItem> t = item(index);
    if(t == nullptr){
        return nullptr;
    }
    return t->icon();
}


LComboBox::LComboBox(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f):LButton(LSizePolicy::ComboBox, parent, f) {
    m_hasMark = 1;

    m_menu = makeInit<LMenu>();
    m_menu->setUseScrollBar(true);
    m_menu->setBackgroundRole(LPalette::Base);
}

std::shared_ptr<LAbstractMenu> LComboBox::menu() const
{
    return m_menu;
}

uint16_t LComboBox::uiElementState(LUiElementType type) const
{
    if(m_menu->isVisible()){
        return uint16_t(LUiState::Sunken);
    }
    return LWidget::uiElementState(type);
}

void LComboBox::currrentItemUpdate()
{
    if(m_currentItem != nullptr){
        setIcon(m_currentItem->icon());
        setText(m_currentItem->text());
    }else{
        setIcon((std::shared_ptr<LTexture>)nullptr);
        setText(L"");
    }
}

std::shared_ptr<LCombBoxBase> LComboBox::initItem(std::shared_ptr<LComboBoxItem> item)
{
    return getPtrT<LComboBox>();
}

void LComboBox::setDown(bool down)
{
    if(m_menu != nullptr){
        if(!m_menu->isVisible()){
            if(m_menu->isScrollable()){
                m_menu->setMinimumWidth(width());
                m_menu->setMaximumWidth(width());
            }else{
                m_menu->setMaximumWidth(LWIDGETSIZE_MAX);
            }
        }

        if(m_menu->isVisible()){
            if(down){
                //第一次触发是菜单转过来的，关闭菜单后还会触发一次，借此标记，避免再次打开
                m_menu->setAttribute(LWidgetAttribute::NoMouseReplay);
                m_menu->close();
            }
        }else{
            if(down){
                std::shared_ptr<LWidget> w = getPtrT<LWidget>();
                m_menu->setMenuCause(w);
                m_menu->popup(m_menu->adjustedMenuPosition(w));
            }
        }
    }
    LButton::setDown(down);
}

void LComboBox::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    if(m_menu == nullptr){
        LButton::paintEvent(e);
        return;
    }

    if(m_shape == Box){
        if(m_menu->isVisible()){
            m_shadow = Raised;
        }else{
            m_shadow = Sunken;
        }
    }else{
        if(m_menu->isVisible()){
            m_shadow = Sunken;
        }else{
            m_shadow = Raised;
        }
    }

    LPainter painter(paintDevice());

    std::shared_ptr<LStyle> pStyle = style();
    std::shared_ptr<LWidget> f = getPtrT<LWidget>();
    pStyle->drawFrame(f, &painter, *this, true);
    pStyle->drawLabel(f, &painter, *this, m_menu->isScrollable()? LDrawMark::Down: LDrawMark::Right);
}

LK_NAMESPACE_END
