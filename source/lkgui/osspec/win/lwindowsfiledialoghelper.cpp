#include "lwindowsfiledialoghelper.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/osspec/win/lwindowswin.h"
#include "lwindowsctx.h"
#include <vector>

LK_NAMESPACE_BEGIN

LWindowsFileDialogHelper::LWindowsFileDialogHelper() {}

LWindowsFileDialogHelper::~LWindowsFileDialogHelper()
{
    if (m_fileDialog){
        m_fileDialog->Release();
        m_fileDialog = nullptr;
    }
}


HWND LWindowsFileDialogHelper::getHWND(IFileDialog *fileDialog)
{
    IOleWindow *oleWindow = nullptr;
    if (FAILED(fileDialog->QueryInterface(IID_IOleWindow, reinterpret_cast<void **>(&oleWindow)))) {
        lWarn() <<  "Native file dialog: unable to query IID_IOleWindow interface.";
        return HWND(0);
    }

    HWND result(0);
    if (FAILED(oleWindow->GetWindow(&result)))
        lWarn() << "Native file dialog: unable to get dialog's window.";

    oleWindow->Release();
    return result;
}


void LWindowsFileDialogHelper::exec()
{
    if(m_fileDialog == nullptr){
        show(0, LWindowModality::WindowModal, nullptr);
    }
}

bool LWindowsFileDialogHelper::show(LFlags<LWindowType>,
                                    LWindowModality windowModality,
                                    std::shared_ptr<LWindow> parent)
{
    if(windowModality == LWindowModality::NonModal){ //不支持非模式
        return false;
    }

    if (parent == nullptr){
        parent = LGuiApplication::focusWindow(); // Need a parent window, else the application loses activation when closed.
    }
    if (parent == nullptr) {
        m_ownerWindow = LWindowsWin::handleOf(parent);
    } else {
        m_ownerWindow = nullptr;
    }

    if (!ensureNativeDialog()){
        return false;
    }
    // Show the dialog.
    HRESULT result = m_fileDialog->Show(m_ownerWindow);
    if (SUCCEEDED(result)) {
        // Get the file name
        IShellItem* psiResult;
        result = m_fileDialog->GetResult(&psiResult);
        if (!SUCCEEDED(result)) {
            lWarn() << "Could not get shell item from dialog.";
            sigRejected.emt();
            return false;
        }

        LPWSTR name;
        result = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &name);
        if (!SUCCEEDED(result)) {
            lWarn() << "Could not get file path from shell item returned by dialog.";
            psiResult->Release();
            sigRejected.emt();
            return false;
        }

        m_outPath = name;
        CoTaskMemFree(name);
        psiResult->Release();

        sigAccepted.emt();
        return true;
    } else if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        sigRejected.emt();
        return true;
    } else {
        lWarn() << "File dialog box show failed.";
        sigRejected.emt();
        return false;
    }
}

void LWindowsFileDialogHelper::hide()
{
    close();
    if (m_fileDialog){
        m_fileDialog->Release();
        m_fileDialog = nullptr;
    }
    m_ownerWindow = nullptr;
}


bool LWindowsFileDialogHelper::init(const CLSID &clsId, const IID &iid)
{
    // Create dialog
    HRESULT result = CoCreateInstance(clsId,
                                    nullptr,
                                    CLSCTX_ALL,
                                    iid,
                                    reinterpret_cast<void**>(&m_fileDialog));

    if (!SUCCEEDED(result)) {
        lWarn() << "Could not create file dialog.";
        return false;
    }
    return true;
}

bool LWindowsFileDialogHelper::ensureNativeDialog()
{
    if(m_fileDialog != nullptr){
        return true;
    }

    if(m_options == nullptr){
        return false;
    }

    if (m_options->acceptMode == LFileDialogOptions::AcceptOpen) {
        if(init(CLSID_FileOpenDialog, IID_IFileOpenDialog) == false){
            return false;
        }
    } else {
        if(init(CLSID_FileSaveDialog, IID_IFileSaveDialog) == false){
            return false;
        }
    }

    if(!m_options->windowTitle.empty()){
        m_fileDialog->SetTitle(m_options->windowTitle.data());
    }

    applyMode();
    applyLabelNames();
    applyNameFilters();
    applySuffix();
    applyInitDirectory();
    applyInitFile();
    return true;
}

// Split a list of name filters into description and actual filters
struct LFilterSpec
{
    std::wstring description;
    std::wstring filter;
};

static std::list<LFilterSpec> filterSpecs(const std::list<std::wstring> &filters, bool hideFilterDetails)
{
    std::list<LFilterSpec> result;
    // Split filter specification as 'Texts (*.txt[;] *.doc)', '*.txt[;] *.doc'
    // into description and filters specification as '*.txt;*.doc'
    for (const std::wstring &filterString : filters) {
        const int openingParenPos = filterString.find(wchar_t('('));
        const int closingParenPos = openingParenPos != -1 ?
                                        filterString.find(wchar_t(')'), openingParenPos + 1) : -1;
        LFilterSpec filterSpec;
        filterSpec.filter = closingParenPos == -1 ?
                                filterString :
                                filterString.substr(openingParenPos + 1, closingParenPos - openingParenPos - 1);

        filterSpec.filter = LStringUtil::replace(L" ", L";", LStringUtil::trim(filterSpec.filter));
        if (filterSpec.filter.empty()){
            filterSpec.filter = L"*";
        }

        filterSpec.description = filterString;
        if(hideFilterDetails){
            if(openingParenPos>=0){
                filterSpec.description = LStringUtil::trim(filterSpec.description.substr(0, openingParenPos));
            }
        }else{
            if (!filterSpec.filter.starts_with(L"*.")) {
                if(openingParenPos>=0){
                    filterSpec.description = LStringUtil::trim(filterSpec.description.substr(0, openingParenPos));
                }
            }
        }
        result.push_back(filterSpec);
    }
    return result;
}

