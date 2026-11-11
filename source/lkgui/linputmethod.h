#ifndef LINPUTMETHOD_H
#define LINPUTMETHOD_H
#include "lkcore/lobject.h"
#include "lkcore/lsignal.h"
#include "lrect.h"
#include "lkgui/paint/ltransform2d.h"
#include "lkcore/lvariant.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LInputMethod : public LObject
{
public:
    LInputMethod();

    enum Action {
        Click,
        ContextMenu
    };

    inline const LTransform2D& inputItemTransform() const{
        return m_inputItemTransform;
    }
    void setInputItemTransform(const LTransform2D &transform);

    const LRect<double>& inputItemRectangle() const{
        return m_inputRectangle;
    }
    void setInputItemRectangle(const LRect<double> &rect);

    LRect<double> cursorRectangle() const;

    void show();
    void hide();

    void reset();
    void commit();

    void update(LFlags<LInputMethodQuery> queries);
    LLayoutDirection inputDirection() const;

    void invokeAction(Action a, int cursorPosition);

    static bool objectAcceptsInputMethod(std::shared_ptr<LObject> object);
    static LVariant queryFocusObject(LInputMethodQuery query, const LVariant &argument);

    LSignal<> sigVisibleChanged;
    LSignal<> sigLocaleChanged;

    LSignal<LLayoutDirection> sigInputDirectionChanged;
protected:
    LTransform2D m_inputItemTransform;
    LRect<double> m_inputRectangle;

};


LK_NAMESPACE_END
#endif // LINPUTMETHOD_H
