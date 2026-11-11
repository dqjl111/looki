#ifndef LWINDOWSFILEDIALOGHELPER_H
#define LWINDOWSFILEDIALOGHELPER_H
#include "lwindows.h"
#include <shlobj.h>
#include <shlwapi.h>
#include "lkgui/lplatformdialoghelper.h"

LK_NAMESPACE_BEGIN

class LWindowsFileDialogHelper : public LPlatformFileDialogHelper
{
public:
    LWindowsFileDialogHelper();
    virtual ~LWindowsFileDialogHelper();

    virtual void exec() override;
    virtual bool show(LFlags<LWindowType> windowFlags,
                      LWindowModality windowModality,
                      std::shared_ptr<LWindow> parent) override;
    virtual void hide() override;
protected:
    bool init(const CLSID &clsId, const IID &iid);
    inline bool ensureNativeDialog();
    void applyNameFilters();
    void applyLabelNames();
    void applyMode();
    void applySuffix();
    void applyInitDirectory();
    void applyInitFile();
    void close();

    HWND getHWND(IFileDialog *fileDialog);
    IFileDialog *m_fileDialog = nullptr;
    HWND m_ownerWindow = nullptr;

};


LK_NAMESPACE_END

#endif // LWINDOWSFILEDIALOGHELPER_H
