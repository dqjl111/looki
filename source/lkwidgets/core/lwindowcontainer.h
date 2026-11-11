#ifndef LWINDOWCONTAINER_H
#define LWINDOWCONTAINER_H

#include "lwidget.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LWindowContainer : public LWidget
{
public:
    explicit LWindowContainer(std::shared_ptr<LWindow> embeddedWindow,
                              std::shared_ptr<LWidget> parent = nullptr,
                              LFlags<LWindowType> f = 0);

    std::shared_ptr<LWindow> containedWindow() const;

    static void toplevelAboutToBeDestroyed(std::shared_ptr<LWidget> parent);
    static void parentWasChanged(std::shared_ptr<LWidget> parent);
    static void parentWasMoved(std::shared_ptr<LWidget> parent);
    static void parentWasRaised(std::shared_ptr<LWidget> parent);
    static void parentWasLowered(std::shared_ptr<LWidget> parent);

protected:
    virtual bool handleEvent(std::shared_ptr<LEvent> ev) override;

    void focusWindowChanged(std::shared_ptr<LWindow> focusWindow);

    void updateContainGeometry();
    void markParentChain();
    void updateUsesNativeWidgets();

    bool isStillAnOrphan() const {
        return m_window->parent() == m_fakeParent;
    }

    std::shared_ptr<LWindow> m_fakeParent;
    std::shared_ptr<LWindow> m_window;
    std::weak_ptr<LWindow> m_oldFocusWindow;
    bool m_usesNativeWidgets = false;
};

LK_NAMESPACE_END

#endif // LWINDOWCONTAINER_H
