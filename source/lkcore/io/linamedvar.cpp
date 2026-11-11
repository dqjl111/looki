#include "linamedvar.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

LINamedVar::LINamedVar()
{

}

//INamedVar::INamedVar(const std::string &_name)
//{

//}

LINamedVar::~LINamedVar()
{

}

LINamedVar& LINamedVar::operator=(const LVariant &var)
{
    setVariant(var);
    return *this;
}

std::string LINamedVar::toEquationString() const
{
    if(isEmpty())
    {
        return getName() + "=\'\'";
    }
    if(isNumber()){
        return getName() + "=" + getVariant().toString();
    }else{
        return getName() + "=\'" + getVariant().toString() + "\'";
    }
}
std::string LINamedVar::toLikeString() const
{
    if(isEmpty())
    {
        return getName() + " like \'%\'";
    }
    else
    {
        return getName() +  " like \'" + getVariant().toString()  + "%\'";
    }
}

bool LINamedVar::isList() const
{
    return false;
}
//////////////////////////////////////////////////////////////////
LIRefNamedVar::LIRefNamedVar(const std::string &_name):name(_name)
{

}

const std::string& LIRefNamedVar::getName() const
{
    return name;
}

//////////////////////////////////////////////////////////////////
LIDynNamedVar::LIDynNamedVar()
{

}

LIDynNamedVar::LIDynNamedVar(const std::string &_name) : name(_name)
{
    // lWarn() << _name << "," << name;
}

const std::string& LIDynNamedVar::getName() const
{
    return name;
}

LK_NAMESPACE_END
