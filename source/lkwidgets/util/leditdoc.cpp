#include "leditdoc.h"
#include "lkcore/io/liofileman.h"
#include "lkcore/io/liomemoryman.h"
#include "lmultilineeditor.h"
#include "llineeditor.h"
#include "ltextlineeditor.h"
#include "lmultitextlineeditor.h"

LK_NAMESPACE_BEGIN

std::map<uint32_t, LEditBaseCreator> LEditDoc::m_globalCreators;

LEditDoc::LEditDoc()
{

}

bool LEditDoc::saveCbor(const std::string &filePath)
{
    if(m_editor != nullptr){
        LIOFileMan man(filePath, std::ios_base::out| std::ios_base::trunc| std::ios_base::binary);
        if(man.open()){
            man << m_editor->toCbor();
            return true;
        }
    }
    return false;
}

bool LEditDoc::loadCbor(const std::string &filePath)
{
    LIOFileMan man(filePath, std::ios_base::in);
    if(man.isOpen()){
        LCbor cbor;
        man >> cbor;
        return loadCbor(cbor);
    }
    return false;
}

bool LEditDoc::loadCbor(const std::vector<uint8_t> &data, bool canClip)
{
    if(data.empty()){
        return false;
    }

    LIOMemoryMan man(data);
    LCbor cbor;
    man >> cbor;
    return loadCbor(cbor);
}

bool LEditDoc::loadCbor(const LCbor &cbor)
{
    if(cbor.isMap()){
        LCbor::map cborMap = cbor.toMap();
        return initEditor(cborMap);
    }
    return false;
}

std::shared_ptr<LEditBase> LEditDoc::createBase(uint32_t type, bool tryGlobal) const
{
    const std::map<uint32_t, LEditBaseCreator>* dcs = docCreators();
    if(dcs != nullptr){
        std::map<uint32_t, LEditBaseCreator>::const_iterator iter = dcs->find(type);
        if(iter != dcs->cend()){
            LEditBaseCreator creator = iter->second;
            return creator();
        }
    }

    if(tryGlobal){
        return createGlobalBase(type);
    }
    return nullptr;
}

std::shared_ptr<LEditBase> LEditDoc::createBase(const LCbor &cbor,
                                                bool tryGlobal,
                                                std::shared_ptr<LEditDoc> doc)
{
    if(cbor.isMap()){
        LCbor::map cborMap = cbor.toMap();
        LCbor::map::const_iterator mapIter = cborMap.find(uint32_t(LEditBase::CborId::Type));
        if(mapIter != cborMap.end()){
            const LCbor& cbor = mapIter->second;
            const uint32_t ttype = (uint32_t)cbor.toUInt64();
            std::shared_ptr<LEditBase> base = nullptr;
            if(doc != nullptr){
                base = doc->createBase(ttype, tryGlobal);
            }else if(tryGlobal){
                base = createGlobalBase(ttype);
            }

            if(base != nullptr){
                base->fromCbor(cborMap, false, true, doc);
                return base;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<LEditBase> LEditDoc::createGlobalBase(uint32_t type)
{
    std::map<uint32_t, LEditBaseCreator>::const_iterator iter = m_globalCreators.find(type);
    if(iter != m_globalCreators.cend()){
        LEditBaseCreator creator = iter->second;
        return creator();
    }
    return nullptr;
}

std::shared_ptr<LEditBase> LEditDoc::createGlobalBase(const LCbor &cbor)
{
    if(cbor.isMap()){
        LCbor::map cborMap = cbor.toMap();
        LCbor::map::const_iterator mapIter = cborMap.find(uint32_t(LEditBase::CborId::Type));
        if(mapIter != cborMap.end()){
            const LCbor& cbor = mapIter->second;
            const uint32_t ttype = (uint32_t)cbor.toUInt64();
            std::map<uint32_t, LEditBaseCreator>::const_iterator iter = m_globalCreators.find(ttype);
            if(iter != m_globalCreators.cend()){
                LEditBaseCreator creator = iter->second;
                std::shared_ptr<LEditBase> base = creator();
                base->fromCbor(cborMap, false, true, nullptr);
                return base;
            }
        }
    }
    return nullptr;
}

void LEditDoc::registGlobalBase(uint32_t type, const LEditBaseCreator &fn)
{
    m_globalCreators[type] = fn;
}

void LEditDoc::removeGlobalBase(uint32_t type)
{
    std::map<uint32_t, LEditBaseCreator>::const_iterator iter = m_globalCreators.find(type);
    if(iter != m_globalCreators.cend()){
        m_globalCreators.erase(iter);
    }
}

void LEditDoc::initGlobal()
{
    if(m_globalCreators.empty()){
        m_globalCreators[uint32_t(LEditType::Container)] = makeEditCreator<LEditContainer>();
        m_globalCreators[uint32_t(LEditType::Row)] = makeEditCreator<LEditRow>();
        m_globalCreators[uint32_t(LEditType::Rows)] = makeEditCreator<LEditRows>();
        m_globalCreators[uint32_t(LEditType::Text)] = makeEditCreator<LEditText>();
        m_globalCreators[uint32_t(LEditType::Editor)] = makeEditCreator<LMultiLineEditor>();
    }
}

bool LEditDoc::initEditor(const LCbor::map& cborMap)
{
    LCbor::map::const_iterator jobIter = cborMap.find(uint32_t(LEditBase::CborId::Job));
    if(jobIter != cborMap.end()){
        const LCbor& jobCbor = jobIter->second;
        const uint32_t job = (uint32_t)jobCbor.toUInt64();

        if(m_editor == nullptr || m_editor->job() != job){

            switch(job){
            case (uint32_t)LEditJob::Line:{
                m_editor = std::make_shared<LLineEditor>();
                break;
            }
            case (uint32_t)LEditJob::Doc:{
                m_editor = std::make_shared<LMultiLineEditor>();
                break;
            }
            case (uint32_t)LEditJob::TextLine:{
                m_editor = std::make_shared<LTextLineEditor>();
                break;
            }
            case (uint32_t)LEditJob::TextDoc:{
                m_editor = std::make_shared<LMultiTextLineEditor>();
                break;
            }
            default:{
                break;
            }
            }

            if(m_editor != nullptr){
                std::shared_ptr<LEditDoc>  doc = getPtrT<LEditDoc>();
                m_editor->setDoc(doc);
                m_editor->fromCbor(cborMap);
            }
        }
    }
    return false;
}

const std::map<uint32_t, LEditBaseCreator> *LEditDoc::docCreators() const
{
    return nullptr;
}


LK_NAMESPACE_END
