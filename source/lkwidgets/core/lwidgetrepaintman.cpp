#include "lwidgetrepaintman.h"
#include "lwidget.h"
#include "lapplication.h"
#include "lkgui/lscreen.h"
#include <algorithm>
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

LWidgetRepaintMan::LWidgetRepaintMan(std::shared_ptr<LWidget> w) :m_tlw(w)
{
    if(w != nullptr){
        m_store = w->backingStore();
    }
    m_dirty = std::make_shared<LRegion>();
    m_topLevelNeedsFlush = std::make_shared<LRegion>();
    updateLists(w);
}


void LWidgetRepaintMan::updateLists(std::shared_ptr<LWidget> widget)
{
    if (widget == nullptr){
        return;
    }

    std::list<std::shared_ptr<LTreeObj> > children = widget->children();
    for(std::shared_ptr<LTreeObj> tc : children){
        std::shared_ptr<LWidget> child = std::dynamic_pointer_cast<LWidget>(tc);
        if(child != nullptr){
            if(child->isWindow()){
                continue;
            }
            updateLists(child);
        }
    }

    if (widget->testAttribute(LWidgetAttribute::StaticContents)){
        addStaticWidget(widget);
    }
}


void LWidgetRepaintMan::removeDirtyWidget(std::shared_ptr<LWidget> w)
{
    if (w==nullptr){
        return;
    }

    auto isWidget = [w](std::weak_ptr<LWidget> ww) { return ww.lock() == w; };

    m_dirtyWidgets.remove_if(isWidget);
    m_dirtyRenderToTextureWidgets.remove_if(isWidget);
    resetWidget(w);

    m_needsFlushWidgets.remove_if(isWidget);

    auto it = w->m_children.begin();
    for(; it != w->m_children.end(); ++it){
        std::shared_ptr<LWidget> child = std::dynamic_pointer_cast<LWidget>(*it);
        if(child != nullptr){
            removeDirtyWidget(child);
        }
    }
}

void LWidgetRepaintMan::addStaticWidget(std::shared_ptr<LWidget> widget)
{
    if (widget == nullptr){
        return;
    }

    auto it = std::find_if(m_staticWidgets.begin(),
                           m_staticWidgets.end(),
                           [widget](std::weak_ptr<LWidget> w) { return w.lock() == widget; });
    if (it == m_staticWidgets.end())
    {
        m_staticWidgets.push_back(widget);
    }
}

void LWidgetRepaintMan::moveStaticWidgets(std::shared_ptr<LWidget> reparented)
{
    LWidgetRepaintMan *newPaintManager = reparented->maybeRepaintMan();
    if (newPaintManager == this){
        return;
    }

    auto it = m_staticWidgets.begin();
    for(; it != m_staticWidgets.end();){
        std::shared_ptr<LWidget> w = it->lock();
        if (reparented == w || reparented->isAncestorOf(w)) {
            it = m_staticWidgets.erase(it);
            if (newPaintManager != nullptr){
                newPaintManager->addStaticWidget(w);
            }
        }else{
            ++it;
        }
    }
}

void LWidgetRepaintMan::removeStaticWidget(std::shared_ptr<LWidget> widget)
{
    m_staticWidgets.remove_if([widget](std::weak_ptr<LWidget> w) { return w.lock() == widget; });
}

void LWidgetRepaintMan::markDirty(LRect<int> widgetRect, std::shared_ptr<LWidget> widget, UpdateTime updateTime, BufferState bufferState)
{
    std::shared_ptr<LRegion> r = std::make_shared<LRegion>(widgetRect);
    markDirty(r, widgetRect, widget, updateTime, bufferState);
}

void LWidgetRepaintMan::markDirty(std::shared_ptr<LRegion> r, std::shared_ptr<LWidget> widget, UpdateTime updateTime, BufferState bufferState)
{
    if(r != nullptr){
        markDirty(r, widget->rect(), widget, updateTime, bufferState);
    }
}


