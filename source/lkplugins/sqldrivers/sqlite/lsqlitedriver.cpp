#include "lsqlitedriver.h"
#include "lkcore/lstringutil.h"
#include "lkcore/ldebug.h"
#include "lkcore/io/li18n.h"
#include "lksql/lsqlcachedresult.h"
#include "lksql/lsqlquery.h"
#include <algorithm>
#include <filesystem>
#include <ranges>
#include <string_view>
#include <sqlite3.h>
#include <numeric>
#include <set>
#include <format>

LK_NAMESPACE_BEGIN

static void _l_lower(sqlite3_context* context, int argc, sqlite3_value** argv)
{
    if (L_UNLIKELY(argc != 1)) {
        sqlite3_result_text(context, nullptr, 0, nullptr);
        return;
    }
    const std::string lower = LStringUtil::toLower(reinterpret_cast<const char*>(sqlite3_value_text(argv[0])));
    sqlite3_result_text(context, lower.data(), (int)lower.size(), SQLITE_TRANSIENT);
}

static void _l_upper(sqlite3_context* context, int argc, sqlite3_value** argv)
{
    if (L_UNLIKELY(argc != 1)) {
        sqlite3_result_text(context, nullptr, 0, nullptr);
        return;
    }
    const std::string upper = LStringUtil::toUpper(reinterpret_cast<const char*>(sqlite3_value_text(argv[0])));
    sqlite3_result_text(context, upper.data(), (int)upper.size(), SQLITE_TRANSIENT);
}


class LSQLiteResult : public LSqlCachedResult
{
    friend class LSqliteDriver;
public:
    explicit LSQLiteResult(const std::shared_ptr<LSqliteDriver> db);
    ~LSQLiteResult();

    virtual bool gotoNext(std::list<LVariant> &values, int idx) override;
    virtual bool reset(const std::string &query) override;
    virtual bool prepare(const std::string &query) override;
    virtual bool exec() override;
    virtual int size() override;
    virtual int numRowsAffected() override;
    virtual LVariant lastInsertId() const override;
    virtual std::shared_ptr<LSqlRecord> record() const override;
    virtual void detachFromResultSet() override;

    virtual void init() override;

    void cleanup();
    void finalize();
    bool fetchNext(std::list<LVariant> &values, int idx, bool initialFetch);
    void initColumns();
    sqlite3 *access();

    sqlite3_stmt *m_stmt = nullptr;
    std::shared_ptr<LSqlRecord> m_rInf;
    std::list<LVariant> m_firstRow;
    bool m_skippedStatus = false; // the status of the fetchNext() that's skipped
    bool m_skipRow = false; // skip the next fetchNext()?
};

LSQLiteResult::LSQLiteResult(const std::shared_ptr<LSqliteDriver> db ) : LSqlCachedResult(db)
{
}

void LSQLiteResult::init(){
    std::shared_ptr<LSqliteDriver> dDriver = std::dynamic_pointer_cast<LSqliteDriver>(m_driver);
    if(dDriver != nullptr){
        std::shared_ptr<LSQLiteResult> r= getPtrT<LSQLiteResult>();
        dDriver->addResult(r);
    }
}

sqlite3 *LSQLiteResult::access(){
    std::shared_ptr<LSqliteDriver> dDriver = std::dynamic_pointer_cast<LSqliteDriver>(m_driver);
    if(dDriver != nullptr){
        return dDriver->access();
    }
    return nullptr;
}

LSQLiteResult::~LSQLiteResult()
{
    std::shared_ptr<LSqliteDriver> dDriver = std::dynamic_pointer_cast<LSqliteDriver>(m_driver);
    if(dDriver != nullptr){
        dDriver->removeResult(nullptr);
    }
    cleanup();
}

void LSQLiteResult::cleanup()
{
    finalize();
    m_rInf = nullptr;
    m_skippedStatus = false;
    m_skipRow = false;
    setRow((int)LSqlLocation::BeforeFirstRow);
    setActive(false);
    LSqlCachedResult::cleanup();
}

void LSQLiteResult::finalize()
{
    if (m_stmt == nullptr){
        return;
    }

    sqlite3_finalize(m_stmt);
    m_stmt = nullptr;
}

