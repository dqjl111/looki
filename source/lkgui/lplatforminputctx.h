#ifndef LPLATFORMINPUTCTX_H
#define LPLATFORMINPUTCTX_H
#include "lkcore/lookiconfig.h"
#include "lkcore/lflags.h"
#include "lguienums.h"
#include "lkcore/lobject.h"
#include "lkgui/linputmethod.h"
LK_NAMESPACE_BEGIN

class LKGUIDEF LPlatformInputCtx
{
public:
    LPlatformInputCtx();
    enum Capability {
        HiddenTextCapability = 0x1
    };

    virtual void update(LFlags<LInputMethodQuery>){}
    static inline bool inputMethodAccepted(){
        return m_inputMethodAccepted;
    }

    static void setInputMethodAccepted(bool accepted){
        m_inputMethodAccepted = accepted;
    }

    virtual void setFocusObject(std::shared_ptr<LObject> object);
    virtual void reset();
    virtual void commit();
    virtual void showInputPanel();
    virtual void hideInputPanel();

    virtual bool hasCapability(uint32_t capability);
    virtual void invokeAction(LInputMethod::Action, int cursorPosition);
    virtual void cursorRectChanged();

    void handleLocaleChanged();

    virtual std::string localeLanguage() const;

    const inline LLayoutDirection inputDirection() const{
        return lchar::Util::isScriptLeftToRight(m_script) ? LLayoutDirection::LeftToRight : LLayoutDirection::RightToLeft;
    }
    const inline uint8_t script() const{
        return m_script;
    }
protected:
    static bool m_inputMethodAccepted;
    uint8_t m_script = lchar::Script_Unknown;
};

LK_NAMESPACE_END
#endif // LPLATFORMINPUTCTX_H
