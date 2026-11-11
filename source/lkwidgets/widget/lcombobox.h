#ifndef LCOMBOBOX_H
#define LCOMBOBOX_H

#include "lkwidgets/widget/lbutton.h"
#include "lmenu.h"

LK_NAMESPACE_BEGIN

class LCombBoxBase;
class LKWIDGETSDEF LComboBoxItem : public LActionBase, public LIVar
{
public:
    LComboBoxItem(const std::wstring& text,
                  std::shared_ptr<LTexture> icon = nullptr,
                  const LVariant& userData = LVariant());

    virtual void activate(ActionEvent event) override;
    virtual void close(ActionEvent event) override;
protected:
    std::weak_ptr<LCombBoxBase> m_box;
    friend class LCombBoxBase;
};

class LKWIDGETSDEF LCombBoxBase
{
public:
    LCombBoxBase();


    std::shared_ptr<LComboBoxItem> addItem(const std::wstring& text,
                                           std::shared_ptr<LTexture> icon = nullptr,
                                           const LVariant& userData = LVariant());
    std::shared_ptr<LComboBoxItem> insertItem(int index,
                                              const std::wstring& text,
                                              std::shared_ptr<LTexture> icon = nullptr,
                                              const LVariant& userData = LVariant());

    void removeItem(std::shared_ptr<LComboBoxItem> item);
    inline void removeItem(int index){
        removeItem(item(index));
    }

    inline const std::shared_ptr<LComboBoxItem>& currentItem() const{
        return m_currentItem;
    }

    inline const LVariant currentData() const{
        return m_currentItem == nullptr? LVariant() : m_currentItem->userData();
    }

    inline const std::wstring& currentText() const{
        return m_currentItem == nullptr? LStringUtil::wempty : m_currentItem->text();
    }

    inline const std::shared_ptr<LTexture> currentIcon() const{
        return m_currentItem == nullptr? nullptr : m_currentItem->icon();
    }

    inline int itemIndex(std::shared_ptr<LComboBoxItem> item) const{
        return menu()->actionIndex(item);
    }

    inline std::shared_ptr<LComboBoxItem> item(int index) const{
        return std::dynamic_pointer_cast<LComboBoxItem>(menu()->action(index));
    }


    LVariant itemData(int index) const;
    const std::wstring& itemText(int index) const;
    std::shared_ptr<LTexture> itemIcon(int index);

    inline int currentIndex() const{
        return itemIndex(m_currentItem);
    }

    inline void setCurrentIndex(int index){
        setCurrentItem(item(index));
    }
    virtual std::shared_ptr<LAbstractMenu> menu() const = 0;

    LSignal<int> sigCurrentIndexChanged;
protected:
    void addItem(std::shared_ptr<LComboBoxItem> item);
    void insertItem(int index, std::shared_ptr<LComboBoxItem> item);
    void setCurrentItem(std::shared_ptr<LComboBoxItem> item);
    virtual void currrentItemUpdate(){}
    virtual std::shared_ptr<LCombBoxBase> initItem(std::shared_ptr<LComboBoxItem>) = 0;


    std::shared_ptr<LComboBoxItem> m_currentItem;

    friend class LComboBoxItem;
};

class LKWIDGETSDEF LComboBox  : public LButton, public LCombBoxBase
{
public:
    _LIVarTagEx(LComboBox, LWidget)

    LComboBox(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);
    virtual std::shared_ptr<LAbstractMenu> menu() const override;
    virtual uint16_t uiElementState(LUiElementType type = LUiElementType::None) const override;
protected:
    virtual void currrentItemUpdate() override;
    virtual std::shared_ptr<LCombBoxBase> initItem(std::shared_ptr<LComboBoxItem>) override;
    virtual void setDown(bool) override;
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;

    std::shared_ptr<LAbstractMenu> m_menu;

};


LK_NAMESPACE_END

#endif // LCOMBOBOX_H