bool LSQLiteResult::fetchNext(std::list<LVariant> &values, int idx, bool initialFetch)
{
    if (m_skipRow) {
        m_skipRow = false;
        std::list<LVariant>::const_iterator cIter = m_firstRow.cbegin();
        std::list<LVariant>::iterator vIter = values.begin();
        for(; cIter != m_firstRow.cend() && vIter !=values.end(); ++cIter, ++vIter ){
            *vIter = *cIter;
        }
        return m_skippedStatus;
    }
    m_skipRow = initialFetch;

    if (initialFetch) {
        m_firstRow.clear();
        m_firstRow.resize(sqlite3_column_count(m_stmt));
    }

    if (!m_stmt) {
        m_error.buildError(LSqlError::Connection,
                           LI18n::trx(L"LSqliteResult 无法取得数据行"),
                           LI18n::trx(L"LSqliteResult 无查询"));
        setRow((int)LSqlLocation::AfterLastRow);
        return false;
    }
    int res = sqlite3_step(m_stmt);
    switch(res) {
    case SQLITE_ROW:{
        // check to see if should fill out columns
        if (m_rInf == nullptr || m_rInf->isEmpty()){
            // must be first call.
            initColumns();
        }
        if (idx < 0 && !initialFetch){
            return true;
        }


        std::list<LVariant>::iterator vIter = values.begin();
        std::advance(vIter, idx);
        for (int i = 0; i < m_rInf->count(); ++i) {
            if(vIter == values.end()){
                break;
            }

            switch (sqlite3_column_type(m_stmt, i)) {
            case SQLITE_BLOB:{
                vIter->setByteArray((const char*)sqlite3_column_blob(m_stmt, i), sqlite3_column_bytes(m_stmt, i));
                break;
            }
            case SQLITE_INTEGER:{
                vIter->setInt64(sqlite3_column_int64(m_stmt, i));
                break;
            }
            case SQLITE_FLOAT:{
                switch(precisionPolicy()) {
                case LSqlPrecisionPolicy::LowInt32:{
                    vIter->setInt32(sqlite3_column_int(m_stmt, i));
                    break;
                }
                case LSqlPrecisionPolicy::LowInt64:{
                    vIter->setInt64(sqlite3_column_int64(m_stmt, i));
                    break;
                }
                case LSqlPrecisionPolicy::LowDouble:
                case LSqlPrecisionPolicy::High:
                default:{
                    vIter->setDouble(sqlite3_column_double(m_stmt, i));
                    break;
                }
                };
                break;
            }
            case SQLITE_NULL:{
                vIter->clear();
                break;
            }
            default:{
                vIter->setString((const char *)sqlite3_column_text(m_stmt, i), sqlite3_column_bytes(m_stmt, i));
                break;
            }
            }

            ++i;
            ++vIter;
        }
        return true;
    }
    case SQLITE_DONE:{
        if (m_rInf == nullptr || m_rInf->isEmpty()){
            // must be first call.
            initColumns();
        }
        setRow((int)LSqlLocation::AfterLastRow);
        sqlite3_reset(m_stmt);
        return false;
    }
    case SQLITE_CONSTRAINT:
    case SQLITE_ERROR:{
        // SQLITE_ERROR is a generic error code and we must call sqlite3_reset()
        // to get the specific error message.
        res = sqlite3_reset(m_stmt);
        m_error.buildError(LSqlError::Connection,
                           LI18n::trx(L"LSqliteResult 无法取得数据行"),
                           std::string(sqlite3_errmsg(access())),
                           std::to_string(res));
        setRow((int)LSqlLocation::AfterLastRow);
        return false;
    }
    case SQLITE_MISUSE:
    case SQLITE_BUSY:
    default:{
        m_error.buildError(LSqlError::Connection,
                           LI18n::trx(L"LSqliteResult 无法取得数据行"),
                           std::string(sqlite3_errmsg(access())),
                           std::to_string(res));
        sqlite3_reset(m_stmt);
        setRow((int)LSqlLocation::AfterLastRow);
        return false;
    }
    }
    return false;
}

void LSQLiteResult::initColumns()
{
    int nCols = sqlite3_column_count(m_stmt);
    if (nCols <= 0){
        return;
    }

    initCols(nCols);

    if (m_rInf == nullptr){
        m_rInf = std::make_shared<LSqlRecord>();
    }

    for (int i = 0; i < nCols; ++i) {
        std::string colName = sqlite3_column_name(m_stmt, i);
        colName.erase(std::remove(colName.begin(), colName.end(), '"'), colName.end());
        std::string tableName = sqlite3_column_table_name(m_stmt, i);
        tableName.erase(std::remove(tableName.begin(), tableName.end(), '"'), tableName.end());

        std::shared_ptr<LSqlField> fld = std::make_shared<LSqlField>(colName, tableName);
        m_rInf->append(fld);
    }
}


