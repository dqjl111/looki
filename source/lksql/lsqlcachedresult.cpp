#include "lsqlcachedresult.h"
#include <iterator>

LK_NAMESPACE_BEGIN

static constexpr lsizetype initialCacheSize = 128;

LSqlCachedResult::LSqlCachedResult(std::shared_ptr<LSqlDriver> driver):LSqlResult(driver) {}

void LSqlCachedResult::initCols(int colCount)
{
    initCols(colCount, m_forwardOnly);
}

void LSqlCachedResult::initCols(int count, bool isForward)
{
    cleanup();
    m_forwardOnly = isForward;
    m_colCount = count;
    if (isForward) {
        m_cache.resize(count);
        m_rowCacheEnd = count;
    } else {
        m_cache.resize(initialCacheSize * count);
    }
}

int LSqlCachedResult::nextIndex()
{
    if (m_forwardOnly){
        return 0;
    }
    int newIdx = m_rowCacheEnd;
    if (newIdx + m_colCount > m_cache.size()){
        m_cache.resize(std::min<uint32_t>(m_cache.size() * 2, m_cache.size() + 10000));
    }
    m_rowCacheEnd += m_colCount;
    return newIdx;
}

void LSqlCachedResult::revertLast()
{
    if (m_forwardOnly){
        return;
    }
    m_rowCacheEnd -= m_colCount;
}

void LSqlCachedResult::cleanup()
{
    setRow((int)LSqlLocation::BeforeFirstRow);
    setActive(false);
    m_cache.clear();
    m_atEnd = false;
    m_colCount = 0;
    m_rowCacheEnd = 0;
}

void LSqlCachedResult::clearValues()
{
    setRow((int)LSqlLocation::BeforeFirstRow);
    m_rowCacheEnd = 0;
    m_atEnd = false;
}

const LVariant& LSqlCachedResult::data(int col) const
{
    if (col >= m_colCount || col < 0 || m_row < 0){
        return LVariant::staticNull();
    }

    int idx = m_forwardOnly ? col : m_row * m_colCount + col;
    if (idx >= m_rowCacheEnd || idx >= m_cache.size()){
        return LVariant::staticNull();
    }

    auto it = m_cache.cbegin();
    std::advance(it, idx);
    return *it;
}

bool LSqlCachedResult::isNull(int col) const
{
    if (col >= m_colCount || col < 0 || m_row < 0){
        return true;
    }

    int idx = m_forwardOnly ? col : m_row * m_colCount + col;
    if (idx >= m_rowCacheEnd || idx >= m_cache.size()){
        return true;
    }
    auto it = m_cache.cbegin();
    std::advance(it, idx);
    return (*it).isNull();
}

bool LSqlCachedResult::fetch(int row)
{
    if ((!isActive()) || (row < 0)){
        return false;
    }
    if (m_row == row){
        return true;
    }
    if (m_forwardOnly) {
        // speed hack - do not copy values if not needed
        if (m_row > row || m_row == (int)LSqlLocation::AfterLastRow){
            return false;
        }
        while(m_row < row - 1) {
            if (!gotoNext(m_cache, -1)){
                return false;
            }
            setRow(m_row + 1);
        }
        if (!gotoNext(m_cache, 0)){
            return false;
        }
        setRow(m_row + 1);
        return true;
    }
    if (canSeek(row)) {
        setRow(row);
        return true;
    }
    if (m_rowCacheEnd > 0 && m_colCount>0){
        setRow(cacheCount());
    }
    while (m_row < (row + 1)) {
        if (!cacheNext()) {
            if (canSeek(row)){
                break;
            }
            return false;
        }
    }
    setRow(row);
    return true;
}

bool LSqlCachedResult::fetchNext()
{
    if (canSeek(m_row + 1)) {
        setRow(m_row + 1);
        return true;
    }
    return cacheNext();
}

bool LSqlCachedResult::fetchPrevious()
{
    return fetch(m_row - 1);
}

bool LSqlCachedResult::fetchFirst()
{
    if (m_forwardOnly && m_row != (int)LSqlLocation::BeforeFirstRow) {
        return false;
    }
    if (canSeek(0)) {
        setRow(0);
        return true;
    }
    return cacheNext();
}

bool LSqlCachedResult::fetchLast()
{
    if (m_atEnd) {
        if (m_forwardOnly){
            return false;
        }else{
            return m_colCount >0 ? fetch(cacheCount() - 1) : false;
        }
    }

    int row = m_row;
    while (fetchNext()){
        ++row; /* brute force */
    }
    if (m_forwardOnly && m_row == (int)LSqlLocation::AfterLastRow) {
        setRow(row);
        return true;
    } else {
        return fetch(row);
    }
}

void LSqlCachedResult::detachFromResultSet()
{
    cleanup();
}

void LSqlCachedResult::setPrecisionPolicy(LSqlPrecisionPolicy policy)
{
    LSqlResult::setPrecisionPolicy(policy);
    cleanup();
}

bool LSqlCachedResult::canSeek(int row) const
{
    if (m_forwardOnly || row < 0){
        return false;
    }
    return m_rowCacheEnd >= (row + 1) * m_colCount;
}

bool LSqlCachedResult::cacheNext()
{
    if (m_atEnd){
        return false;
    }

    if (isForwardOnly()) {
        m_cache.resize(m_colCount);
    }

    if (!gotoNext(m_cache, nextIndex())) {
        revertLast();
        m_atEnd = true;
        return false;
    }
    setRow(m_row + 1);
    return true;
}





LK_NAMESPACE_END