/*!
    Synchronizes the \a exposedRegion of the \a exposedWidget with the backing store.

    If there are dirty widgets, including but not limited to the \a exposedWidget,
    these will be repainted first. The backingstore is then flushed to the screen,
    regardless of whether or not there were any repaints.
*/
void LWidgetRepaintMan::sync(std::shared_ptr<LWidget> exposedWidget,
                             const std::shared_ptr<LRegion> exposedRegion)
{

    std::shared_ptr<LWidget> tlw= m_tlw.lock();
    if(tlw != nullptr){
        if (!tlw->isVisible()){
            return;
        }

        if (!exposedWidget || !hasPlatformWindow(exposedWidget)
            || !exposedWidget->isVisible() || !exposedWidget->testAttribute(LWidgetAttribute::Mapped)
            || !exposedWidget->updatesEnabled() || exposedRegion->isEmpty()) {
            return;
        }

        // Nothing to repaint.
        if(m_store == nullptr){
            return;
        }
        if (!isDirty() && m_store->size().isValid()) {
            //LPlatformTextureList *widgetTextures = widgetTexturesFor(tlw, exposedWidget);
            //flush(exposedWidget, widgetTextures ? nullptr : exposedRegion, widgetTextures);
            flush(exposedWidget, exposedRegion);
            return;
        }

        // As requests to sync a specific widget typically comes from an expose event
        // we can't rely solely on our own dirty tracking to decide what to flush, and
        // need to respect the platform's request to at least flush the entire widget,
        LPoint<int> offset;
        if(exposedWidget != tlw){
            offset = exposedWidget->mapTo(tlw, LPoint<int>());
        }
        markNeedsFlush(exposedWidget, exposedRegion, offset);

        if (syncAllowed()){
            paintAndFlush();
        }
    }
}

/*!
    Synchronizes the backing store, i.e. dirty areas are repainted and flushed.
*/
void LWidgetRepaintMan::sync()
{
    m_updateRequestSent = false;
    std::shared_ptr<LWidget> tlw= m_tlw.lock();
    if(tlw != nullptr){
        if (tlw->shouldDiscardSyncRequest()) {
            // If the top-level is minimized, it's not visible on the screen so we can delay the
            // update until it's shown again. In order to do that we must keep the dirty states.
            // These will be cleared when we receive the first expose after showNormal().
            // However, if the widget is not visible (isVisible() returns false), everything will
            // be invalidated once the widget is shown again, so clear all dirty states.
            if (!tlw->isVisible()) {
                m_dirty->clear();
                for(std::weak_ptr<LWidget> ww : m_dirtyWidgets){
                    std::shared_ptr<LWidget> dw = ww.lock();
                    resetWidget(dw);
                }
                m_dirtyWidgets.clear();
            }
            return;
        }

        if (syncAllowed()){
            paintAndFlush();
        }
    }
}

void LWidgetRepaintMan::markNeedsFlush(std::shared_ptr<LWidget> widget,
                                       const std::shared_ptr<LRegion> region,
                                       const LPoint<int> &topLevelOffset)
{
    if (widget == nullptr || region == nullptr){
        return;
    }

    if(widget->shouldPaintOnScreen() || region->isEmpty()){
        return;
    }

    std::shared_ptr<LWidget> tlw= m_tlw.lock();
    if (widget == tlw) {
        m_topLevelNeedsFlush->unite(region);
    } else if (!hasPlatformWindow(widget) && !widget->isWindow()) {
        std::shared_ptr<LWidget> nativeParent = widget->nativeParentWidget();
        if (nativeParent == tlw) {
            // Alien widgets with the top-level as the native parent (common case)
            m_topLevelNeedsFlush->unite(region->translated(topLevelOffset));
        } else {
            // Alien widgets with native parent != tlw
            const LPoint<int> nativeParentOffset = widget->mapTo(nativeParent, LPoint<int>());
            std::shared_ptr<LRegion> t = region->translated(nativeParentOffset);
            markNeedsFlush(nativeParent, t);
        }
    } else {
        markNeedsFlush(widget, region);
    }
}

