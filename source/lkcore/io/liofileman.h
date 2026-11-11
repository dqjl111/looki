#ifndef LIOFILEMAN_H
#define LIOFILEMAN_H

#include "lioman.h"
#include <fstream>
#include <filesystem>

LK_NAMESPACE_BEGIN

class LKCOREDEF LIOFileMan : public LIOMan
{
public:
    LIOFileMan();

    LIOFileMan(const std::string& file,
               std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out| std::ios_base::binary);
    LIOFileMan(const std::filesystem::path& file,
               std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out| std::ios_base::binary);

    virtual bool isOpen() const override;
    virtual bool isSeekable() const override;

    virtual bool open() override;
    virtual bool close() override;

    bool open(std::ios_base::openmode mode);
    bool open(const std::string& file,
              std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out| std::ios_base::binary);
    bool open(const std::filesystem::path& file,
              std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out| std::ios_base::binary);

    bool exists();
    bool remove();
    bool resize(int64_t size);
    inline int64_t size() const{
        return std::filesystem::file_size(m_file);
    }

    inline const std::string& file() const{
        return m_file;
    }
protected:
    std::fstream m_fs;
    std::string m_file;
    std::ios_base::openmode m_mode;
};

LK_NAMESPACE_END

#endif // LIOFILEMAN_H
