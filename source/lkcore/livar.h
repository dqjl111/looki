#ifndef LIVAR_H
#define LIVAR_H

#include "lkcore/lookiconfig.h"
#include "lkcore/io/lcbor.h"
#include <functional>
#include <memory>
#include <concepts>
#include <unordered_map>
#include <typeindex>
#include "lkcore/3rd/nameof.h"
#include "lkcore/lstringutil.h"

LK_NAMESPACE_BEGIN

class LKCOREDEF LIVar  : public std::enable_shared_from_this<LIVar>
{
public:
    LIVar();
    virtual ~LIVar();

    //注意shared_from_this不能在构造函数中调用，getPtrT也不能，如果有需要要另外init，见makeInit
    template<class T>
    std::shared_ptr<T> getPtrT() const{
        std::shared_ptr<LIVar> t = const_cast<LIVar*>(this)->shared_from_this();
        return std::dynamic_pointer_cast<T>(t);
    }

    virtual LCbor toCbor() const{
        return LCbor();
    }

    virtual bool fromCbor(const LCbor&){
        return false;
    }

    virtual void init(){}
    std::string typeName() const;
};

template< class T, class... Args>
std::shared_ptr<T> makeInit(Args&&... args){
    std::shared_ptr<T> t = std::make_shared<T>(args...);
    t->init();
    return t;
}

typedef std::function<std::shared_ptr<LIVar>()> LIVarCreator;

template<class T>
LIVarCreator makeCreator(){
    LIVarCreator fn =
        []() {
            std::shared_ptr<T> t = std::make_shared<T>();
            t->init();
            return t;
        };
    return fn;
}

template<class T, class... Args>
LIVarCreator makeCreator(Args&&... args){
    std::function<void(Args&...)> fn =
        [](Args&... args) {
            std::shared_ptr<T> t = std::make_shared<T>(args...);
            t->init();
            return t;
        };

    return std::bind(fn, args...);
}


class LKCOREDEF LIVarTag{
protected:
    static inline std::unordered_map<std::string, LIVarTag*>& tags(){
        static std::unordered_map<std::string, LIVarTag*>  t;
        return t;
    }
public:
    LIVarTag();
    virtual const std::string& typeName() const = 0;
    virtual const std::type_info& typeInfo() const = 0;
    std::type_index typeIndex() const{
        return std::type_index(typeInfo());
    }

    virtual const LIVarTag* baseInst() const = 0;
    const std::string& baseTypeName() const{
        const LIVarTag* pTag = baseInst();
        if(pTag == nullptr){
            return LStringUtil::empty;
        }
        return pTag->typeName();
    }

    const std::type_info& baseTypeInfo() const{
        const LIVarTag* pTag = baseInst();
        if(pTag == nullptr){
            return typeid(LIVar);
        }
        return pTag->typeInfo();
    }

    std::type_index baseTypeIndex() const{
        return std::type_index(baseTypeInfo());
    }

    virtual bool isMatch(const std::shared_ptr<LIVar> w) const = 0;
    virtual bool isMatch(const LIVarTag* t) const = 0;
    virtual std::shared_ptr<LIVar> create() const = 0;

    void fetchTags(std::list<const LIVarTag*>& tags) const{
        const LIVarTag* pTag = baseInst();
        if(pTag != nullptr){
            pTag->fetchTags(tags);
        }
        tags.push_back(this);
    }

    std::list<const LIVarTag*> listTags() const{
        std::list<const LIVarTag*> ret;
        fetchTags(ret);
        return ret;
    }

    static LIVarTag* tag(const std::string& tagName);

    static bool isMatchBefore(const std::string& type1, const std::string& type2);
    static bool isMatchAfter(const std::string& type1, const std::string& type2);

    static bool isMatchBefore(const LIVarTag* tag1, const LIVarTag* tag2);
    static bool isMatchAfter(const LIVarTag* tag1, const LIVarTag* tag2);

    static const LIVarTag* inst(){
        return nullptr;
    }
};

#define _LIVarTagInst(PTag) \
    static inline const LIVarTag* p##PTag = PTag::inst(); \
    virtual const LIVarTag* tag() const{ return p##PTag; }

#define _LIVarTagInstOverride(PTag) \
    static inline const LIVarTag* p##PTag = PTag::inst();\
    virtual const LIVarTag* tag() const override { return p##PTag; }

#define _LIVarTag(T) typedef LIVarTagT<T> LIVarTag##T; _LIVarTagInst(LIVarTag##T)
#define _LIVarTagEx(T, P) typedef LIVarTagT<T, LIVarTag##P> LIVarTag##T; _LIVarTagInstOverride(LIVarTag##T)



template<class T, class PTag = LIVarTag>
class LIVarTagT : public PTag{
public:
    LIVarTagT(){
        LIVarTag::tags()[typeNameT()] = this;
    }

    static const std::string& typeNameT(){
        const static std::string& name = std::string(NAMEOF_SHORT_TYPE(T));
        return name;
    }

    virtual const std::string& typeName() const override{
        return typeNameT();
    }

    virtual const std::type_info& typeInfo() const override{
        return typeid(T);
    }

    virtual const LIVarTag* baseInst() const override{
        return PTag::inst();
    }

    virtual bool isMatch(const std::shared_ptr<LIVar> w) const override{
        return std::dynamic_pointer_cast<T>(w) != nullptr;
    }

    virtual bool isMatch(const LIVarTag* t) const override{
        return dynamic_cast<const LIVarTagT<T, PTag>*>(t) != nullptr;
    }

    virtual std::shared_ptr<LIVar> create() const override{
        std::shared_ptr<T> t = std::make_shared<T>();
        t->init();
        return t;
    }

    static const LIVarTag* inst(){
        const static LIVarTagT<T, PTag> tag;
        return &tag;
    }
};



LK_NAMESPACE_END

#endif // LIVAR_H