bool LWidgetRepaintMan::hasPlatformWindow(std::shared_ptr<LWidget> widget)
{
    if(widget != nullptr){
        std::shared_ptr<LWindow> h = widget->windowHandle();
        if(h != nullptr){
            return h->handle() != nullptr;
        }
    }
    return false;
}

bool LWidgetRepaintMan::isDirty() const
{
    return !(m_dirtyWidgets.empty() && m_dirty->isEmpty() && m_dirtyRenderToTextureWidgets.empty());
}

std::shared_ptr<LRegion> LWidgetRepaintMan::staticContents(std::shared_ptr<LWidget> parent, const LRect<int> &withinClipRect) const
{
    std::shared_ptr<LWidget> tlw= m_tlw.lock();
    if(parent == nullptr){
        if(tlw != nullptr){
            if (tlw->testAttribute(LWidgetAttribute::StaticContents)) {
                LRect<int> backingstoreRect(0, 0, m_store->size());
                if (!withinClipRect.isEmpty()){
                    backingstoreRect &= withinClipRect;
                }
                std::shared_ptr<LRegion> region = std::make_shared<LRegion>(backingstoreRect);
                return region;
            }
        }
    }

    std::shared_ptr<LRegion> region = std::make_shared<LRegion>();
    if(parent != nullptr){
        if (parent->m_children.empty()){
            return region;
        }
    }


    const bool clipToRect = !withinClipRect.isEmpty();
    for (std::weak_ptr<LWidget> ww : m_staticWidgets) {
        std::shared_ptr<LWidget> w = ww.lock();
        if(w != nullptr){
            if((!w->m_isOpaque)
                || (w->m_pExtraData == nullptr)
                || (!w->isVisible())){
                continue;
            }

            if(w->m_pExtraData->staticContentsSize.isEmpty()){
                continue;
            }

            if(parent != nullptr){
                if(!(parent->isAncestorOf(w))){
                    continue;
                }
            }

            LRect<int> rect(0,
                       0,
                       w->m_pExtraData->staticContentsSize.width(),
                       w->m_pExtraData->staticContentsSize.height());

            LPoint<int> offset;
            if(parent != nullptr){
                offset = w->mapTo(parent, LPoint<int>());
            }else{
                offset = w->mapTo(tlw, LPoint<int>());
            }

            if (clipToRect){
                rect &= withinClipRect.translated(-offset);
            }
            if (rect.isEmpty()){
                continue;
            }

            rect &= w->clipRect();
            if (rect.isEmpty()){
                continue;
            }

            std::shared_ptr<LRegion> visible
                = std::make_shared<LRegion>(rect);
            w->clipToEffectiveMask(visible);
            if (visible->isEmpty()){
                continue;
            }
            w->subtractOpaqueSiblings(visible, nullptr, true);

            visible->translate(offset);
            region->unite(visible);
        }
    }
    return region;

}

bool LWidgetRepaintMan::bltRect(const LRect<int> &rect, int dx, int dy, std::shared_ptr<LWidget> widget)
{
    std::shared_ptr<LWidget> tlw = m_tlw.lock();
    const LPoint<int> pos(widget->mapTo(tlw, rect.topLeft()));
    const LRect<int> tlwRect(pos, rect.size());
    if(m_dirty != nullptr){
        if (m_dirty->isIntersects(tlwRect)){
            return false; // We don't want to scroll junk.
        }
    }
    return m_store->scroll(tlwRect, dx, dy);
}

