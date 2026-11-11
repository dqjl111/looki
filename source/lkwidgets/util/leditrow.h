#ifndef LEDITROW_H
#define LEDITROW_H
#include "leditcontainer.h"
#include "ledittext.h"
LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LEditRow : public LEditContainer
{
public:
    LEditRow();

    virtual bool isRow() const final;
    virtual uint32_t type() const override;
    virtual bool isDynChild() const override;

    virtual void calculatePos() override;
    virtual double upHeight() const override;

    virtual bool moveCursor(LMoveOper op) override;
    virtual bool setToActivePoint(const LPoint<double>& p) override;

    std::list<std::shared_ptr<LEditOp>> input(const wchar_t ch);
    virtual std::list<std::shared_ptr<LEditOp>> input(const std::wstring& ch);
    virtual bool canMergeText(const std::wstring& ch,  std::shared_ptr<LEditText> textRow = nullptr);

    enum MergeType{NoMerge, Merge};
    std::list<std::shared_ptr<LEditOp>> putToActive(std::shared_ptr<LEditBase> child,
                                                    bool isForward=true,
                                                    MergeType mergeType =NoMerge);

    virtual const LMargins<double>& margins() const override{
        return m_margin;
    }

    bool isSingleText();
    bool append(std::shared_ptr<LEditBase> child);
    bool prepend(std::shared_ptr<LEditBase> child);

    std::shared_ptr<LEditText> blockCurrentText();

    std::shared_ptr<LEditText> rowCurrentText();
    std::shared_ptr<LEditText> rowFirstText();
    std::shared_ptr<LEditText> rowNextText();
    std::shared_ptr<LEditText> rowPreviousText();
    std::shared_ptr<LEditText> rowLastText();

    std::shared_ptr<LEditText> createText(const wchar_t ch,  std::shared_ptr<LEditText> textRow = nullptr);
    virtual std::shared_ptr<LEditText> createText(const std::wstring& ch,  std::shared_ptr<LEditText> textRow = nullptr);
protected:
    double m_baseUpHeight=0;
    double m_baseHeight=0;
    LMargins<double> m_margin;

    virtual void calculateSize() override;
    virtual bool setToRowChild(std::shared_ptr<LEditBase> t, int index) override;
    std::shared_ptr<LEditBase> merge(std::shared_ptr<LEditRow> row, int pos = 0);

    virtual void initMargin(double baseSize);
    virtual bool isAutoMargin();

    friend class LEditContainer;
    friend class LTextMergeSplit;
};

LK_NAMESPACE_END

#endif // LEDITROW_H
