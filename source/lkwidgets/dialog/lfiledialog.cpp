#include "lfiledialog.h"
#include "lkgui/lguiapplication.h"
#include <filesystem>

LK_NAMESPACE_BEGIN

LFileDialog::LFileDialog(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f):LDialog(parent, f)
{
    m_options = std::make_shared<LFileDialogOptions>();
}


void LFileDialog::setVisible(bool visible)
{
    std::shared_ptr<LPlatformDialogHelper> helper = platformHelper();
    if (helper != nullptr) {
        if (visible) {
            setAttribute(LWidgetAttribute::DontShowOnScreen);

            m_options->windowTitle = windowTitle();
            m_platformUsed = helper->show(windowFlags(),windowModality(), transientParentWindow());
        } else if (m_platformUsed) {
            helper->hide();
        }
    }
}

int LFileDialog::exec()
{
    resetModalitySetByOpen();

    bool wasShowModal = testAttribute(LWidgetAttribute::ShowModal);
    setAttribute(LWidgetAttribute::ShowModal, true);
    setResult(0);

    show();
    if (m_platformHelper != nullptr) {
        m_platformHelper->exec();
    }
    setAttribute(LWidgetAttribute::ShowModal, wasShowModal);
    return m_rescode;
}

const std::wstring &LFileDialog::selectedFile() const
{
    if(m_platformHelper != nullptr){
        return m_platformHelper->m_outPath;
    }
    return LStringUtil::wempty;
}

LFlags<LPathOption> LFileDialog::pathOptions() const
{
    return m_options->pathOptions;
}

LFileDialogOptions::ViewMode LFileDialog::viewMode() const
{
    return m_options->viewMode;
}

LFileDialogOptions::FileMode LFileDialog::fileMode() const
{
    return m_options->fileMode;
}

LFileDialogOptions::AcceptMode LFileDialog::acceptMode() const
{
    return m_options->acceptMode;
}

const std::wstring &LFileDialog::labelText(LFileDialogOptions::DialogLabel id) const
{
    if(id >= 0 && id< LFileDialogOptions::DialogLabelCount){
        return m_options->labels[id];
    }
    return LStringUtil::wempty;
}

LFlags<LPathFilter> LFileDialog::pathFilters() const
{
    return m_options->pathFilters;
}

const std::list<std::wstring>& LFileDialog::nameFilters() const
{
    return m_options->nameFilters;
}

const std::wstring &LFileDialog::defaultSuffix() const
{
    return m_options->defaultSuffix;
}

const std::wstring &LFileDialog::initDirectory() const
{
    return m_options->initDirectory;
}

const std::wstring &LFileDialog::initNameFilter() const
{
    return m_options->initNameFilter;
}

const std::wstring &LFileDialog::initFileName() const
{
    return m_options->initFileName;
}

void LFileDialog::setPathOptions(LFlags<LPathOption> opt)
{
    m_options->pathOptions = opt;
}

void LFileDialog::setViewMode(LFileDialogOptions::ViewMode mode)
{
    m_options->viewMode = mode;
}

void LFileDialog::setFileMode(LFileDialogOptions::FileMode mode)
{
    m_options->fileMode = mode;
}

void LFileDialog::setAcceptMode(LFileDialogOptions::AcceptMode mode)
{
    m_options->acceptMode = mode;
}

void LFileDialog::setLabelText(LFileDialogOptions::DialogLabel id, const std::wstring &s)
{
    if(id >= 0 && id< LFileDialogOptions::DialogLabelCount){
        m_options->labels[id] = s;
    }
}

void LFileDialog::setPathFilters(LFlags<LPathFilter> f)
{
    m_options->pathFilters = f;
}

void LFileDialog::setNameFilters(const std::list<std::wstring> &f)
{
    m_options->nameFilters = f;
}

void LFileDialog::setDefaultSuffix(const std::wstring &s)
{
    m_options->defaultSuffix = s;
}

void LFileDialog::setInitDirectory(const std::wstring &s)
{
    m_options->initDirectory = s;
}

void LFileDialog::setInitNameFilter(const std::wstring &s)
{
    m_options->initNameFilter = s;
}

void LFileDialog::setInitFileName(const std::wstring &s)
{
    m_options->initFileName= s;
}

