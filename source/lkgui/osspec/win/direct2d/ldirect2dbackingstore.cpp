#include "ldirect2dbackingstore.h"
#include "lkgui/paint/lpainter.h"
#include "ldirect2dpixmap.h"
#include "ldirect2dwin.h"

LK_NAMESPACE_BEGIN

LDirect2DBackingStore::LDirect2DBackingStore()
{

}

void LDirect2DBackingStore::beginPaint(const std::shared_ptr<LRegion> region)
{
    std::shared_ptr<LWindow> w = window();
    if(w != nullptr){
        std::shared_ptr<LDirect2DWin> dw = std::dynamic_pointer_cast<LDirect2DWin>(w->handle());
        if(dw != nullptr){
            std::shared_ptr<LPixmap> pixmap = dw->pixmap();
            if(pixmap != nullptr){
                std::shared_ptr<LDirect2DPixmap> dp =
                    std::dynamic_pointer_cast<LDirect2DPixmap>(pixmap->handle());
                if(dp != nullptr){
                    dp->beginPaint();

                    LPainter painter(pixmap);                 
                    LColor clear(LColorID::Transparent);
                    painter.setCompositionMode(LCompositionMode::Source);

                    for (const LRect<int> &r : *region){
                        painter.fillRect(r.toRectF(), clear);
                    }
                }
            }
        }
    }
}

void LDirect2DBackingStore::endPaint()
{
    std::shared_ptr<LWindow> w = window();
    if(w != nullptr){
        std::shared_ptr<LDirect2DWin> dw = std::dynamic_pointer_cast<LDirect2DWin>(w->handle());
        if(dw != nullptr){
            std::shared_ptr<LPixmap> pixmap = dw->pixmap();
            if(pixmap != nullptr){
                std::shared_ptr<LDirect2DPixmap> dp =
                    std::dynamic_pointer_cast<LDirect2DPixmap>(pixmap->handle());
                if(dp != nullptr){
                    dp->endPaint();
                }
            }
        }
    }
}

std::shared_ptr<LPaintDevice> LDirect2DBackingStore::paintDevice()
{
    std::shared_ptr<LWindow> w = window();
    if(w != nullptr){
        std::shared_ptr<LDirect2DWin> dw = std::dynamic_pointer_cast<LDirect2DWin>(w->handle());
        if(dw != nullptr){
            return dw->pixmap();
        }
    }
    return nullptr;
}

void LDirect2DBackingStore::flush(std::shared_ptr<LWindow> targetWindow,
                                  const std::shared_ptr<LRegion> region,
                                  const LPoint<int> &offset)
{

    if(targetWindow != nullptr){
        std::shared_ptr<LDirect2DWin> dw = std::dynamic_pointer_cast<LDirect2DWin>(targetWindow->handle());
        if(dw != nullptr){
            std::shared_ptr<LWindow> tWin = window();
            if (targetWindow != tWin) {
                if(tWin != nullptr){
                    std::shared_ptr<LDirect2DWin> tDw = std::dynamic_pointer_cast<LDirect2DWin>(tWin->handle());
                    if(tDw != nullptr){
                        dw->flush(tDw->directPixmap(), region, offset);
                    }
                }
            }else{
                dw->present(region);
            }
        }
    }
}

void LDirect2DBackingStore::resize(const LSize<int> &size,
                                   const std::shared_ptr<LRegion> region)
{

    std::shared_ptr<LWindow> tWin = window();
    if(tWin != nullptr){
        std::shared_ptr<LDirect2DWin> tDw = std::dynamic_pointer_cast<LDirect2DWin>(tWin->handle());
        if(tDw != nullptr){
            std::shared_ptr<LPixmap> oldPixmap = nullptr;
            std::shared_ptr<LPixmap> pixmap = tDw->pixmap();
            if(pixmap != nullptr){
                if(pixmap->size() == size){
                    return;
                }
                //lWarn() << "org size:" << pixmap->width() << "," << pixmap->height();
                oldPixmap =  pixmap->copy();
                pixmap = nullptr; //需要释放，否则交换区大小重置会失败
            }

            tDw->resizeSwapChain(size);
            pixmap = tDw->pixmap();

            if (oldPixmap != nullptr && pixmap != nullptr) {
                std::shared_ptr<LDirect2DPixmap> newDp =
                    std::dynamic_pointer_cast<LDirect2DPixmap>(pixmap->handle());
                newDp->copy(oldPixmap->handle(), region);
            }
        }
    }
}

LK_NAMESPACE_END
