#ifndef LSQLCACHEDRESULT_H
#define LSQLCACHEDRESULT_H

#include "lksql/lsqlresult.h"

LK_NAMESPACE_BEGIN

class LKSQLDEF LSqlCachedResult : public LSqlResult
{
public:
    LSqlCachedResult(std::shared_ptr<LSqlDriver> driver);

    void initCols(int colCount);
    void cleanup();
    void clearValues();

    virtual bool gotoNext(std::list<LVariant> &values, int index) = 0;

    const virtual LVariant& data(int col) const override;
    virtual bool isNull(int  col) const override;
    virtual bool fetch(int row) override;
    virtual bool fetchNext() override;
    virtual bool fetchPrevious() override;
    virtual bool fetchFirst() override;
    virtual bool fetchLast() override;

    int colCount() const;
    std::list<LVariant>& cache();

    virtual void detachFromResultSet() override;
    virtual void setPrecisionPolicy(LSqlPrecisionPolicy policy) override;
private:
    bool canSeek(int row) const;
    bool cacheNext();
    void initCols(int count, bool isForward);
    inline int cacheCount() const{
        return  m_rowCacheEnd / m_colCount;
    }
    int nextIndex();
    void revertLast();

    std::list<LVariant> m_cache;
    int m_rowCacheEnd = 0;
    int m_colCount = 0;
    bool m_atEnd = false;
};


LK_NAMESPACE_END
#endif // LSQLCACHEDRESULT_H