void LWidgetRepaintMan::markDirty(std::shared_ptr<LRegion> r,
                                  LRect<int> widgetRect,
                                  std::shared_ptr<LWidget> widget,
                                  UpdateTime updateTime,
                                  BufferState bufferState)
{
    if(widget->isVisible()== false
        || widget->updatesEnabled() == false
        || widget->window() != m_tlw.lock()){
#ifdef _DEBUG
        lDebug() << "Invalid mark dirty widget";
#endif
        return;
    }

    if (widget->shouldPaintOnScreen()) {
        if (widget->isDirtyEmpty()) {
            widget->m_dirty = r;
            sendUpdateRequest(widget, updateTime);
            return;
        } else if (LRegion::regionStrictContains(widget->m_dirty.get(), widgetRect)) {
            if (updateTime == UpdateNow){
                sendUpdateRequest(widget, updateTime);
            }
            return; // Already dirty
        }

        const bool eventAlreadyPosted = !widget->isDirtyEmpty();
        if(widget->m_dirty == nullptr){
            widget->m_dirty = r;
        }else{
            widget->m_dirty->unite(r);
        }
        if (!eventAlreadyPosted || updateTime == UpdateNow){
            sendUpdateRequest(widget, updateTime);
        }
        return;
    }

    // ---------------------------------------------------------------------------

    std::shared_ptr<LWidget> tlw = m_tlw.lock();
    if (widget->m_renderToTexture) {
        if (!widget->m_inDirtyList){
            addDirtyRenderToTextureWidget(widget);
        }
        if (!m_updateRequestSent || updateTime == UpdateNow){
            sendUpdateRequest(tlw, updateTime);
        }
        return;
    }

    // ---------------------------------------------------------------------------
    if(m_dirty != nullptr){        
        const LPoint<int> offset = widget->mapTo(tlw, LPoint<int>());
        LRect<int> translatedRect = widgetRect.translated(offset);

        if (LRegion::regionStrictContains(m_dirty.get(), translatedRect)) {
            if (updateTime == UpdateNow){
                sendUpdateRequest(tlw, updateTime);
            }
            return; // Already dirty
        }

        // ---------------------------------------------------------------------------

        if (bufferState == BufferInvalid) {
            const bool eventAlreadyPosted = !m_dirty->isEmpty() || m_updateRequestSent;
            std::shared_ptr<LRegion>  t = r->translated(offset);

            //std::shared_ptr<LRegion> backDirty = std::make_shared<LRegion>(m_dirty);
            m_dirty->unite(t);
            //checkDirty();

            if (!eventAlreadyPosted || updateTime == UpdateNow){
                sendUpdateRequest(tlw, updateTime);
            }
            return;
        }
    }


    // ---------------------------------------------------------------------------

    if (m_dirtyWidgets.empty()) {
        addDirtyWidget(widget, r);
        sendUpdateRequest(tlw, updateTime);
        return;
    }

    // ---------------------------------------------------------------------------

    if (widget->m_inDirtyList) {
        if (!LRegion::regionStrictContains(widget->m_dirty.get(), widgetRect)) {
            if(widget->m_dirty == nullptr){
                widget->m_dirty = r;
            }else{
                widget->m_dirty->unite(r);
            }
        }
    } else {
        addDirtyWidget(widget, r);
    }

    // ---------------------------------------------------------------------------
    if (updateTime == UpdateNow){
        sendUpdateRequest(tlw, updateTime);
    }
}

void LWidgetRepaintMan::addDirtyWidget(std::shared_ptr<LWidget> widget, const std::shared_ptr<LRegion> rgn)
{
    if(widget != nullptr){
        if (!widget->m_widgetData.inDestructor) {
            widget->m_dirty = rgn;
            m_dirtyWidgets.push_back(widget);
            widget->m_inDirtyList = true;
        }
    }
}

void LWidgetRepaintMan::resetWidget(std::shared_ptr<LWidget> widget)
{
    if (widget != nullptr) {
        widget->m_inDirtyList = false;
        widget->m_isScrolled = false;
        widget->m_isMoved = false;
        widget->m_dirty = nullptr;
    }
}

