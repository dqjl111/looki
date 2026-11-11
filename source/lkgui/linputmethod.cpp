#include "linputmethod.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/paint/ltransform2d.h"

LK_NAMESPACE_BEGIN

static inline bool platformSupportsHiddenText()
{
    const std::shared_ptr<LPlatformInputCtx> inputContext = LGuiApplication::platformIntegration()->inputContext();
    if(inputContext != nullptr){
        inputContext->hasCapability(LPlatformInputCtx::HiddenTextCapability);
    }
    return false;
}


static LRect<double> inputMethodQueryRectangleHelper(LInputMethodQuery imquery, const LTransform2D &xform)
{
    LRect<double> r;
    if (std::shared_ptr<LObject> focusObject = lGuiApp->focusObject()) {
        LVariant var = focusObject->inputMethodQuery(imquery);
        LCbor* cbor = var.getCbor();
        if (cbor != nullptr){
            r.fromCbor(*cbor);
            r = xform.mapRect(r);
        }
    }
    return r;
}

LInputMethod::LInputMethod()
{

}

void LInputMethod::setInputItemTransform(const LTransform2D &transform)
{
    if (m_inputItemTransform == transform){
        return;
    }

    m_inputItemTransform = transform;
    const std::shared_ptr<LPlatformInputCtx> inputContext = LGuiApplication::platformIntegration()->inputContext();
    if(inputContext != nullptr){
        inputContext->cursorRectChanged();
    }
}

void LInputMethod::setInputItemRectangle(const LRect<double> &rect)
{
    m_inputRectangle = rect;
}


LRect<double> LInputMethod::cursorRectangle() const
{
    return inputMethodQueryRectangleHelper(LInputMethodQuery::ImCursorRectangle, m_inputItemTransform);
}

void LInputMethod::show()
{
    const std::shared_ptr<LPlatformInputCtx> inputContext = LGuiApplication::platformIntegration()->inputContext();
    if(inputContext != nullptr){
        inputContext->showInputPanel();
    }
}

void LInputMethod::hide()
{
    const std::shared_ptr<LPlatformInputCtx> inputContext = LGuiApplication::platformIntegration()->inputContext();
    if(inputContext != nullptr){
        inputContext->hideInputPanel();
    }
}

void LInputMethod::reset()
{
    const std::shared_ptr<LPlatformInputCtx> inputContext = LGuiApplication::platformIntegration()->inputContext();
    if(inputContext != nullptr){
        inputContext->reset();
    }
}

void LInputMethod::commit()
{
    const std::shared_ptr<LPlatformInputCtx> inputContext = LGuiApplication::platformIntegration()->inputContext();
    if(inputContext != nullptr){
        inputContext->commit();
    }
}

void LInputMethod::update(LFlags<LInputMethodQuery> queries)
{
    if (queries & LInputMethodQuery::ImEnabled) {
        std::shared_ptr<LObject> focus = LGuiApplication::focusObject();
        bool enabled = objectAcceptsInputMethod(focus);
        LPlatformInputCtx::setInputMethodAccepted(enabled);
    }

    const std::shared_ptr<LPlatformInputCtx> inputContext = LGuiApplication::platformIntegration()->inputContext();
    if(inputContext != nullptr){
        inputContext->update(queries);

        if (queries & LInputMethodQuery::ImCursorRectangle){
            inputContext->cursorRectChanged();
        }
    }
}

LLayoutDirection LInputMethod::inputDirection() const
{
    const std::shared_ptr<LPlatformInputCtx> inputContext = LGuiApplication::platformIntegration()->inputContext();
    if(inputContext != nullptr){
        return inputContext->inputDirection();
    }
    return LLayoutDirection::LeftToRight;
}

void LInputMethod::invokeAction(Action a, int cursorPosition)
{
    const std::shared_ptr<LPlatformInputCtx> inputContext = LGuiApplication::platformIntegration()->inputContext();
    if(inputContext != nullptr){
        inputContext->invokeAction(a, cursorPosition);
    }
}

bool LInputMethod::objectAcceptsInputMethod(std::shared_ptr<LObject> object)
{
    bool enabled = false;
    if (object != nullptr) {
        // If the platform does not support hidden text, query the hints
        // in addition and disable in case of ImhHiddenText.
        static const bool supportsHiddenText = platformSupportsHiddenText();
        enabled = object->inputMethodQuery(LInputMethodQuery::ImEnabled).getBool();
        if(enabled && !supportsHiddenText){
            LFlags<LInputMethodHint> hints(object->inputMethodQuery(LInputMethodQuery::ImHints).getInt32());
            if(hints.testFlag(LInputMethodHint::ImhHiddenText)){
                enabled = false;
            }
        }
    }
    return enabled;
}

LVariant LInputMethod::queryFocusObject(LInputMethodQuery query, const LVariant &argument)
{
    std::shared_ptr<LObject> focusObject = LGuiApplication::focusObject();
    if (focusObject == nullptr){
        return LVariant();
    }
    return focusObject->inputMethodQuery(query, argument);
}

LK_NAMESPACE_END
