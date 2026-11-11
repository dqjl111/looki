#ifndef LWIDGETREPAINTMAN_H
#define LWIDGETREPAINTMAN_H
#include "lkcore/lobject.h"
#include <list>
#include "lkgui/lrect.h"
#include "lkgui/paint/lregion.h"
#include "lkgui/lbackingstore.h"
//#include "lkgui/paint/

LK_NAMESPACE_BEGIN

class LWidget;
class LWidgetRepaintMan
{
public:
    LWidgetRepaintMan(std::shared_ptr<LWidget> w);

    enum UpdateTime {
        UpdateNow,
        UpdateLater
    };

    enum BufferState{
        BufferValid,
        BufferInvalid
    };

    void removeDirtyWidget(std::shared_ptr<LWidget> w);

    void addStaticWidget(std::shared_ptr<LWidget> widget);
    void moveStaticWidgets(std::shared_ptr<LWidget> reparented);
    void removeStaticWidget(std::shared_ptr<LWidget> widget);

    void markDirty(LRect<int> widgetRect,
                   std::shared_ptr<LWidget> widget,
                   UpdateTime updateTime = UpdateLater,
                   BufferState bufferState = BufferValid);

    void markDirty(std::shared_ptr<LRegion> r,
                   std::shared_ptr<LWidget> widget,
                   UpdateTime updateTime = UpdateLater,
                   BufferState bufferState = BufferValid);

    void sync(std::shared_ptr<LWidget> exposedWidget, const std::shared_ptr<LRegion> exposedRegion);
    void sync();

    inline std::shared_ptr<LBackingStore> backingStore() const { return m_store; }
    inline void setBackingStore(std::shared_ptr<LBackingStore> backingStore) { m_store = backingStore; }

    void markNeedsFlush(std::shared_ptr<LWidget> widget,
                        const std::shared_ptr<LRegion> region,
                        const LPoint<int> &topLevelOffset);

    static bool hasPlatformWindow(std::shared_ptr<LWidget> widget);

    bool isDirty() const;

    std::shared_ptr<LRegion> staticContents(std::shared_ptr<LWidget> parent = nullptr,
                                            const LRect<int> &withinClipRect = LRect<int>()) const;

    bool bltRect(const LRect<int> &rect, int dx, int dy, std::shared_ptr<LWidget> widget);
protected:
    void updateLists(std::shared_ptr<LWidget> widget);

    void markDirty(std::shared_ptr<LRegion> r,
                   LRect<int> widgetRect,
                   std::shared_ptr<LWidget> widget,
                   UpdateTime updateTime = UpdateLater,
                   BufferState bufferState = BufferValid);

    void addDirtyWidget(std::shared_ptr<LWidget> widget, const std::shared_ptr<LRegion> rgn);
    void resetWidget(std::shared_ptr<LWidget> widget);
    void sendUpdateRequest(std::shared_ptr<LWidget> widget, UpdateTime updateTime);

    void addDirtyRenderToTextureWidget(std::shared_ptr<LWidget> widget);

    bool syncAllowed();
    void paintAndFlush();

    void markNeedsFlush(std::shared_ptr<LWidget> widget, const std::shared_ptr<LRegion> region = nullptr);
    void flush();
    void flush(std::shared_ptr<LWidget> widget, const std::shared_ptr<LRegion> region);
    void updateStaticContentsSize();

    inline void checkDirty() const
    {
#ifdef _DEBUG
        if(m_dirty != nullptr){
            for(const LRect<int>& rect : *m_dirty){
                if(rect.isEmpty()){
                    lWarn() << "Dirty contains invalid rect:" << rect.m_x << "," <<rect.m_y << "," << rect.m_width << rect.m_height;
                }
            }
        }
#endif
    }

    bool m_updateRequestSent = false;

    std::shared_ptr<LRegion> m_dirty; // needsRepaint
    std::weak_ptr<LWidget> m_tlw;
    std::shared_ptr<LBackingStore> m_store;

    std::list<std::weak_ptr<LWidget> > m_dirtyWidgets;
    std::list<std::weak_ptr<LWidget> > m_dirtyRenderToTextureWidgets;
    std::list<std::weak_ptr<LWidget> > m_staticWidgets;

    std::shared_ptr<LRegion> m_topLevelNeedsFlush;
    std::list<std::weak_ptr<LWidget> > m_needsFlushWidgets;
};

LK_NAMESPACE_END

#endif // LWIDGETREPAINTMAN_H