void LWidgetRepaintMan::sendUpdateRequest(std::shared_ptr<LWidget> widget, UpdateTime updateTime)
{
    if (widget == nullptr){
        return;
    }

    // Having every repaint() leading to a sync/flush is bad as it causes
    // compositing and waiting for vsync each and every time. Change to
    // UpdateLater, except for approx. once per frame to prevent starvation in
    // case the control does not get back to the event loop.
    if(updateTime == UpdateNow){
        std::shared_ptr<LWidget> w= widget->window();
        if(w != nullptr){
            std::shared_ptr<LWindow> wh = w->windowHandle();
            if(wh != nullptr){
                double refresh = 60;
                std::shared_ptr<LScreen> scr = wh->screen();
                if (scr != nullptr){
                    refresh = scr->m_refreshRate;
                }

                if(wh->compositing()){
                    if(wh->lastComposeTime().isValid()){
                        const int64_t elapsed = wh->lastComposeTime().msecsElapsed();
                        if (elapsed <= int64_t(1000.0 / refresh)){
                            updateTime = UpdateLater;
                        }
                    }
                }
            }

        }
    }

    switch (updateTime) {
    case UpdateLater:{
        m_updateRequestSent = true;
        std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::UpdateRequest);
        LCoreApplication::postEvent(widget, e);
        break;
    }
    case UpdateNow: {
        std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::UpdateRequest);
        LCoreApplication::sendEvent(widget, e);
        break;
    }
    default:{
        break;
    }
    }

}

void LWidgetRepaintMan::addDirtyRenderToTextureWidget(std::shared_ptr<LWidget> widget)
{
    if(widget != nullptr){
        if (widget->m_inDirtyList && !widget->m_widgetData.inDestructor) {
            m_dirtyRenderToTextureWidgets.push_back(widget);
            widget->m_inDirtyList = true;
        }
    }
}

bool LWidgetRepaintMan::syncAllowed()
{
    //Todo
    return true;
}