bool LSQLiteResult::gotoNext(std::list<LVariant> &values, int idx)
{
    return fetchNext(values, idx, false);
}

bool LSQLiteResult::reset(const std::string &query)
{
    if (!prepare(query)){
        return false;
    }
    return exec();
}

bool LSQLiteResult::prepare(const std::string &query)
{
    if(m_driver == nullptr  || !m_driver->isOpen() || m_driver->isOpenError()){
        return false;
    }

    cleanup();

    m_isSel = false;
    const char *pzTail = nullptr;
#if (SQLITE_VERSION_NUMBER >= 3003011)
    int res = sqlite3_prepare_v2(access(), query.data(), (int)query.length(), &m_stmt, &pzTail);
#else
    int res = sqlite3_prepare(access()), query.data(), (int)query.length(), &m_stmt, &pzTail);
#endif

    if (res != SQLITE_OK) {
        m_error.buildError(LSqlError::Statement,
                           LI18n::trx(L"LSqliteResult 无法执行查询"),
                           std::string(sqlite3_errmsg(access())),
                           std::to_string(res));
        finalize();
        return false;
    } else if (pzTail != nullptr && strlen(pzTail)!=0 ) {
        m_error.buildError(LSqlError::Statement,
                           LI18n::trx(L"LSqliteResult 无法同时执行多重查询"),
                           std::string(sqlite3_errmsg(access())),
                           std::to_string(SQLITE_MISUSE));
        finalize();
        return false;
    }
    return true;
}

