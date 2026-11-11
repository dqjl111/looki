#ifndef LPLATFORMDIALOGHELPER_H
#define LPLATFORMDIALOGHELPER_H
#include "lkcore/lobject.h"
#include "lkcore/io/lurl.h"
#include "lkgui/lwindow.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LPlatformDialogHelper : public LObject
{
public:
    LPlatformDialogHelper();

    enum Type {
        File
    };

    virtual void exec() = 0;
    virtual bool show(LFlags<LWindowType> windowFlags,
                      LWindowModality windowModality,
                      std::shared_ptr<LWindow> parent) = 0;
    virtual void hide() = 0;

    LSignal<> sigAccepted;
    LSignal<> sigRejected;
};


class LKGUIDEF LFileDialogOptions
{
public:
    enum ViewMode : uint8_t{ Detail, List };
    enum FileMode : uint8_t{ AnyFile, ExistingFile, Directory, ExistingFiles, DirectoryOnly };
    enum AcceptMode : uint8_t { AcceptOpen, AcceptSave };
    enum DialogLabel : uint8_t{ LookIn, FileName, FileType, Accept, Reject, DialogLabelCount };


    LFlags<LPathOption> pathOptions;
    std::wstring windowTitle;
    ViewMode viewMode = Detail;
    FileMode fileMode = AnyFile;
    AcceptMode acceptMode = AcceptOpen;
    std::wstring labels[LFileDialogOptions::DialogLabelCount];
    LFlags<LPathFilter> pathFilters = LPathFilter::AllEntries | LPathFilter::NoDotAndDotDot | LPathFilter::AllDirs;
    std::list<std::wstring> nameFilters;
    std::wstring defaultSuffix;
    std::wstring initDirectory;
    std::wstring initNameFilter;
    std::wstring initFileName;

    static std::wstring suffixFromFilter(const std::wstring &filter);
};


class LKGUIDEF LPlatformFileDialogHelper : public LPlatformDialogHelper
{
public:
    LPlatformFileDialogHelper(){}
    std::shared_ptr<LFileDialogOptions> m_options;
    std::wstring m_outPath;

};



LK_NAMESPACE_END

#endif // LPLATFORMDIALOGHELPER_H
