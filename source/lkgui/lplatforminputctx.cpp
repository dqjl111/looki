#include "lplatforminputctx.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/linputmethod.h"

LK_NAMESPACE_BEGIN

bool LPlatformInputCtx::m_inputMethodAccepted = false;

LPlatformInputCtx::LPlatformInputCtx()
{

}

void LPlatformInputCtx::setFocusObject(std::shared_ptr<LObject> object)
{

}

void LPlatformInputCtx::reset()
{

}

void LPlatformInputCtx::commit()
{

}

void LPlatformInputCtx::showInputPanel()
{

}

void LPlatformInputCtx::hideInputPanel()
{

}

bool LPlatformInputCtx::hasCapability(uint32_t capability)
{
    return false;
}

void LPlatformInputCtx::invokeAction(LInputMethod::Action action, int cursorPosition)
{
    if (action == LInputMethod::Click){
        reset();
    }
}

void LPlatformInputCtx::cursorRectChanged()
{

}

void LPlatformInputCtx::handleLocaleChanged()
{
    std::string language = localeLanguage();
    // lWarn() << "input lang:" << language;

    uint8_t orgScript = m_script;

    std::vector<std::string> langInfos = LStringUtil::splitToVec(language, "_-.@");
    if(langInfos.size() >1){
        m_script = lchar::Util::shortNameToScript(LStringUtil::trim(langInfos[1]));
    }else{
        m_script = lchar::Script_Unknown;
    }

    if(m_script != orgScript){
        std::shared_ptr<LInputMethod> m = LGuiApplication::inputMethod();
        m->sigLocaleChanged.emt();

        bool isLeftToRight = lchar::Util::isScriptLeftToRight(m_script);
        if(isLeftToRight != lchar::Util::isScriptLeftToRight(orgScript)){
            if(isLeftToRight){
                m->sigInputDirectionChanged.emt(LLayoutDirection::LeftToRight);
            }else{
                m->sigInputDirectionChanged.emt(LLayoutDirection::RightToLeft);
            }
        }
    }
}


std::string LPlatformInputCtx::localeLanguage() const
{
    return "";
}
LK_NAMESPACE_END
