#ifndef LDIALOG_H
#define LDIALOG_H

#include "lkwidgets/core/lwidget.h"

LK_NAMESPACE_BEGIN

class LPlatformDialogHelper;
class LButton;
class LKWIDGETSDEF LDialog : public LWidget
{
public:
    LDialog(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);

    enum DialogCode { Rejected, Accepted };

    inline void setModal(bool modal){
        setAttribute(LWidgetAttribute::ShowModal, modal);
    }
    inline void setResult(int r){
        m_rescode = r;
    }
    inline int result() const{
        return m_rescode;
    }
    virtual int exec();
    virtual void open();
    virtual void done(int);
    virtual void accept();
    virtual void reject();
    virtual void setVisible(bool visible) override;

    void setMainDefault(std::shared_ptr<LButton> b);
    void hideDefault();

    LSignal<> sigAccepted;
    LSignal<> sigRejected;
    LSignal<int> sigFinished;

protected:
    virtual void keyPressEvent(std::shared_ptr<LKeyEvent> e) override;
    virtual void closeEvent(std::shared_ptr<LEvent> e) override;
    virtual void showEvent(std::shared_ptr<LEvent> e) override;
    // virtual void contextMenuEvent(std::shared_ptr<LContextMenuEvent> e) override;
    std::shared_ptr<LWindow> transientParentWindow() const;

    void hide(int resultCode);
    void finalize(int resultCode, int dialogCode);
    void adjustPosition(std::shared_ptr<LWidget> w);

    void resetModalitySetByOpen();

    std::shared_ptr<LEventLoop> m_eventLoop;
    std::weak_ptr<LButton> m_mainDef;

    int m_rescode = -1;
    int m_resetModalityTo = -1;
    bool m_wasModalitySet = false;
};

LK_NAMESPACE_END
#endif // LDIALOG_H
