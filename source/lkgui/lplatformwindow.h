#ifndef LPLATFORMWINDOW_H
#define LPLATFORMWINDOW_H

#include "lplatformsurface.h"
#include "lwindow.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LPlatformWindow : public LPlatformSurface
{
public:
    LPlatformWindow(std::shared_ptr<LWindow> win);
    virtual ~LPlatformWindow();

    virtual void initialize();

    std::shared_ptr<LWindow>  window() const;
    std::shared_ptr<LPlatformWindow> parent() const;

    virtual std::shared_ptr<LPlatformScreen> screen() const override;
    virtual LSurfaceFormat format()  const override;
    virtual WId winId() const;
    virtual bool isEmbedded() const{
        return false;
    }

    virtual const LRect<int>& geometry() const;
    virtual void setGeometry(const LRect<int> &rect);
    virtual LRect<int> normalGeometry() const;
    virtual double devicePixelRatio() const;

    virtual bool isActive() const;
    virtual bool isExposed() const;
    virtual bool isForeignWindow() const { return false; }
    virtual bool isAncestorOf(const std::shared_ptr<LPlatformWindow> child) const;
    virtual void setParent(const std::shared_ptr<LPlatformWindow> window);
    virtual void setWindowTitle(const std::wstring &title);
    virtual void setWindowIcon(const std::shared_ptr<LTexture> icon);
    virtual void setWindowFlags(LFlags<LWindowType> flags);
    virtual void setWindowStates(LFlags<LWindowState> states);
    virtual void setOpacity(double level);
    virtual bool setMouseGrabEnabled(bool grab);
    virtual bool setKeyboardGrabEnabled(bool grab);
    virtual void setVisible(bool visible);

    virtual bool windowEvent(std::shared_ptr<LEvent> event);

    virtual LPoint<int> mapToGlobal(const LPoint<int> &pos) const;
    virtual LPoint<int> mapFromGlobal(const LPoint<int> &pos) const;

    virtual void requestActivateWindow();
    virtual void propagateSizeHints();

    virtual LMargins<int> frameMargins() const;

    virtual void setFrameStrutEventsEnabled(bool enabled);
    virtual bool frameStrutEventsEnabled() const;
    virtual bool setWindowModified(bool modified);
    virtual void setMask(std::shared_ptr<LRegion> region);

    virtual void setAlertState(bool enabled);
    virtual bool isAlertState() const;

    virtual bool close();
    virtual void raise();
    virtual void lower();

    static std::shared_ptr<LScreen> initialGeometry(const std::shared_ptr<LWindow> w,
                                      const LRect<int> &initGeo,
                                      int defaultWidth,
                                      int defaultHeight,
                                      LRect<int>& retRect);

    static LRect<double> closestAcceptableGeometry(const std::shared_ptr<LWindow> w,
                                                    const LRect<double> &nativeRect);

protected:
    LRect<int> m_rect;

    std::shared_ptr<LPlatformScreen> screenForGeometry(const LRect<int> &newGeometry) const;
};

LK_NAMESPACE_END

#endif // LPLATFORMWINDOW_H
