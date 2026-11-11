#ifndef LNAMEDVALUE_H
#define LNAMEDVALUE_H
#include "linamedvar.h"
#include <limits.h>

LK_NAMESPACE_BEGIN

template<typename T>
concept TINameVar = std::same_as<T, LIRefNamedVar> || std::same_as<T, LIDynNamedVar>;

template<typename T>
concept TVarEmptyable = requires(const T t){ t.empty();LVariant(t);};

template<typename T>
concept TVarClearable = requires(T t){ t.clear();LVariant(t);};

template<typename T>
concept TVariable = requires(T t){ LVariant(t);};

template<typename T>
concept TVarVec = std::same_as<T, std::vector<typename T::value_type>>;

template<typename T>
inline std::enable_if_t<std::numeric_limits<T>::is_specialized , bool> typeIsEmpty(const T& v){
    return v == (std::numeric_limits<T>::max)();
}

template<typename T>
inline std::enable_if_t<std::numeric_limits<T>::is_specialized , void> typeReset(T& v){
    v = (std::numeric_limits<T>::max)();
}

template<typename T>
inline LVariant typeToVariant(const T& v) requires TVariable<T>{
    return LVariant(v);
}

template<typename T>
inline void typeFromVariant(T& v, const LVariant& var) requires TVariable<T>{
    if(var.isNull())
    {
        typeReset<T>(v);
    }else{
        v = var.get<T>();
    }
}

template<typename T>
inline LVariant typeToVariant(const T& v) requires TVarVec<T>{
    return LVariant((void*)v.data(), v.size()*sizeof(typename T::value_type));
}

template<typename T>
inline void typeFromVariant(T& v, const LVariant& var) requires TVarVec<T>{
    if(var.isNull())
    {
        v.clear();
    }else{
        v.resize(var.getByteSize()/sizeof(typename T::value_type));
        memcpy((char*)v.data(), var.getBinData(), var.getByteSize());
    }
}

template<typename T>
inline bool typeIsNumber(const T&){
    return  std::numeric_limits<T>::is_specialized;
}

///////////////////////////////
template<typename T>
inline  bool typeIsEmpty(const T& v) requires TVarEmptyable<T>{
    return v.empty();
}

template<typename T>
inline  void typeReset(T& v) requires TVarClearable<T>{
    return v.clear();
}

template<typename T>
inline  bool typeIsEmpty(const T& v) requires TVarVec<T>{
    return v.empty();
}

template<typename T>
inline  void typeReset(T& v) requires TVarVec<T>{
    return v.clear();
}

inline void typeFromVariant(std::string& v, const LVariant& var){
    v = var.toString();
}

inline void typeFromVariant(std::wstring& v, const LVariant& var){
    v = var.toWString();
}

//////////////////
inline  bool typeIsEmpty(const LSysTimePoint& v){
    return LSysClock::to_time_t(v) == (std::numeric_limits<__time64_t>::max)();
}

inline  void typeReset(LSysTimePoint& v){
    v = LSysClock::from_time_t((std::numeric_limits<__time64_t>::max)());
}
//////////////////
inline  bool typeIsEmpty(const LCbor& v){
    return v.isUndefined();
}

inline  void typeReset(LCbor& v){
    v = LCbor(LCbor::SIMPLE_UNDEFINED);
}
//////////////////
template<typename T, TINameVar P=LIRefNamedVar, bool varCanEmpty = true>
class LNamedVar : public P
{
public:
    LNamedVar() requires std::same_as<P, LIDynNamedVar>{}
    LNamedVar(const std::string &_name):P(_name){}
    LNamedVar(const std::string &_name, const T& value):P(_name), value(value){}

    virtual bool isEmpty() const override{
        return typeIsEmpty(value) && varCanEmpty;
    }

    virtual void reSet() override{
        typeReset(value);
    }
    virtual LVariant getVariant() const override{
        return typeToVariant(value);
    }
    virtual void setVariant(const LVariant& var) override{
        typeFromVariant(value, var);
    }

    virtual bool isNumber() const override{
        return typeIsNumber(value);
    }

    LNamedVar<T, P>& operator= (const T& _value){
        value = _value;
    }
    LNamedVar<T, P>& operator= (const LNamedVar<T, P>& _value){
        value = _value.value;
        return *this;
    }

    virtual std::shared_ptr<LINamedVar> newVar() const override{
        return std::make_shared<LNamedVar<T, P> >(P::getName());
    }

    T value;
};

LK_NAMESPACE_END

#define DECLARE_DYNVAR(T, defVar) _LK::LNamedVar<T, _LK::LIDynNamedVar, true> _##defVar; T& defVar;
#define DECLARE_REFVAR(T, defVar) _LK::LNamedVar<T, _LK::LIRefNamedVar, true> _##defVar; T& defVar;

#define DECLARE_DYNNOEMPTY(T, defVar) _LK::LNamedVar<T, _LK::LIDynNamedVar, false> _##defVar; T& defVar;
#define DECLARE_REFNOEMPTY(T, defVar) _LK::LNamedVar<T, _LK::LIRefNamedVar, false> _##defVar; T& defVar;

#define BIND_VAR(defVar, name) _##defVar(name),defVar(_##defVar.value)

#endif // NAMEDVALUE_H


