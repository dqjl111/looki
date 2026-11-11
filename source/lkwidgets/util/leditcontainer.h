#ifndef LEDITCONTAINER_H
#define LEDITCONTAINER_H
#include "leditbase.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LEditContainer : public LEditBase
{
public:
    LEditContainer();
    virtual bool isDynChild() const;
    virtual bool isContainer() const final;
    virtual uint32_t type() const override;

    virtual LCbor::map toCbor(bool filterSelect = false) const override;
    LCbor::array childrenCbor(int pos=0, int len=-1);

    virtual bool fromCbor(const LCbor::map& cborMap,
                          bool checkType ,
                          bool newStyle,
                          std::shared_ptr<LEditDoc> pDoc=nullptr) override;

    inline const std::list<std::shared_ptr<LEditBase> >& children() const{
        return m_children;
    }

    inline int childIndex(std::shared_ptr<LEditBase> c) const{
        return lItemIndex(m_children, c);
    }

    std::shared_ptr<LEditBase> firstBlock() const;
    std::shared_ptr<LEditBase> nextBlock() const;
    std::shared_ptr<LEditBase> previousBlock() const;
    std::shared_ptr<LEditBase> lastBlock() const;

    bool isAncestor(std::shared_ptr<LEditBase> t) const;
    bool isAcceptableChild(std::shared_ptr<LEditBase> t)  const;

    inline std::shared_ptr<LEditBase> activeChild() const{
        return m_activeChild;
    }
    void setActiveChild(std::shared_ptr<LEditBase> child);

    inline bool hasChildren() const{
        return !m_children.empty();
    }

    virtual void draw( LPainter *painter,
                      const LPoint<double> &pos = LPoint<double>(),
                      int flags = (int)(DrawFlags::All),
                      const LRect<double> &clip = LRect<double>()) const override;
    virtual void drawCursor(LPainter *p,
                            const LPoint<double> &pos = LPoint<double>(),
                            int width = 1,
                            const LRect<double> &clip = LRect<double>()) const override;

    virtual void calculateRect();
    virtual void calculatePos();

    virtual bool hasSelect() const override;
    virtual bool isAllSelect() const override;

    virtual void deSelect() override;
    virtual void selectAll(bool isLeftToRight = true) override;

    virtual  std::list<std::shared_ptr<LEditOp>> removeSelection() override;

    virtual bool moveCursor(LMoveOper op) override;

    virtual int maxIndex() const override;
    virtual int minIndex() const override;
    virtual int activePos() const override;
    virtual void setActivePos(int pos) override;

    virtual bool canStrechChild(std::shared_ptr<LEditBase> ch) const;

    virtual bool isStretchX() const override;
    virtual bool isStretchY() const override;

    virtual bool isAtStart() const override;
    virtual bool isAtEnd() const override;

    virtual void moveToStart() override;
    virtual void moveToEnd() override;

    virtual bool isTextEmpty();

    bool deleteChild(std::shared_ptr<LEditBase> child);
    std::shared_ptr<LEditBase> childAtPoint(const LPoint<double>& p) const;
    std::shared_ptr<LEditBase> leafAtPoint(const LPoint<double>& p) const;

    virtual LPoint<double> activePoint() const override;
    virtual bool setToActivePoint(const LPoint<double>& p) override;
protected:

    std::list<std::shared_ptr<LEditBase> > m_children;
    std::shared_ptr<LEditBase> m_activeChild = nullptr;

    virtual bool setToRowChild(std::shared_ptr<LEditBase> t, int index);
    virtual void calculateSize() override;
    virtual void stretchSize() override;

    virtual int del(int pos, int length=1);
    virtual bool insert(std::shared_ptr<LEditBase> child, int pos = 0);
    virtual void clear() override;

    virtual void markSelectStart() override;
    virtual void selectFrom(const std::vector<int>& indexes, int level, uint8_t type=SelectAsValue) override;
    virtual void selectTo(const std::vector<int>& indexes, int level, uint8_t type=SelectAsValue) override;
    virtual void selectRange(const std::vector<int>& start,
                             const std::vector<int>& end,
                             int level,
                             uint8_t type=SelectAsValue) override;

    friend class LIEditor;
    friend class LItemInputDelete;
};


LK_NAMESPACE_END
#endif // LEDITCONTAINER_H
