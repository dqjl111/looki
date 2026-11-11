#ifndef LFILEDIALOG_H
#define LFILEDIALOG_H

#include "lkwidgets/dialog/ldialog.h"
#include "lkcore/io/li18n.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LFileDialog : public LDialog
{
public:
    LFileDialog(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);
    virtual void setVisible(bool visible) override;
    virtual int exec() override;
    const std::wstring& selectedFile() const;


    LFlags<LPathOption> pathOptions() const;
    LFileDialogOptions::ViewMode viewMode() const;
    LFileDialogOptions::FileMode fileMode() const;
    LFileDialogOptions::AcceptMode acceptMode() const;
    const std::wstring& labelText(LFileDialogOptions::DialogLabel id) const;
    LFlags<LPathFilter> pathFilters() const;
    const std::list<std::wstring>& nameFilters() const;
    const std::wstring& defaultSuffix() const;
    const std::wstring& initDirectory() const;
    const std::wstring& initNameFilter() const;
    const std::wstring& initFileName() const;

    void setPathOptions(LFlags<LPathOption> opt);
    void setViewMode(LFileDialogOptions::ViewMode mode);
    void setFileMode(LFileDialogOptions::FileMode mode);
    void setAcceptMode(LFileDialogOptions::AcceptMode mode);
    void setLabelText(LFileDialogOptions::DialogLabel id, const std::wstring& s);
    void setPathFilters(LFlags<LPathFilter> f);
    void setNameFilters(const std::list<std::wstring>& f);
    void setDefaultSuffix(const std::wstring& s);
    void setInitDirectory(const std::wstring& s);
    void setInitNameFilter(const std::wstring& s);
    void setInitFileName(const std::wstring& s);


    static std::wstring getOpenFile(const std::wstring& defaultFile=L"",
                               const std::wstring& filter=L"",
                               const std::wstring& title=L"",
                               std::shared_ptr<LWidget>  parent = nullptr,
                               bool isExistOnly = false,
                               bool initSuffix = true);

    static std::wstring getSaveFile(const std::wstring& defaultFile=L"",
                               const std::wstring& filter=L"",
                               const std::wstring& title=L"",
                               std::shared_ptr<LWidget>  parent = nullptr,
                               bool initSuffix = true);

    static std::wstring getFile(const std::wstring& defaultFile,
                           const std::list<std::wstring>& filters,
                           const std::wstring& title,
                           std::shared_ptr<LWidget>  parent,
                           LFileDialogOptions::AcceptMode acceptMode,
                           LFileDialogOptions::FileMode fileMode = LFileDialogOptions::AnyFile,
                           bool initSuffix = true);
protected:
    virtual std::shared_ptr<LPlatformDialogHelper> platformHelper() const;
    void initPlatformHeler();

    std::shared_ptr<LFileDialogOptions> m_options;
    std::shared_ptr<LPlatformFileDialogHelper> m_platformHelper;
    bool m_platformUsed = false;
};

LK_NAMESPACE_END

#endif // LFILEDIALOG_H