bool LSQLiteResult::exec()
{
    m_skippedStatus = false;
    m_skipRow = false;
    if (m_rInf == nullptr){
        m_rInf = std::make_shared<LSqlRecord>();
    }else{
        m_rInf->clear();
    }
    clearValues();
    m_error.clear();

    int res = sqlite3_reset(m_stmt);
    if (res != SQLITE_OK) {
        m_error.buildError(LSqlError::Statement,
                           LI18n::trx(L"LSqliteResult 无法重置查询"),
                           std::string(sqlite3_errmsg(access())),
                           std::to_string(res));
        finalize();
        return false;
    }

    int paramCount = sqlite3_bind_parameter_count(m_stmt);


#if (SQLITE_VERSION_NUMBER >= 3003011)
    std::list<LVariant> prunedValues;
    std::list<LVariant>* values = nullptr;
    bool paramCountIsValid = paramCount == m_boundValues.size();

    // In the case of the reuse of a named placeholder
    // We need to check explicitly that paramCount is greater than or equal to 1, as sqlite
    // can end up in a case where for virtual tables it returns 0 even though it
    // has parameters
    if (paramCount >= 1 && paramCount < m_boundValues.size()) {
        const auto countIndexes = [](int counter, const IndexMap::value_type &v) {
            return counter + v.second.size();
        };

        const int bindParamCount = std::accumulate(m_indexes.cbegin(),
                                                   m_indexes.cend(),
                                                   0,
                                                   countIndexes);

        paramCountIsValid = bindParamCount == m_boundValues.size();
        // When using named placeholders, it will reuse the index for duplicated
        // placeholders. So we need to ensure the list has only one instance of
        // each value as SQLite will do the rest for us.

        std::set<int> handledIndexes;
        for (int i = 0, currentIndex = 0; i < m_boundValues.size(); ++i) {
            if (handledIndexes.contains(i)){
                continue;
            }

            const char *parameterName = sqlite3_bind_parameter_name(m_stmt, currentIndex + 1);
            if (!parameterName) {
                paramCountIsValid = false;
                continue;
            }
            const std::string placeHolder = std::string(parameterName);

            IndexMap::const_iterator iter = m_indexes.find(placeHolder);
            if(iter != m_indexes.cend()){
                const std::list<int> &indexes = iter->second;

                if(!indexes.empty()){
                    handledIndexes.insert(indexes.cbegin(), indexes.cend());
                    auto it = m_boundValues.cbegin();
                    std::advance(it, *indexes.cbegin());
                    prunedValues.push_back(*it);
                }

            }
            ++currentIndex;
        }
        values = &prunedValues;
    }else{
        values = &m_boundValues;
    }
#else
    std::list<LVariant>& values = m_boundValues;
    bool paramCountIsValid = paramCount == values.size();
#endif

    if (paramCountIsValid) {
        int i=0;
        for (const LVariant &value : *values) {
            if (value.isNull()) {
                res = sqlite3_bind_null(m_stmt, i + 1);
            } else {
                switch (value.type()) {
                case LVariant::VarByteArray:
                case LVariant::VarCbor: {
                    res = sqlite3_bind_blob(m_stmt, i + 1, value.getBinData(), value.getByteSize(), SQLITE_STATIC);
                    break;
                }
                case LVariant::VarChar:
                case LVariant::VarInt8:
                case LVariant::VarUInt8:
                case LVariant::VarInt16:
                case LVariant::VarUInt16:
                case LVariant::VarBool:{
                    res = sqlite3_bind_int(m_stmt, i + 1, value.toInt());
                    break;
                }
                case LVariant::VarFloat:
                case LVariant::VarDouble:{
                    res = sqlite3_bind_double(m_stmt, i + 1, value.toDouble());
                    break;
                }
                case LVariant::VarUInt32:
                case LVariant::VarInt64:
                case LVariant::VarUInt64:
                case LVariant::VarTimePoint:{
                    res = sqlite3_bind_int64(m_stmt, i + 1, value.toUInt64());
                    break;
                }
                case LVariant::VarString: {
                    res = sqlite3_bind_text(m_stmt, i + 1, value.getString(),value.getByteSize(), SQLITE_STATIC);
                    break;
                }
                case LVariant::VarWString: {
                    res = sqlite3_bind_text16(m_stmt,
                                              i + 1,
                                              value.getWString(),
                                              value.getByteSize(),
                                              SQLITE_STATIC);
                    break;
                }
                default: {
                    const std::string str = value.toString();
                    res = sqlite3_bind_text(m_stmt, i + 1, str.data(), (int)str.size(), SQLITE_TRANSIENT);
                    break;
                }
                }
            }
            if (res != SQLITE_OK) {
                m_error.buildError(LSqlError::Statement,
                                   LI18n::trx(L"LSqliteResult 无法绑定参数"),
                                   std::string(sqlite3_errmsg(access())),
                                   std::to_string(res));
                finalize();
                return false;
            }

            ++i;
        }
    } else {
        m_error.buildError(LSqlError::Statement, LI18n::trx(L"LSqliteResult 参数数目不匹配"));
        return false;
    }
    m_skippedStatus = fetchNext(m_firstRow, 0, true);
    if (m_error.isValid()) {
        m_isSel = false;
        setActive(false);
        return false;
    }
    m_isSel = (!m_rInf->isEmpty());
    setActive(true);
    return true;
}

int LSQLiteResult::size()
{
    return -1;
}

int LSQLiteResult::numRowsAffected()
{
    std::shared_ptr<LSqliteDriver> dDriver = std::dynamic_pointer_cast<LSqliteDriver>(m_driver);
    if(dDriver != nullptr){
        return sqlite3_changes(dDriver->access());
    }
    return 0;
}


LVariant LSQLiteResult::lastInsertId() const
{
    if (isActive()) {
        std::shared_ptr<LSqliteDriver> dDriver = std::dynamic_pointer_cast<LSqliteDriver>(m_driver);
        if(dDriver != nullptr){
            int64_t id = sqlite3_last_insert_rowid(dDriver->access());
            if (id){
                return id;
            }
        }
    }
    return LVariant();
}

std::shared_ptr<LSqlRecord> LSQLiteResult::record() const
{
    if (!isActive() || !isSelect()){
        return nullptr;
    }
    return m_rInf;
}

void LSQLiteResult::detachFromResultSet()
{
    if (m_stmt != nullptr){
        sqlite3_reset(m_stmt);
    }
}


LSqliteDriver::LSqliteDriver()
{

}

LSqliteDriver::~LSqliteDriver()
{
    LSqliteDriver::close();
}

