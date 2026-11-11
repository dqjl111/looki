#ifndef LINAMEDVAR_H
#define LINAMEDVAR_H
#include <string>
#include "lkcore/lvariant.h"
#include "lkcore/lstringutil.h"

LK_NAMESPACE_BEGIN

class LKCOREDEF LINamedVar
{
public:
    LINamedVar();
    virtual ~LINamedVar();

    virtual const std::string& getName() const = 0;
    virtual LVariant getVariant() const = 0;
    virtual void setVariant(const LVariant& var) = 0;
    virtual bool isEmpty() const = 0;
    virtual void reSet() = 0;
    virtual bool isNumber() const = 0;
    virtual std::shared_ptr<LINamedVar> newVar() const = 0;

    LINamedVar& operator=(const LVariant& var);
    std::string toEquationString() const;
    std::string toLikeString() const;

    virtual bool isList() const;
protected:
};



//////////////////////////////////////////////////////////////////
class LKCOREDEF LIRefNamedVar : public LINamedVar
{
public:
    LIRefNamedVar(const std::string& _name = LStringUtil::empty);
    virtual const std::string& getName() const override;
protected:
    const std::string& name;
};

//////////////////////////////////////////////////////////////////
class LKCOREDEF LIDynNamedVar : public LINamedVar
{
public:
    LIDynNamedVar();
    LIDynNamedVar(const std::string& _name);

    virtual const std::string& getName() const override;
    std::string name;
};


LK_NAMESPACE_END

#endif // LINAMEDVAR_H