void LWidgetRepaintMan::paintAndFlush()
{
    if(m_store == nullptr){
        return;
    }

    std::shared_ptr<LWidget> tlw = m_tlw.lock();
    if(tlw== nullptr){
        return;
    }

    const bool updatesDisabled = !tlw->updatesEnabled();
    bool repaintAllWidgets = false;

    const LRect<int> tlwRect = tlw->m_widgetData.crect;
    if (!updatesDisabled && m_store->size() != tlwRect.size()) {
        if ((!m_staticWidgets.empty()) && !m_store->size().isEmpty() ) {
            // Repaint existing dirty area and newly visible area.
            const LRect<int> clipRect(0, 0, m_store->size());
            std::shared_ptr<LRegion> staticRegion = staticContents(nullptr, clipRect);

            std::shared_ptr<LRegion> newVisible
                = std::make_shared<LRegion>(0, 0, tlwRect.width(), tlwRect.height());
            newVisible->subtract(staticRegion);

            //std::shared_ptr<LRegion> backDirty = std::make_shared<LRegion>(m_dirty);
            m_dirty->intersect(newVisible);
            //checkDirty();
            m_store->setStaticContents(staticRegion);
        } else {
            // Repaint everything.
            for(std::weak_ptr<LWidget> ww : m_dirtyWidgets){
                std::shared_ptr<LWidget> sw =ww.lock();
                if(sw != nullptr){
                    resetWidget(sw);
                }
            }
            m_dirtyWidgets.clear();
            m_dirty = std::make_shared<LRegion>(0, 0, tlwRect.width(), tlwRect.height());
            repaintAllWidgets = true;
        }
    }

    if (m_store->size() != tlwRect.size()){
        m_store->resize(tlwRect.size());
    }

    if (updatesDisabled){
        return;
    }

    // Contains everything that needs repaint.
    std::shared_ptr<LRegion> toClean = std::make_shared<LRegion>(*m_dirty);

    // Loop through all update() widgets and remove them from the list before they are
    // painted (in case someone calls update() in paintEvent). If the widget is opaque
    // and does not have transparent overlapping siblings, append it to the
    // opaqueNonOverlappedWidgets list and paint it directly without composition.
    std::list<std::shared_ptr<LWidget> > opaqueNonOverlappedWidgets;
    for (std::weak_ptr<LWidget> ww : m_dirtyWidgets) {
        std::shared_ptr<LWidget> w = ww.lock();
        if(w == nullptr){
            continue;
        }
        if (w->m_widgetData.inDestructor){
            continue;
        }

        // Clip with mask() and clipRect().
        if(!w->isDirtyEmpty()){
            w->m_dirty->intersect(w->clipRect());
            w->clipToEffectiveMask(w->m_dirty);
        }

        // Subtract opaque siblings and children.
        bool hasDirtySiblingsAbove = false;
        // We know for sure that the widget isn't overlapped if 'isMoved' is true.
        if (!w->m_isMoved){
            w->subtractOpaqueSiblings(w->m_dirty, &hasDirtySiblingsAbove);
        }

        // Make a copy of the widget's dirty region, to restore it in case there is an opaque
        // render-to-texture child that completely covers the widget, because otherwise the
        // render-to-texture child won't be visible, due to its parent widget not being redrawn
        // with a proper blending mask.
        std::shared_ptr<LRegion> dirtyBeforeSubtractedOpaqueChildren = w->m_dirty;

        // Scrolled and moved widgets must draw all children.
        if (!w->m_isScrolled && !w->m_isMoved){
            w->subtractOpaqueChildren(w->m_dirty, w->rect());
        }

        if (w->isDirtyEmpty() && w->m_textureChildSeen){
            w->m_dirty = dirtyBeforeSubtractedOpaqueChildren;
        }

        if (w->isDirtyEmpty()) {
            resetWidget(w);
            continue;
        }

        std::shared_ptr<LRegion> widgetDirty = nullptr;
        if(w != tlw){
            widgetDirty = w->m_dirty->translated(w->mapTo(tlw, LPoint<int>()));
        }else{
            widgetDirty = w->m_dirty;
        }
        toClean->unite(widgetDirty);


        if (!hasDirtySiblingsAbove && w->m_isOpaque && !m_dirty->isIntersects(widgetDirty->boundingRect())) {
            opaqueNonOverlappedWidgets.push_back(w);
        } else {
            resetWidget(w);
            //std::shared_ptr<LRegion> backDirty = std::make_shared<LRegion>(m_dirty);
            m_dirty->unite(widgetDirty);
            //checkDirty();
        }
    }
    m_dirtyWidgets.clear();

    if (toClean->isEmpty()) {
        std::list<std::shared_ptr<LWidget> > paintPending;
        for(std::weak_ptr<LWidget> ww : m_dirtyRenderToTextureWidgets){
            std::shared_ptr<LWidget> w = ww.lock();
            if(w != nullptr){
                paintPending.push_back(w);
                resetWidget(w);
            }
        }
        m_dirtyRenderToTextureWidgets.clear();

        for(std::shared_ptr<LWidget> w : paintPending){
            w->sendPaintEvent(w->rect());
            if (w != tlw) {
                std::shared_ptr<LWidget> npw = w->nativeParentWidget();
                if (hasPlatformWindow(w)) {
                    markNeedsFlush(w);
                }else if((npw!=nullptr) && (npw != tlw)){
                    markNeedsFlush(npw);
                }
            }
        }
        // We might have newly exposed areas on the screen if this function was
        // called from sync, so we have to make sure those
        // are flushed. We also need to composite the renderToTexture widgets.
        flush();
        return;
    }

    m_store->beginPaint(toClean);

    // Must do this before sending any paint events because
    // the size may change in the paint event.
    updateStaticContentsSize();
    std::shared_ptr<LRegion> dirtyCopy = std::make_shared<LRegion>(m_dirty);
    m_dirty->clear();
    m_updateRequestSent = false;

    // Paint opaque non overlapped widgets.
    for (std::shared_ptr<LWidget> w : opaqueNonOverlappedWidgets) {

        LFlags<LDrawWidgetOpt> flags = LDrawWidgetOpt::DrawRecursive;
        // Scrolled and moved widgets must draw all children.
        if (!w->m_isScrolled && !w->m_isMoved){
            flags |= LDrawWidgetOpt::DontDrawOpaqueChildren;
        }
        if (w == tlw){
            flags |= LDrawWidgetOpt::DrawAsRoot;
        }

        std::shared_ptr<LRegion> toBePainted = std::make_shared<LRegion>(w->m_dirty);
        resetWidget(w);

        LPoint<int> offset;
        if (w != tlw){
            offset += w->mapTo(tlw, LPoint<int>());
        }
        w->drawWidget(m_store->paintDevice(), toBePainted, offset, flags, nullptr, this);
    }

    // Paint the rest with composition.
    if (repaintAllWidgets || !dirtyCopy->isEmpty()) {
        LFlags<LDrawWidgetOpt> flags = LDrawWidgetOpt::DrawAsRoot | LDrawWidgetOpt::DrawRecursive;
        tlw->drawWidget(m_store->paintDevice(), dirtyCopy, LPoint<int>(), flags, nullptr, this);
    }

    m_store->endPaint();
    flush();
}

