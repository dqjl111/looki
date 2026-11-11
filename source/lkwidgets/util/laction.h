#ifndef LACTION_H
#define LACTION_H
#include "lkcore/lsignal.h"
#include "lkgui/image/ltexture.h"
#include "lkwidgets/core/lwidget.h"
#include "lkgui/lshortcutman.h"
#include <list>

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LActionBase
{
public:
    LActionBase(bool isSep= false);
    LActionBase(const std::wstring& text,
            std::shared_ptr<LTexture> icon = nullptr,
            const LVariant& userData = LVariant());

    inline bool isSeparator() const{
        return m_isSeparator;
    }

    inline bool isVisible() const{
        return m_isVisible;
    }

    inline bool isEnabled() const{
        return m_isEnabled;
    }

    inline bool isPopup() const{
        return m_isPopup;
    }

    inline bool isCheckable() const{
        return m_isCheckable;
    }

    inline bool isChecked() const{
        return m_isCheckable ? m_isChecked : false;
    }

    inline bool isSelected() const{
        return m_isSelected;
    }

    inline bool isListItem() const{
        return m_isListItem;
    }

    inline bool isEmpty() const{
        return m_text.empty() || m_icon == nullptr;
    }

    virtual LFlags<LUiState> uiState() const;

    void setSeparator(bool v);
    void setVisible(bool v);
    void setEabled(bool v);
    void setCheckable(bool v);
    void setChecked(bool v);
    void setSelected(bool v);

    inline const std::wstring& text() const{
        return m_text;
    }

    inline const std::shared_ptr<LTexture> icon() const{
        return m_icon;
    }

    void setText(const std::wstring& s);
    void setIcon(const std::shared_ptr<LTexture> t);

    void setUserData(const LVariant& data);
    inline const LVariant& userData() const{
        return m_userData;
    }

    enum ActionEvent { Trigger, Hover, Pressed };

    virtual const LPoint<int> actPos() const;
    virtual void setActPos(const LPoint<int>& p);

    virtual void activate(ActionEvent events);
    virtual const LKeySequence shortcut() const;
    virtual void close(ActionEvent events);

    virtual const std::shared_ptr<LWidget> widget() const;
    inline bool hasWidget() const{
        return widget() != nullptr;
    }
    virtual void setWidget(const std::shared_ptr<LWidget> w, bool isPopup = true);
protected:
    union{
        uint8_t m_Flags = 0;
        struct{
            uint8_t m_isSeparator : 1;
            uint8_t m_isVisible : 1;
            uint8_t m_isEnabled : 1;
            uint8_t m_isCheckable : 1;
            uint8_t m_isSelected : 1;
            uint8_t m_isChecked : 1;
            uint8_t m_isPopup : 1;
            uint8_t m_isListItem : 1;
        };
    };

    std::wstring m_text;
    std::shared_ptr<LTexture> m_icon;
    LVariant m_userData;
};


class LKWIDGETSDEF LAction : public LObject, public LActionBase
{
public:
    LAction(const std::wstring& text,
            std::shared_ptr<LTexture> texture = nullptr);

    virtual ~LAction();

    static bool widgetShortcutContextMatcher(std::shared_ptr<LObject> object, LShortcutCtxType context);

    virtual const LKeySequence shortcut() const override;
    void setShortcut(const LKeySequence& seq,
                     LShortcutCtxType ctxType = LShortcutCtxType::Window,
                     uint32_t funID = 0,
                     LShortcutEntry::ContextMatcher matcher = widgetShortcutContextMatcher);

    const inline std::list<std::weak_ptr<LWidget> >& associatedWidgets() const{
        return m_associateWidgets;
    }

    void dissociate(std::shared_ptr<LWidget> w);
    void associate(std::shared_ptr<LWidget> w);


    virtual const std::shared_ptr<LWidget> widget() const override;
    virtual void setWidget(const std::shared_ptr<LWidget> w, bool isPopup = true) override;

protected:
    std::shared_ptr<LShortcutEntry> m_shortCut;
    std::list<std::weak_ptr<LWidget> > m_associateWidgets;
    std::shared_ptr<LWidget> m_widget;
};


class LKWIDGETSDEF LSignalAction : public LAction
{
public:
    LSignalAction(const std::wstring& text,
            std::shared_ptr<LTexture> texture = nullptr);

    LSignal<> sigTrigger;
    virtual void activate(ActionEvent event) override;
};

class LKWIDGETSDEF LCheckAction : public LAction
{
public:
    LCheckAction(const std::wstring& text,
            std::shared_ptr<LTexture> texture = nullptr);

    LSignal<bool> sigTrigger;
    virtual void activate(ActionEvent event) override;
};


LK_NAMESPACE_END

#endif // LACTION_H
