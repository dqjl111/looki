#include "lsqlresult.h"
#include "lkcore/text/utf8iterator.h"
#include <format>

LK_NAMESPACE_BEGIN

LSqlResult::LSqlResult(std::shared_ptr<LSqlDriver> driver): m_driver(driver){

}

bool LSqlResult::fetchNext()
{
    return fetch(m_row + 1);
}

bool LSqlResult::fetchPrevious()
{
    return fetch(m_row - 1);
}

bool LSqlResult::exec()
{
    bool ret;
    // fake preparation - just replace the placeholders..
    std::string query = lastQuery();
    if (m_binds == NamedBinding) {
        std::list<Holder>::const_reverse_iterator iter = m_holders.crbegin();
        for(; iter !=m_holders.crend(); ++iter ){
            const std::string &holder = iter->name;
            std::shared_ptr<LSqlField> f = std::make_shared<LSqlField>();
            f->setValue(boundValue(holder));
            query = query.replace(iter->value, holder.length(), m_driver->formatValue(f));
        }
    } else {
        lsizetype i = 0;
        for (const LVariant &var : std::as_const(m_boundValues)) {
            i = query.find('?', i);
            if (i == std::string::npos){
                continue;
            }

            std::shared_ptr<LSqlField> f = std::make_shared<LSqlField>();
            f->setValue(var);

            const std::string val = m_driver->formatValue(f);
            query = query.replace(i, 1, val);
            i += val.length();
        }
    }

    // have to retain the original query with placeholders
    std::string orig = lastQuery();
    ret = reset(query);
    m_executedQuery = query;
    setQuery(orig);
    resetBindCount();
    return ret;
}

void LSqlResult::setRow(int row)
{
    m_row = row;
}

void LSqlResult::setActive(bool a)
{
    if (m_active){
        m_executedQuery = m_sql;
    }
    m_active = a;
}

bool LSqlResult::prepare(const std::string &query)
{
    m_sql = LStringUtil::trim(query);
    if (m_holders.empty()) {
        // parse the query to memorize parameter location
        namedToPositionalBinding(m_sql);
    }
    return true; // fake prepares should always succeed
}

bool LSqlResult::savePrepare(const std::string &query)
{
    if (m_driver == nullptr){
        return false;
    }
    clear();
    m_sql = LStringUtil::trim(query);
    if (!m_driver->hasFeature(LSqlDriver::PreparedQueries)){
        return prepare(query);
    }

    // parse the query to memorize parameter location
    m_executedQuery = namedToPositionalBinding(m_sql);
    if (m_driver->hasFeature(LSqlDriver::NamedPlaceholders)){
        m_executedQuery = positionalToNamedBinding(m_sql);
    }
    return prepare(m_executedQuery);
}

void LSqlResult::detachFromResultSet()
{

}

void LSqlResult::setPrecisionPolicy(LSqlPrecisionPolicy policy)
{
    m_precisionPolicy = policy;
}

void LSqlResult::setQuery(const std::string &query)
{
    m_sql = LStringUtil::trim(query);
}

LVariant LSqlResult::lastInsertId() const
{
    return LVariant();
}

void LSqlResult::setForwardOnly(bool forward)
{
    m_forwardOnly = forward;
}

void LSqlResult::bindValue(const std::string &placeholder, const LVariant &val, LFlags<LSqlIoFlag> type)
{
    m_binds = NamedBinding;
    // if the index has already been set when doing emulated named
    // bindings - don't reset it
    IndexMap::iterator iter = m_indexes.find(placeholder);
    if(iter != m_indexes.end()){
        std::list<int> &indexes = iter->second;
        for (int idx : indexes) {
            if (m_boundValues.size() <= idx){
                m_boundValues.resize(idx + 1);
            }

            auto it = m_boundValues.begin();
            std::advance(it, idx);
            *it = val;
            if (type != LSqlIoFlag::In || !m_types.empty()){
                m_types[idx] = type;
            }
        }
    }
}

void LSqlResult::bindValue(int index, const LVariant &val, LFlags<LSqlIoFlag> type)
{
    m_binds = PositionalBinding;
    std::string key = std::format(":{0}", index);
    std::list<int> &indexes = m_indexes[key];
    std::list<int>::const_iterator iit = std::find(indexes.cbegin(), indexes.cend(), index);
    if (iit == indexes.cend()) {
        indexes.push_back(index);
    }

    if (m_boundValues.size() <= index){
        m_boundValues.resize(index + 1);
    }
    auto it = m_boundValues.begin();
    std::advance(it, index);
    *it = val;
    if (type != LSqlIoFlag::In || !m_types.empty()){
        m_types[index] = type;
    }
}

void LSqlResult::addBindValue(const LVariant &val, LFlags<LSqlIoFlag> type)
{
    bindValue(m_bindCount, val, type);
    ++m_bindCount;
}

const LVariant& LSqlResult::boundValue(const std::string &placeholder) const
{
    IndexMap::const_iterator iter = m_indexes.find(placeholder);
    if(iter != m_indexes.end()){
        const std::list<int> &indexes = iter->second;
        if(!indexes.empty()){
            auto it = indexes.cbegin();
            int index = *it;
            if(index>=0 && index < m_boundValues.size()){
                auto vt = m_boundValues.cbegin();
                std::advance(vt, index);
                return *vt;
            }
        }
    }
    return LVariant::staticNull();
}


const LVariant& LSqlResult::boundValue(int index) const
{
    if(index>=0 && index < m_boundValues.size()){
        auto it = m_boundValues.cbegin();
        std::advance(it, index);
        return *it;
    }
    return LVariant::staticNull();
}