void LWidgetRepaintMan::markNeedsFlush(std::shared_ptr<LWidget> widget,
                                       const std::shared_ptr<LRegion> region)
{
    if (widget == nullptr){
        return;
    }

    if (widget->m_needsFlush == nullptr){
        widget->m_needsFlush = std::make_shared<LRegion>();
    }

    widget->m_needsFlush->unite(region);

    auto it = std::find_if(m_needsFlushWidgets.begin(),
                           m_needsFlushWidgets.end(),
                           [widget](std::weak_ptr<LWidget> w) { return w.lock() == widget; });
    if (it == m_staticWidgets.end())
    {
        m_needsFlushWidgets.push_back(widget);
    }
}

void LWidgetRepaintMan::flush()
{
    const bool hasNeedsFlushWidgets = !m_needsFlushWidgets.empty();
    bool flushed = false;

    // Flush the top level widget
    if (!m_topLevelNeedsFlush->isEmpty()) {
        std::shared_ptr<LWidget> tlw = m_tlw.lock();
        flush(tlw, m_topLevelNeedsFlush);
        m_topLevelNeedsFlush->clear();
        flushed = true;
    }

    if (!hasNeedsFlushWidgets){
        return;
    }

    for (std::weak_ptr<LWidget> ww : m_needsFlushWidgets) {
        std::shared_ptr<LWidget> w = ww.lock();
        if(w != nullptr){
            flush(w, w->m_needsFlush);
            w->m_needsFlush = nullptr;
        }
    }
    m_needsFlushWidgets.clear();
}

void LWidgetRepaintMan::flush(std::shared_ptr<LWidget> widget, const std::shared_ptr<LRegion> region)
{
    std::shared_ptr<LWidget> tlw = m_tlw.lock();
    if(tlw == nullptr || widget == nullptr){
        return;
    }

    if (tlw->testAttribute(LWidgetAttribute::DontShowOnScreen)
        || widget->testAttribute(LWidgetAttribute::DontShowOnScreen)){
        return;
    }

    // Foreign Windows do not have backing store content and must not be flushed
    if (std::shared_ptr<LWindow> widgetWindow = widget->windowHandle()) {
        if(widgetWindow != nullptr){
            if (widgetWindow->type() == LWindowType::ForeignWindow){
                return;
            }
        }
    }

    LPoint<int> offset;
    if (widget != tlw){
        offset += widget->mapTo(tlw, LPoint<int>());
    }

    m_store->flush(region, widget->windowHandle(), offset);
}

void LWidgetRepaintMan::updateStaticContentsSize()
{
    for(std::weak_ptr<LWidget> ww : m_staticWidgets){
        std::shared_ptr<LWidget> w = ww.lock();
        if(w != nullptr){
            if(w->m_pExtraData == nullptr){
                w->createExtra();
            }
            w->m_pExtraData->staticContentsSize =  w->m_widgetData.crect.size();
        }
    }
}


LK_NAMESPACE_END
