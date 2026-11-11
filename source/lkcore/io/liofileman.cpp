#include "liofileman.h"

LK_NAMESPACE_BEGIN

LIOFileMan::LIOFileMan()
{

}

LIOFileMan::LIOFileMan(const std::string& file, std::ios_base::openmode mode)
    : m_fs(file, mode), m_file(file), m_mode(mode)
{
    if(mode & std::ios_base::in){
        m_pIs = &m_fs;
    }
    if(mode & (std::ios_base::out|std::ios_base::ate|std::ios_base::app|std::ios_base::trunc)){
        m_pOs = &m_fs;
    }
}

LIOFileMan::LIOFileMan(const std::filesystem::path &file, std::ios_base::openmode mode)
    : m_fs(file, mode), m_file(file.string()), m_mode(mode)
{
    if(mode & std::ios_base::in){
        m_pIs = &m_fs;
    }
    if(mode & (std::ios_base::out|std::ios_base::ate|std::ios_base::app|std::ios_base::trunc)){
        m_pOs = &m_fs;
    }
}

bool LIOFileMan::isOpen() const
{
    if(m_fs.is_open()){
        return LIOMan::isOpen();
    }
    return false;
}

bool LIOFileMan::isSeekable() const
{
    return true;
}

bool LIOFileMan::open()
{
    if(m_fs.is_open()){
        return true;
    }
    m_fs.open(m_file, m_mode);
    return m_fs.is_open();
}

bool LIOFileMan::close()
{
    m_fs.close();
    return !m_fs.is_open();
}

bool LIOFileMan::open(std::ios_base::openmode mode)
{
    if(m_fs.is_open()){
        m_fs.close();
    }

    if(mode & std::ios_base::in){
        m_pIs = &m_fs;
    }
    if(mode & (std::ios_base::out|std::ios_base::ate|std::ios_base::app|std::ios_base::trunc)){
        m_pOs = &m_fs;
    }
    m_fs.open(m_file, m_mode);
    return m_fs.is_open();
}

bool LIOFileMan::open(const std::string &file, std::ios_base::openmode mode)
{
    if(m_fs.is_open()){
        m_fs.close();
    }
    m_pIs = nullptr;
    m_pOs = nullptr;

    m_file = file;
    m_mode = mode;

    if(m_mode & std::ios_base::in){
        m_pIs = &m_fs;
    }
    if(m_mode & (std::ios_base::out|std::ios_base::ate|std::ios_base::app|std::ios_base::trunc)){
        m_pOs = &m_fs;
    }
    m_fs.open(m_file, m_mode);
    return m_fs.is_open();
}

bool LIOFileMan::open(const std::filesystem::path &file, std::ios_base::openmode mode)
{
    if(m_fs.is_open()){
        m_fs.close();
    }
    m_pIs = nullptr;
    m_pOs = nullptr;

    m_file = file.string();
    m_mode = mode;

    if(m_mode & std::ios_base::in){
        m_pIs = &m_fs;
    }
    if(m_mode & (std::ios_base::out|std::ios_base::ate|std::ios_base::app|std::ios_base::trunc)){
        m_pOs = &m_fs;
    }
    m_fs.open(file, m_mode);
    return m_fs.is_open();
}

bool LIOFileMan::exists()
{
    return std::filesystem::exists(m_file);
}

bool LIOFileMan::remove()
{
    return std::filesystem::remove(m_file);
}

bool LIOFileMan::resize(int64_t size)
{
    if(size>=0){
        if(isOpen()){
            if(m_pIs != nullptr){
                int64_t pos = tellg();
                if(pos > size){
                    seekg(size);
                }
            }

            if(m_pOs != nullptr){
                int64_t pos = tellp();
                if(pos > size){
                    seekp(size);
                }
            }
        }

        std::filesystem::resize_file(m_file, size);
        return std::filesystem::file_size(m_file) == size;
    }
    return false;
}
LK_NAMESPACE_END