bool LSqlResult::isValid() const
{
    return m_row >=0;
}

std::string LSqlResult::namedToPositionalBinding(const std::string &query)
{
    if(m_driver == nullptr){
        return "";
    }
    // In the Interbase case if it is an EXECUTE BLOCK then it is up to the
    // caller to make sure that it is not using named bindings for the wrong
    // parts of the query since Interbase uses them literally
    const static std::regex exeBlock( "EXECUTE BLOCK.*", std::regex_constants::icase ) ;
    if (m_driver->dbmsType() == LSqlDriver::Interbase && std::regex_match( query, exeBlock)){
        return query;
    }

    std::ostringstream result;
    char closingQuote = char(0);
    uint32_t count = 0;
    bool ignoreBraces = (m_driver->dbmsType() == LSqlDriver::PostgreSQL);
    const bool markNotationSupported = (m_driver->dbmsType() != LSqlDriver::PostgreSQL);

    LUtf8Iterator iter(query);
    LUtf8Iterator nextIter;
    uint32_t prevCode = 0;

    bool hasNext = iter.next();
    while (hasNext) {
        uint32_t ch = iter.unicode();
        nextIter = iter;
        hasNext = nextIter.next();

        if (closingQuote != char(0)) {
            if (ch == closingQuote) {
                if (closingQuote == ']'
                    && hasNext && nextIter.unicode() == closingQuote) {
                    // consume the extra character. don't close.
                    iter = nextIter;
                    result << iter.utf8Code();
                } else {
                    closingQuote = char(0);
                }
            }
            result << iter.utf8Code();
            prevCode = ch;
            hasNext = iter.next();
        } else {
            if (ch == ':'
                && (iter.atBegin() || prevCode != ':')
                && (hasNext && LStringUtil::isAlnum(nextIter.unicode()))) {
                hasNext = nextIter.next();
                while (hasNext && LStringUtil::isAlnum(nextIter.unicode())){
                    prevCode = nextIter.unicode();
                    hasNext = nextIter.next();
                }
                // if question mark notation is not supported we have to use
                // the native binding.  fieldSerial() should be renamed
                // to toNativeBinding() and used unconditionally here
                if (markNotationSupported){
                    result << '?';
                }else{
                    result <<  std::format(":{0}", count);
                }
                std::string holder(query.substr(iter.codePosition(), nextIter.codePosition()-iter.codePosition()));
                m_indexes[holder].push_back(count++);
                m_holders.push_back(Holder(holder, iter.codePosition()));
                iter = nextIter;
            } else {
                if (ch == '\'' || ch == '"' || ch == '`'){
                    closingQuote = ch;
                }
                else if (!ignoreBraces && ch == '['){
                    closingQuote = ']';
                }
                result << iter.utf8Code();
                prevCode = ch;
                hasNext = iter.next();
            }
        }
    }
    m_boundValues.resize(m_holders.size());
    return result.str();
}

std::string LSqlResult::positionalToNamedBinding(const std::string &query) const
{
    if(m_driver == nullptr){
        return "";
    }

    if (!m_positionalBindingEnabled){
        return query;
    }

    std::ostringstream result;
    char closingQuote = char(0);
    uint32_t count = 0;
    bool ignoreBraces = (m_driver->dbmsType() == LSqlDriver::PostgreSQL);

    LUtf8Iterator iter(query);
    bool hasNext = iter.next();
    LUtf8Iterator nextIter;

    while (hasNext) {
        uint32_t ch = iter.unicode();
        nextIter = iter;
        hasNext = nextIter.next();

        if (closingQuote != char(0)) {
            if (ch == closingQuote) {
                if (closingQuote == ']'
                    && hasNext && nextIter.unicode() == closingQuote) {
                    // consume the extra character. don't close.
                    iter = nextIter;
                    result << iter.utf8Code();
                } else {
                    closingQuote = char(0);
                }
            }
            result << iter.utf8Code();
        } else {
            if (ch == '?') {
                result << std::format(":{0}", count++);
            } else {
                if (ch == '\'' || ch == '"' || ch == '`'){
                    closingQuote = ch;
                }else if (!ignoreBraces && ch == '['){
                    closingQuote = ']';
                }
                result << iter.utf8Code();
            }
        }
        hasNext = iter.next();
    }
    return result.str();
}

void LSqlResult::clearValues()
{
    m_boundValues.clear();
    m_bindCount = 0;
}

void LSqlResult::resetBindCount()
{
    m_bindCount = 0;
}

std::list<std::string> LSqlResult::boundValueNames() const
{
    std::list<std::string> ret;
    for (const Holder &holder : std::as_const(m_holders)){
        ret.push_back(holder.name);
    }
    return ret;
}

std::string LSqlResult::boundValueName(int index) const
{
    if(m_holders.size() > index){
        auto it = m_holders.cbegin();
        std::advance(it, index);
        return it->name;
    }else{
        return std::format(":{0}", index);
    }
}

void LSqlResult::clearIndex()
{
    m_indexes.clear();
    m_holders.clear();
    m_types.clear();
}

void LSqlResult::clear()
{
    clearValues();
    clearIndex();
}

bool LSqlResult::hasOutValues() const
{
    if (m_types.empty()){
        return false;
    }
    std::unordered_map<int, LFlags<LSqlIoFlag> >::const_iterator it;
    for (it = m_types.cbegin(); it != m_types.cend(); ++it) {
        if (it->second != LSqlIoFlag::In){
            return true;
        }
    }
    return false;
}



LK_NAMESPACE_END
