#ifndef LABSTRACTMENU_H
#define LABSTRACTMENU_H

#include "lkwidgets/util/laction.h"
#include "lkwidgets/widget/labstractslider.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LAbstractMenu  : public LWidget
{
public:
    LAbstractMenu(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f=LWindowType::Popup);

    void addAction(std::shared_ptr<LActionBase> action);
    int removeAction(std::shared_ptr<LActionBase> action);

    void addActions(const std::list<std::shared_ptr<LActionBase> > &actions);
    void insertAction(std::shared_ptr<LActionBase> before, std::shared_ptr<LActionBase> action);
    void insertActions(std::shared_ptr<LActionBase> before, const std::list<std::shared_ptr<LActionBase> > &actions);

    void insertAction(int index, std::shared_ptr<LActionBase> action);
    void insertActions(int index, const std::list<std::shared_ptr<LActionBase> > &actions);
    std::shared_ptr<LActionBase> action(int index) const;

    inline const std::list<std::shared_ptr<LActionBase> >& actions(){
        return m_actions;
    }

    inline int actionIndex(const std::shared_ptr<LActionBase>& item) const{
        return item==nullptr ? -1 : lItemIndex(m_actions, item);
    }

    std::shared_ptr<LActionBase> actionAt(const LPoint<int> &p, bool* isAtClose = nullptr) const;
    LRect<int> actionRect(std::shared_ptr<LActionBase> at) const;
    void setFirstActionActive();

    const std::shared_ptr<LWidget> topCausedWidget() const;
    void setMenuCause(std::shared_ptr<LWidget> w, std::shared_ptr<LActionBase> at = nullptr);

    inline bool isContextMenu() const{
        return topCausedWidget() == nullptr;
    }

    inline std::shared_ptr<LActionBase> addMenu(std::shared_ptr<LAbstractMenu> menu){
        return insertMenu(nullptr, menu);
    }
    std::shared_ptr<LActionBase> insertMenu(std::shared_ptr<LActionBase> before, std::shared_ptr<LAbstractMenu> menu);
    const inline std::shared_ptr<LActionBase> menuAction() const{
        return m_menuAction.lock();
    }


    virtual LSize<int> sizeHint() const override;
    void setScrollable(bool isScrollable);
    const inline bool isScrollable() const{
        return m_scrollable;
    }
    void setUseScrollBar(bool useScrollBar);
    const inline bool isUseScrollBar() const{
        return m_useScrollBar;
    }

    std::shared_ptr<LActionBase> exec();
    std::shared_ptr<LActionBase> exec(const LPoint<int> &pos, std::shared_ptr<LActionBase> at = nullptr);

    virtual void popup(const LPoint<int> &pos = LPoint<int>(), std::shared_ptr<LActionBase> at = nullptr);
    void hideUpToMenuBar();
    void hideMenu();

    LPoint<int> adjustedMenuPosition(std::shared_ptr<LWidget> w);
    const inline bool canCloseAction() const{
        return m_canCloseAction;
    }
protected:

    enum ScrollLocation { ScrollStay, ScrollBottom, ScrollTop, ScrollCenter };
    enum ScrollDirection { ScrollNone=0, ScrollUp=0x01, ScrollDown=0x02 };
    enum SelectionReason { SelectedFromKeyboard, SelectedFromElsewhere };

    virtual void initSpecific() override;
    virtual uint16_t uiElementState(LUiElementType type = LUiElementType::None) const override;
    virtual void hideEvent(std::shared_ptr<LEvent> e) override;
    virtual void mouseReleaseEvent(std::shared_ptr<LMouseEvent> e) override;
    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> e) override;
    virtual void mouseMoveEvent(std::shared_ptr<LMouseEvent> e) override;
    virtual void wheelEvent(std::shared_ptr<LWheelEvent> event) override;
    virtual void resizeEvent(std::shared_ptr<LResizeEvent> event) override;
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    virtual void leaveEvent(std::shared_ptr<LEvent> event) override;

    LSize<int> rawSizeHint() const;
    void updateActionRects();
    virtual void updateActionRects(const LRect<int> &containerRect) =0;
    virtual void popupAction(std::shared_ptr<LActionBase> a, bool activateFirst) =0;
    LRect<int> popupGeometry(std::shared_ptr<LScreen> screen = nullptr) const;

    int scrollerSize() const;
    LRect<int> prevScrollerRect(std::shared_ptr<LStyle> pStyle) const;
    LRect<int> nextScrollerRect(std::shared_ptr<LStyle> pStyle) const;
    LRect<int> scrollBarRect(std::shared_ptr<LStyle> pStyle) const;
    virtual LRect<int> actionCloseRect(std::shared_ptr<LActionBase> a,const LRect<int>& actRect) const;

    void updateSliderData();
    void updateSliderRange();

    void setCurrentAction(std::shared_ptr<LActionBase> a,
                              bool popup = false,
                              SelectionReason reason = SelectedFromElsewhere,
                              bool activateFirst = false);

    void scrollMenu(ScrollLocation location, bool active=false);
    void scrollMenu(ScrollDirection direction, int destOffset=0, bool active=false);
    void scrollMenu(std::shared_ptr<LActionBase> action, ScrollLocation location, bool active=false);

    bool mouseEventTaken(std::shared_ptr<LMouseEvent> e);
    virtual void drawScrollBar(LPainter *painter, LStyleOptComplex& opt);
    virtual void drawScroller(LPainter *painter, const LStyleOpt& opt, ScrollDirection type);
    virtual void drawMenuItem(std::shared_ptr<LActionBase> a, LPainter *p, const LStyleOpt& opt, std::shared_ptr<LStyle> pStyle) = 0;

    void setRepeatAction(LAbstractSlider::SliderAction action, int thresholdTime = 500, int repeatTime = 50);
    void doRepeat();
    void triggerAction(LAbstractSlider::SliderAction action);
    void stopRepeatAction();
    void setSliderDown(bool);
    void activateControl(LUiSubCtrl control, int threshold= 500);
    bool isSliderDataValid();
    int pixelPosToRangeValue(int pos, const LStyleOptComplex& opt, std::shared_ptr<LStyle> pStyle) const;

    void setSyncAction();
    void activateAction(std::shared_ptr<LActionBase> a, LAction::ActionEvent);
    void closeAction(std::shared_ptr<LActionBase> a, LAction::ActionEvent);
    void updateLayoutDirection();

    static bool considerAction(const std::shared_ptr<LActionBase> action);


    LUiSubCtrl m_pressedCtrl;
    LUiSubCtrl m_hoverCtrl;

    struct LMenuCaused {
        std::weak_ptr<LWidget> widget;
        std::weak_ptr<LActionBase> action;
    };

    LMenuCaused m_causedPopup;
    std::list<std::shared_ptr<LActionBase> > m_actions;
    mutable std::vector<LRect<int>> m_actionRects;

    std::weak_ptr<LActionBase> m_menuAction;
    std::shared_ptr<LActionBase> m_currentAction;
    std::shared_ptr<LActionBase> m_syncAction; //标志最后执行点击的action
    static std::weak_ptr<LAbstractMenu> m_mouseDown;

    std::shared_ptr<LEventLoop> m_eventLoop = nullptr;
    std::shared_ptr<LTimer> m_repeatTimer;
    int m_repeatActionTime  = 0;
    LAbstractSlider::SliderAction m_repeatAction= LAbstractSlider::SliderNoAction;
    std::unique_ptr<LSliderData> m_sliderData;


    mutable int m_viewSize = 0;
    int m_scrollOffset = 0;
    uint8_t m_scrollFlags = ScrollNone;

    union{
        uint16_t m_Flags = 0;
        struct{
            mutable bool m_itemsDirty : 1;
            mutable bool m_hasCheckableItems : 1;
            mutable bool m_hasPopupItems : 1;
            mutable bool m_isReachBound : 1; //窗口达到边界范围
            bool m_lastContextMenu : 1;
            bool m_collapsibleSeparators : 1;
            bool m_toolTipsVisible : 1;
            // Selection
            bool m_hasHadMouseDown : 1;
            bool m_isShortcutVisible : 1;
            bool m_scrollable : 1;
            bool m_useScrollBar:1;
            bool m_scrollHorizon:1;
            bool m_canCloseAction :1;
        };
    };
};


LK_NAMESPACE_END

#endif // LABSTRACTMENU_H