bool LSqliteDriver::open(const std::string &db,
                         const std::string &user,
                         const std::string &password,
                         const std::string &host,
                         int port,
                         const std::string &connOpts)
{
    if (isOpen()){
        close();
    }


    int timeOut = 5000;
    bool sharedCache = false;
    bool openReadOnlyOption = false;
    bool openUriOption = false;
    bool useExtendedResultCodes = true;
    bool useLkVfs = false;
    bool useQtCaseFolding = false;
    bool openNoFollow = false;

    std::string_view optViews(connOpts);

    for (auto subrange : optViews | std::ranges::views::split(';')) {
        std::string_view option = LStringUtil::trim(std::string_view(subrange));
        if (option.starts_with("SQLITE_BUSY_TIMEOUT")) {
            option = LStringUtil::trim(option.substr(19));
            if (option.starts_with('=')) {
                bool ok = false;
                const int nt = LStringUtil::sto<int>(option, &ok);
                if (ok){
                    timeOut = nt;
                }
            }
        } else if (option == "SQLITE_USE_LK_VFS") {
            useLkVfs = true;
        } else if (option == "SQLITE_OPEN_READONLY") {
            openReadOnlyOption = true;
        } else if (option == "SQLITE_OPEN_URI") {
            openUriOption = true;
        } else if (option == "SQLITE_ENABLE_SHARED_CACHE") {
            sharedCache = true;
        } else if (option == "SQLITE_NO_USE_EXTENDED_RESULT_CODES") {
            useExtendedResultCodes = false;
        } else if (option == "SQLITE_ENABLE_NON_ASCII_CASE_FOLDING") {
            useQtCaseFolding = true;
        } else if (option == "SQLITE_OPEN_NOFOLLOW") {
            openNoFollow = true;
        }
        else{
            lWarn() << "Unsupport sqlite option:" << option;
        }
    }

    int openMode = (openReadOnlyOption ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE));
    openMode |= (sharedCache ? SQLITE_OPEN_SHAREDCACHE : SQLITE_OPEN_PRIVATECACHE);
    if (openUriOption){
        openMode |= SQLITE_OPEN_URI;
    }
    if (openNoFollow) {
        openMode |= SQLITE_OPEN_NOFOLLOW;
    }

    openMode |= SQLITE_OPEN_NOMUTEX;

#ifdef _WIN32
    // sqlite3 的路径参数要求 UTF-8，而调用方传入的 std::string 通常是本地编码（如 GBK）。
    // 仅当路径含非 ASCII 字符时做一次转换：纯 ASCII 下本地编码与 UTF-8 一致，
    // 也避免影响 ":memory:" 之类的特殊名称。
    std::string dbNameUtf8 = db;
    if (std::any_of(db.begin(), db.end(),
                    [](char c) { return static_cast<unsigned char>(c) > 0x7F; })) {
        const std::u8string u8 = std::filesystem::path(db).u8string();
        dbNameUtf8.assign(reinterpret_cast<const char *>(u8.data()), u8.size());
    }
    const int res = sqlite3_open_v2(dbNameUtf8.c_str(), &m_access, openMode, useLkVfs ? "LkVFS" : nullptr);
#else
    const int res = sqlite3_open_v2(db.data(), &m_access, openMode, useLkVfs ? "LkVFS" : nullptr);
#endif

    if (res == SQLITE_OK) {
        sqlite3_busy_timeout(m_access, timeOut);
        sqlite3_extended_result_codes(m_access, useExtendedResultCodes);
        m_isOpen = true;
        m_isOpenError = false;

        if (useQtCaseFolding) {
            sqlite3_create_function_v2(m_access, "lower", 1, SQLITE_UTF8, nullptr,
                                       &_l_lower, nullptr, nullptr, nullptr);
            sqlite3_create_function_v2(m_access, "upper", 1, SQLITE_UTF8, nullptr,
                                       &_l_upper, nullptr, nullptr, nullptr);
        }
        return true;
    } else {
        makeError(LI18n::trx(L"数据库打开失败"), LSqlError::Connection, res);
        m_isOpen = false;
        m_isOpenError = true;

        if (m_access) {
            sqlite3_close(m_access);
            m_access = nullptr;
        }

        return false;
    }
}