std::wstring LFileDialog::getOpenFile(const std::wstring &defaultFile, const std::wstring &filter, const std::wstring &title, std::shared_ptr<LWidget> parent, bool isExistOnly, bool initSuffix)
{
    std::list<std::wstring> filters;
    if(!filter.empty()){
        filters = LStringUtil::split(filter, L";;", true);
    }

    if(isExistOnly){
        return getFile(defaultFile, filters, title, parent, LFileDialogOptions::AcceptOpen, LFileDialogOptions::ExistingFile, initSuffix);
    }else{
        return getFile(defaultFile, filters, title, parent, LFileDialogOptions::AcceptOpen, LFileDialogOptions::AnyFile, initSuffix);
    }
}

std::wstring LFileDialog::getSaveFile(const std::wstring &defaultFile, const std::wstring &filter, const std::wstring &title, std::shared_ptr<LWidget> parent, bool initSuffix)
{
    std::list<std::wstring> filters;
    if(!filter.empty()){
        filters = LStringUtil::split(filter, L";;", true);
    }
    return getFile(defaultFile, filters, title, parent, LFileDialogOptions::AcceptSave, LFileDialogOptions::AnyFile, initSuffix);
}

std::wstring LFileDialog::getFile(const std::wstring &defaultFile, const std::list<std::wstring> &filters, const std::wstring &title, std::shared_ptr<LWidget> parent, LFileDialogOptions::AcceptMode acceptMode, LFileDialogOptions::FileMode fileMode, bool initSuffix)
{

    std::shared_ptr<LFileDialog> dlg = makeInit<LFileDialog>(parent);
    dlg->setAcceptMode(acceptMode);
    dlg->setInitDirectory(std::filesystem::current_path().wstring());
    dlg->setWindowTitle(title);
    dlg->setFileMode(fileMode);
    dlg->setPathOptions(LPathOption::DontConfirmOverwrite);


    if(!defaultFile.empty()){
        dlg->setInitFileName(defaultFile);
    }

    if(!filters.empty()){
        dlg->setNameFilters(filters);

        if(initSuffix){
            bool hasSetDefaultSuffix  =false;
            for(const std::wstring& filter : filters){
                if(filter.contains(L".")){
                    std::wstring t = filter;
                    size_t tPos = t.find(L"(");
                    if(tPos >=0){
                        t = t.substr(tPos +1);
                        tPos = t.find(L")");
                        if(tPos>=0){
                            t = t.substr(0, tPos);
                        }
                    }

                    std::wstring suffix = LStringUtil::trim(t.substr(t.find(L".")+1));
                    if(defaultFile.ends_with(suffix)){
                        hasSetDefaultSuffix = true;
                        dlg->setDefaultSuffix(suffix);
                    }

                    if(!hasSetDefaultSuffix){
                        hasSetDefaultSuffix = true;
                        dlg->setDefaultSuffix(suffix);
                    }
                }
            }
        }
    }


    std::wstring ret;
    if(dlg->exec() == LDialog::Accepted){
        ret = dlg->selectedFile();
        if(!ret.empty()){
            std::filesystem::path p(ret);
            if(std::filesystem::is_directory(p)){
                std::filesystem::current_path(p);
            }else{
                std::filesystem::current_path(p.parent_path());
            }
        }
    }
    return ret;
}


std::shared_ptr<LPlatformDialogHelper> LFileDialog::platformHelper() const
{
    if(m_platformHelper != nullptr){
        return m_platformHelper;
    }
    const_cast<LFileDialog*>(this)->initPlatformHeler();
    return m_platformHelper;
}

void LFileDialog::initPlatformHeler()
{
    std::shared_ptr<LPlatformDialogHelper> t =
        LGuiApplication::platformIntegration()->createPlatformDialogHelper(LPlatformDialogHelper::File);

    const_cast<LFileDialog*>(this)->m_platformHelper = std::dynamic_pointer_cast<LPlatformFileDialogHelper>(t);
    m_platformHelper->m_options = m_options;
    m_platformHelper->sigRejected.connect((LDialog*)this, &LDialog::reject, ConnectionType::Direct);
    m_platformHelper->sigAccepted.connect((LDialog*)this, &LDialog::accept, ConnectionType::Direct);
}


LK_NAMESPACE_END
