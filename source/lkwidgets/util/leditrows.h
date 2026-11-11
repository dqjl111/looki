#ifndef LEDITROWS_H
#define LEDITROWS_H
#include "lkwidgets/util/leditrow.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LEditRows: public LEditContainer
{
public:
    LEditRows();

    virtual uint32_t type() const override;
    virtual bool isDynChild() const override;

    bool append(std::shared_ptr<LEditRow> child);
    bool prepend(std::shared_ptr<LEditRow> child);
    virtual bool insert(std::shared_ptr<LEditBase> child, int pos = 0) override;

    virtual bool setToRowChild(std::shared_ptr<LEditBase> t, int index) override;

    std::shared_ptr<LEditRow> currentRow();
    std::shared_ptr<LEditRow> firstRow();
    std::shared_ptr<LEditRow> nextRow();
    std::shared_ptr<LEditRow> previousRow();
    std::shared_ptr<LEditRow> lastRow();

    virtual std::shared_ptr<LEditRow> createRow(uint32_t type= (uint32_t)LEditType::Unknown);

    virtual void calculateSize() override;
    virtual void calculatePos() override;

    virtual bool setToActivePoint(const LPoint<double>& p) override;
    virtual bool moveCursor(LMoveOper op) override;
protected:
    virtual bool setToActivePointIgnoreWidth(const LPoint<double>& p);
    std::shared_ptr<LEditRow> newRow(int pos = 0, uint32_t type= (uint32_t)LEditType::Unknown);
};


LK_NAMESPACE_END
#endif // LEDITROWS_H
