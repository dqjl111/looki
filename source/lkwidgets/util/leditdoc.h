#ifndef LEDITDOC_H
#define LEDITDOC_H
#include "lkcore/lobject.h"
#include "lkwidgets/util/lundostack.h"
#include "lkwidgets/util/leditbase.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LEditDoc : public LObject
{
public:
    LEditDoc();

    bool saveCbor(const std::string& filePath);
    bool loadCbor(const std::string& filePath);
    bool loadCbor(const std::vector<uint8_t>& data, bool canClip = false);
    bool loadCbor(const LCbor& cbor);


    std::shared_ptr<LEditBase> createBase(uint32_t type, bool tryGlobal = true) const;
    static std::shared_ptr<LEditBase> createBase(const LCbor& cbor,
                                                 bool tryGlobal = true,
                                                 std::shared_ptr<LEditDoc> doc = nullptr);

    static std::shared_ptr<LEditBase> createGlobalBase(uint32_t type);
    static std::shared_ptr<LEditBase> createGlobalBase(const LCbor& cbor);

    static void registGlobalBase(uint32_t type, const LEditBaseCreator& fn);
    static void removeGlobalBase(uint32_t type);
    static void initGlobal();
protected:
    virtual bool initEditor(const LCbor::map& cborMap);
    std::shared_ptr<LIEditor> m_editor;

    virtual const std::map<uint32_t, LEditBaseCreator>* docCreators() const;
    static std::map<uint32_t, LEditBaseCreator> m_globalCreators;
};


LK_NAMESPACE_END
#endif // LEDITDOC_H
