#include "lsqlquery.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN
LSqlQuery::LSqlQuery(std::shared_ptr<LSqlResult> result)
{
    m_result = result;
}

LSqlQuery::LSqlQuery(std::shared_ptr<LSqlDatabase> db)
{
    if(db != nullptr){
        std::shared_ptr<LSqlDriver> driver = db->driver();
        if(driver != nullptr){
            m_result = driver->createResult();
        }
    }
}

LSqlQuery::LSqlQuery(const std::string &query, std::shared_ptr<LSqlDatabase> db)
{
    if(db != nullptr){
        std::shared_ptr<LSqlDriver> driver = db->driver();
        if(driver != nullptr){
            m_result = driver->createResult();
        }
    }

    if(!query.empty()){
        exec(query);
    }
}

LSqlQuery::LSqlQuery(const LSqlQuery &org)
{
    m_result = org.m_result;
}

LSqlQuery::LSqlQuery(const LSqlQuery &&org)
{
    m_result = org.m_result;
}

std::shared_ptr<LSqlRecord> LSqlQuery::record() const
{
    if(m_result != nullptr){
        return m_result->record();
    }
    return nullptr;
}

bool LSqlQuery::next()
{
    if(m_result != nullptr){
        if (m_result->isSelect() && m_result->isActive()){
            switch (m_result->row()) {
            case (int)LSqlLocation::BeforeFirstRow:{
                return m_result->fetchFirst();
            }
            case (int)LSqlLocation::AfterLastRow:{
                return false;
            }
            default:{
                if (!m_result->fetchNext()) {
                    m_result->setRow((int)LSqlLocation::AfterLastRow);
                    return false;
                }
                return true;
            }
            }
        }
    }
    return false;
}

bool LSqlQuery::previous()
{
    if(m_result != nullptr){
        if(m_result->isForwardOnly()){
            lWarn() << "LSqlQuery::seek: cannot seek backwards in a forward only query";
            return false;
        }

        if (m_result->isSelect() && m_result->isActive()){
            switch (m_result->row()) {
            case (int)LSqlLocation::BeforeFirstRow:{
                return false;
            }
            case (int)LSqlLocation::AfterLastRow:{
                return m_result->fetchLast();
            }
            default:{
                if (!m_result->fetchPrevious()) {
                    m_result->setRow((int)LSqlLocation::BeforeFirstRow);
                    return false;
                }
                return true;
            }
            }
        }
    }
    return false;
}

bool LSqlQuery::first(){
    if(m_result != nullptr){
        if (m_result->isSelect() && m_result->isActive()){
            if(m_result->isForwardOnly()){
                if(m_result->row() > int(LSqlLocation::BeforeFirstRow)){
                    lWarn() << "LSqlQuery::seek: cannot seek backwards in a forward only query";
                    return false;
                }
            }
            return m_result->fetchFirst();
        }
    }
    return false;

}

bool LSqlQuery::last(){
    if(m_result != nullptr){
        if (m_result->isSelect() && m_result->isActive()){
            return m_result->fetchLast();
        }
    }
    return false;
}

void LSqlQuery::clear(){
    if(m_result != nullptr){
        m_result->clearLastError();
        m_result->clear();
    }
}

bool LSqlQuery::exec(){
    if(m_result != nullptr){
        m_result->resetBindCount();

        if (m_result->lastError().isValid()){
            m_result->clearLastError();
        }
        return m_result->exec();
    }
    return false;

}

bool LSqlQuery::exec(const std::string &query)
{
    std::shared_ptr<LSqlDriver> dr = driver();
    if (dr == nullptr) {
        lWarn() << "LSqlQuery::exec: called before driver has been set up";
        return false;
    }

    m_result->clear();
    m_result->setActive(false);
    m_result->clearLastError();
    m_result->setRow((int)LSqlLocation::BeforeFirstRow);
    m_result->setPrecisionPolicy(m_result->precisionPolicy());

    m_result->setQuery(query);
    if (!dr->isOpen() || dr->isOpenError()) {
        lWarn() << "LSqlQuery::exec: database not open";
        return false;
    }
    if (query.empty()) {
        lWarn() << "LSqlQuery::exec: empty query";
        return false;
    }

    return m_result->reset(query);
}

bool LSqlQuery::prepare(const std::string &query)
{
    if(m_result != nullptr){

        m_result->setActive(false);
        m_result->clearLastError();
        m_result->setRow((int)LSqlLocation::BeforeFirstRow);
        m_result->setPrecisionPolicy(m_result->precisionPolicy());

        std::shared_ptr<LSqlDriver> dr = driver();
        if (dr == nullptr) {
            lWarn() << "LSqlQuery::prepare: no driver";
            return false;
        }
        if (!dr->isOpen() || dr->isOpenError()) {
            lWarn() << "LSqlQuery::prepare: database not open";
            return false;
        }
        if (query.empty()) {
            lWarn() << "LSqlQuery::prepare: empty query";
            return false;
        }
        return m_result->savePrepare(query);
    }
    return false;
}

void LSqlQuery::bindValue(const std::string& placeholder, const LVariant& val, LFlags<LSqlIoFlag> type){
    if(m_result != nullptr){
        m_result->bindValue(placeholder, val, type);
    }
}

void LSqlQuery::bindValue(int pos, const LVariant& val, LFlags<LSqlIoFlag> type){
    if(m_result != nullptr){
        m_result->bindValue(pos, val, type);
    }
}

