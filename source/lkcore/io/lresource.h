#ifndef LRESOURCE_H
#define LRESOURCE_H

#include "lkcore/io/lcbor.h"
#include <unordered_map>
#include "lkcore/lstringutil.h"

LK_NAMESPACE_BEGIN


class LKCOREDEF LIResRoot{
public:
    LIResRoot(){}

    enum ResType { ResBuiltin, ResFile, ResBuffer };
    virtual ResType type() const = 0;
    virtual const std::string& mappingRoot() const { return LStringUtil::empty; }
    virtual const uint8_t *data(int node, int64_t *size) const = 0;
    virtual int findNode(const std::string &_path) const = 0;
    virtual std::string name(int node) const = 0;
    virtual std::list<std::string> children(int node) const = 0;
    virtual bool isCompressed(int node) const = 0;
    virtual bool isContainer(int node) const = 0;
};


class LKCOREDEF LResource
{
public:
    LResource(const std::string& fileName = "");

    static void registResourceData(const uint8_t *tree, const uint8_t *name, const uint8_t *data);
    static void unregistResourceData(const uint8_t *tree, const uint8_t *name, const uint8_t *data);

    void clear();
    void setFileName(const std::string& fileName);
    inline const std::string& fileName(){
        return m_fileName;
    }

    static std::string cleanPath(const std::string& fileName);

    int64_t size() const;
    const uint8_t *data() const;
    bool isCompress() const;
    std::vector<uint8_t> uncompressedData() const;


protected: 
    void ensureInitialized() const;
    bool load(const std::string &file);

    std::string m_fileName;
    int64_t m_size = -1;
    const uint8_t *m_data = nullptr;
    bool m_isCompress = false;

};

LK_NAMESPACE_END

#endif // LRESOURCE_H