void LSqliteDriver::close()
{
    if (m_isOpen) {
        for (const std::weak_ptr<LSqlResult>& wr : m_results){
            std::shared_ptr<LSQLiteResult> r = std::dynamic_pointer_cast<LSQLiteResult>(wr.lock());
            if(r != nullptr){
                r->finalize();
            }
        }

        m_notificationId.clear();

        if(m_access != nullptr){
            sqlite3_update_hook(m_access, nullptr, nullptr);
            const int res = sqlite3_close(m_access);

            if (res != SQLITE_OK){
                makeError(LI18n::trx(L"关闭数据库异常"), LSqlError::Connection, res);
            }
        }
        m_access = nullptr;
        m_isOpen = false;
        m_isOpenError = false;
    }
}

bool LSqliteDriver::hasFeature(DriverFeature f) const
{
    switch (f) {
    case BLOB:
    case Transactions:
    case Unicode:
    case LastInsertId:
    case PreparedQueries:
    case PositionalPlaceholders:
    case SimpleLocking:
    case FinishQuery:
    case LowPrecisionNumbers:
    case EventNotifications:{
        return true;
    }
    case QuerySize:
    case BatchOperations:
    case MultipleResultSets:
    case CancelQuery:{
        return false;
    }
    case NamedPlaceholders:{
#if (SQLITE_VERSION_NUMBER < 3003011)
        return false;
#else
        return true;
#endif
    }
    }
    return false;
}

std::shared_ptr<LSqlResult> LSqliteDriver::createResult() const
{
    std::shared_ptr<LSqliteDriver> d= getPtrT<LSqliteDriver>();
    return std::make_shared<LSQLiteResult>(d);
}

void LSqliteDriver::addResult(std::shared_ptr<LSqlResult> r)
{
    if(r != nullptr){
        m_results.push_back(r);
    }
}

void LSqliteDriver::removeResult(std::shared_ptr<LSqlResult> r)
{
    auto isResult = [r](std::weak_ptr<LSqlResult> rr) { return rr.lock() == r; };
    m_results.remove_if(isResult);
}

bool LSqliteDriver::beginTransaction()
{
    if (!isOpen() || isOpenError()){
        return false;
    }

    LSqlQuery q(createResult());
    if (!q.exec("BEGIN")) {
        m_error.buildError(LSqlError::Transaction,
                           LI18n::trx(L"无法开启事务"),
                           q.lastError().m_databaseError,
                           q.lastError().m_errorCode);
        return false;
    }
    return true;
}

bool LSqliteDriver::commitTransaction()
{
    if (!isOpen() || isOpenError()){
        return false;
    }

    LSqlQuery q(createResult());
    if (!q.exec("COMMIT")) {
        m_error.buildError(LSqlError::Transaction,
                           LI18n::trx(L"无法提交事务"),
                           q.lastError().m_databaseError,
                           q.lastError().m_errorCode);
        return false;
    }

    return true;
}

bool LSqliteDriver::rollbackTransaction()
{
    if (!isOpen() || isOpenError()){
        return false;
    }

    LSqlQuery q(createResult());
    if (!q.exec("ROLLBACK")) {
        m_error.buildError(LSqlError::Transaction,
                           LI18n::trx(L"无法回滚事务"),
                           q.lastError().m_databaseError,
                           q.lastError().m_errorCode);
        return false;
    }
    return true;
}

std::list<std::string> LSqliteDriver::tables(LFlags<LSqlTableType> type) const
{
    std::list<std::string> res;
    if (!isOpen()){
        return res;
    }

    LSqlQuery q(createResult());
    q.setForwardOnly(true);

    std::string sql;
    if ((type & LSqlTableType::Tables) && (type & LSqlTableType::Views)){
        sql = "type='table' OR type='view'";
    }else if (type & LSqlTableType::Tables){
        sql = "type='table'";
    }else if (type & LSqlTableType::Views){
        sql = "type='view'";
    }

    if(!sql.empty()){
        sql = std::format("SELECT name FROM sqlite_master WHERE {0} UNION ALL SELECT name FROM sqlite_temp_master WHERE {0}", sql);
        if(q.exec(sql)){
            while(q.next()){
                res.push_back(q.value(0).toString());
            }
        }
    }

    if (type & LSqlTableType::System) {
        // there are no internal tables beside this one:
        res.push_back("sqlite_master");
    }
    return res;
}

void LSqliteDriver::makeError(const std::string &descr, LSqlError::Type type, int errorCode)
{
    m_error.buildError(type,
                       descr,
                       std::string(sqlite3_errmsg(m_access)),
                       std::to_string(errorCode));
}



LK_NAMESPACE_END