void LSqlQuery::addBindValue(const LVariant &val, LFlags<LSqlIoFlag> type)
{
    if(m_result != nullptr){
        m_result->addBindValue(val, type);
    }
}

const LVariant &LSqlQuery::boundValue(const std::string &placeholder) const
{
    if(m_result != nullptr){
        return m_result->boundValue(placeholder);
    }
    return LVariant::staticNull();
}

const LVariant &LSqlQuery::boundValue(int index) const
{
    if(m_result != nullptr){
        return m_result->boundValue(index);
    }
    return LVariant::staticNull();
}

std::list<LVariant> LSqlQuery::boundValues() const
{
    if(m_result != nullptr){
        return m_result->boundValues();
    }
    return std::list<LVariant>();
}

const LVariant &LSqlQuery::value(int index) const
{
    if(m_result != nullptr){
        if (m_result->isActive() && m_result->isValid() && (index > -1)){
            return m_result->data(index);
        }
    }
    return LVariant::staticNull();
}

const LVariant &LSqlQuery::value(const std::string &name) const
{
    if(m_result != nullptr){
        std::shared_ptr<LSqlRecord> record = m_result->record();
        if(record != nullptr){
            int index = record->indexOf(name);
            if (m_result->isActive() && m_result->isValid() && (index > -1)){
                return m_result->data(index);
            }
        }
    }
    return LVariant::staticNull();
}

bool LSqlQuery::isValid() const
{
    if(m_result == nullptr){
        return false;
    }
    return m_result->isValid();
}

bool LSqlQuery::isActive() const
{
    if(m_result == nullptr){
        return false;
    }
    return m_result->isActive();
}

bool LSqlQuery::isSelect() const
{
    if(m_result == nullptr){
        return false;
    }
    return m_result->isSelect();
}

bool LSqlQuery::isForwardOnly() const
{
    if(m_result == nullptr){
        return false;
    }
    return m_result->isForwardOnly();
}

void LSqlQuery::setForwardOnly(bool forward)
{
    if(m_result != nullptr){
        return m_result->setForwardOnly(forward);
    }
}

int LSqlQuery::row() const
{
    if(m_result != nullptr){
        return m_result->row();
    }
    return (int)LSqlLocation::BeforeFirstRow;
}

const std::string &LSqlQuery::lastQuery() const
{
    if(m_result != nullptr){
        return m_result->lastQuery();
    }
    return LStringUtil::empty;
}

int LSqlQuery::numRowsAffected() const
{
    if(m_result != nullptr){
        return m_result->numRowsAffected();
    }
    return 0;
}

void LSqlQuery::setPositionalBindingEnabled(bool enable)
{
    if(m_result != nullptr){
        m_result->setPositionalBindingEnabled(enable);
    }
}

bool LSqlQuery::isPositionalBindingEnabled() const
{
    if(m_result != nullptr){
        return m_result->isPositionalBindingEnabled();
    }
    return false;
}

bool LSqlQuery::seek(int index, bool relative)
{
    if(m_result != nullptr){
        if (m_result->isActive() && m_result->isValid()){

            int actualIdx;
            if (!relative) { // arbitrary seek
                if (index < 0) {
                    m_result->setRow((int)LSqlLocation::BeforeFirstRow);
                    return false;
                }
                actualIdx = index;
            } else {
                switch (m_result->row()) { // relative seek
                case (int)LSqlLocation::BeforeFirstRow:{
                    if (index > 0)
                        actualIdx = index - 1;
                    else {
                        return false;
                    }
                    break;
                }
                case (int)LSqlLocation::AfterLastRow:{
                    if (index < 0) {
                        m_result->fetchLast();
                        actualIdx = m_result->row() + index + 1;
                    } else {
                        return false;
                    }
                    break;
                }
                default:{
                    if ((m_result->row() + index) < 0) {
                        m_result->setRow((int)LSqlLocation::BeforeFirstRow);
                        return false;
                    }
                    actualIdx = m_result->row() + index;
                    break;
                }
                }
            }
            // let drivers optimize
            if (isForwardOnly() && actualIdx < m_result->row()) {
                lWarn() << "LSqlQuery::seek: cannot seek backwards in a forward only query";
                return false;
            }
            if (actualIdx == (m_result->row() + 1) && m_result->row() != (int)LSqlLocation::BeforeFirstRow) {
                if (!m_result->fetchNext()) {
                    m_result->setRow((int)LSqlLocation::AfterLastRow);
                    return false;
                }
                return true;
            }
            if (actualIdx == (m_result->row() - 1)) {
                if (!m_result->fetchPrevious()) {
                    m_result->setRow((int)LSqlLocation::BeforeFirstRow);
                    return false;
                }
                return true;
            }
            if (!m_result->fetch(actualIdx)) {
                m_result->setRow((int)LSqlLocation::AfterLastRow);
                return false;
            }
            return true;
        }
    }
    return false;
}

const LSqlError &LSqlQuery::lastError() const
{
    if(m_result != nullptr){
        return m_result->lastError();
    }else{
        return LSqlError::staticNoError();
    }
}

std::shared_ptr<LSqlDriver> LSqlQuery::driver() const
{
    if(m_result == nullptr){
        return nullptr;
    }
    return m_result->driver();
}

std::shared_ptr<LSqlResult> LSqlQuery::result() const
{
    return m_result;
}


LK_NAMESPACE_END