void LWindowsFileDialogHelper::applyNameFilters()
{
    const std::list<std::wstring> & nameFilters = m_options->nameFilters;
    bool hideFiltersDetails = m_options->pathOptions & LPathOption::HideNameFilterDetails;


    const std::list<LFilterSpec> specs = filterSpecs(nameFilters, hideFiltersDetails);
    std::vector<COMDLG_FILTERSPEC> comFilterSpec(specs.size());

    // Split filter specification as 'Texts (*.txt[;] *.doc)'
    // into description and filters specification as '*.txt;*.doc'

    int i=0;
    for (const LFilterSpec& spec : specs) {
        COMDLG_FILTERSPEC& t = comFilterSpec.at(i);
        t.pszName = spec.description.data();
        t.pszSpec = spec.filter.data();
        ++i;
    }

    m_fileDialog->SetFileTypes(comFilterSpec.size(), comFilterSpec.data());

    if (m_options->acceptMode == LFileDialogOptions::AcceptSave){
        if (m_options->defaultSuffix.empty()) {
            for (const std::wstring &filter : nameFilters) {
                const std::wstring suffix = LFileDialogOptions::suffixFromFilter(filter);
                if (!suffix.empty()) {
                    m_fileDialog->SetDefaultExtension(suffix.data());
                    break;
                }
            }
        }
    }

    if(m_options->initNameFilter.empty()){
        if (m_options->fileMode != LFileDialogOptions::Directory
            && m_options->fileMode != LFileDialogOptions::DirectoryOnly) {
            int index= 0;
            for (const std::wstring &filter : nameFilters) {
                if(filter.contains(m_options->initNameFilter)){
                    m_fileDialog->SetFileTypeIndex(index + 1); // one-based.
                    break;
                }
                ++index;
            }
        }
    }
}

void LWindowsFileDialogHelper::applyLabelNames()
{
    {
        std::wstring& name = m_options->labels[LFileDialogOptions::FileName];
        if(!name.empty()){
            m_fileDialog->SetFileNameLabel(name.data());
        }
    }
    {
        std::wstring& name = m_options->labels[LFileDialogOptions::Accept];
        if(!name.empty()){
            m_fileDialog->SetOkButtonLabel(name.data());
        }
    }
    {
        std::wstring& name = m_options->labels[LFileDialogOptions::Reject];
        if(!name.empty()){
            IFileDialog2 *result;
            if(SUCCEEDED(m_fileDialog->QueryInterface(IID_IFileDialog2, reinterpret_cast<void **>(&result)))){
                result->SetCancelButtonLabel(name.data());
            }
        }
    }
}

void LWindowsFileDialogHelper::applyMode()
{
    DWORD flags = FOS_PATHMUSTEXIST;
    if (LWindowsCtx::readAdvancedExplorerSettings(L"Hidden", 1) == 1){ // 1:show, 2:hidden
        flags |= FOS_FORCESHOWHIDDEN;
    }
    if (m_options->pathOptions & LPathOption::DontResolveSymlinks){
        flags |= FOS_NODEREFERENCELINKS;
    }
    switch (m_options->fileMode) {
    case LFileDialogOptions::AnyFile:
        if (m_options->acceptMode == LFileDialogOptions::AcceptSave){
            flags |= FOS_NOREADONLYRETURN;
        }
        if (!(m_options->pathOptions & LPathOption::DontConfirmOverwrite)){
            flags |= FOS_OVERWRITEPROMPT;
        }
        break;
    case LFileDialogOptions::ExistingFile:{
        flags |= FOS_FILEMUSTEXIST;
        break;
    }
    case LFileDialogOptions::Directory:
    case LFileDialogOptions::DirectoryOnly:{
        flags |= FOS_PICKFOLDERS | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM;
        break;
    }
    case LFileDialogOptions::ExistingFiles:{
        flags |= FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT;
        break;
    }
    default:{
        break;
    }
    }

    if (FAILED(m_fileDialog->SetOptions(flags))){
        lWarn() << "Set file dialog options failed";
    }
}

void LWindowsFileDialogHelper::applySuffix()
{
    if(!m_options->defaultSuffix.empty()){
        m_fileDialog->SetDefaultExtension(m_options->defaultSuffix.data());
    }
}

void LWindowsFileDialogHelper::applyInitDirectory()
{
    if(!m_options->initDirectory.empty()){
        IShellItem *result = nullptr;
        const HRESULT hr =
            SHCreateItemFromParsingName(m_options->initDirectory.data(),
                                        nullptr, IID_IShellItem,
                                        reinterpret_cast<void **>(&result));
        if (FAILED(hr)) {
            lWarn() << "SHCreateItemFromParsingName failed:" << m_options->initDirectory;
        }

        m_fileDialog->SetFolder(result);
        result->Release();
    }
}

void LWindowsFileDialogHelper::applyInitFile()
{
    if(!m_options->initFileName.empty()){
        m_fileDialog->SetFileName(m_options->initFileName.data());
    }
}

void LWindowsFileDialogHelper::close()
{
    if(m_fileDialog != nullptr){
        m_fileDialog->Close(S_OK);
        const HWND hwnd = getHWND(m_fileDialog);

        if (hwnd && IsWindowVisible(hwnd)){
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            sigRejected.emt();
        }
    }
}



LK_NAMESPACE_END
