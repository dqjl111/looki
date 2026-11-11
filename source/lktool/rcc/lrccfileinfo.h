#ifndef LRCCFILEINFO_H
#define LRCCFILEINFO_H

#include "lkcore/io/lioman.h"
#include <filesystem>
#include <list>

LK_NAMESPACE_BEGIN

class LRccWriter;
class LRccFileInfo
{
public:
    LRccFileInfo();

    inline const std::string& name() const{
        return m_name;
    }

    std::shared_ptr<LRccFileInfo> findChild(const std::string& node);
    bool contains(const std::string& node);
protected:
    std::weak_ptr<LRccFileInfo> m_parent;
    std::list<std::shared_ptr<LRccFileInfo> > m_children;
    std::filesystem::path m_path;
    std::string  m_name;

    union{
        uint8_t m_flags = 0;
        struct{
            uint8_t m_isDir : 1;
            uint8_t m_isCompress : 1;
        };
    };


    int32_t m_nameOffset = 0;
    int32_t m_dataOffset = 0;
    int32_t m_childOffset = 0;

    friend class LRccWriter;
};


LK_NAMESPACE_END

#endif // LRCCFILEINFO_H
