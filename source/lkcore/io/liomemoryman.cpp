#include "liomemoryman.h"
LK_NAMESPACE_BEGIN


LIOMemoryMan::LIOMemoryMan(uint8_t* base, int64_t size,  std::ios_base::openmode mode)
    :m_stream(std::span<char>((char*)base, size), mode)
{
    if(mode & std::ios_base::in){
        m_pIs = &m_stream;
    }
    if(mode & (std::ios_base::out|std::ios_base::ate|std::ios_base::app|std::ios_base::trunc)){
        m_pOs = &m_stream;
    }
}

LIOMemoryMan::LIOMemoryMan(const std::vector<uint8_t> &vec)
    :m_stream(std::span<char>((char*)vec.data(), vec.size()), std::ios::binary|std::ios::in)
{
    m_pIs = &m_stream;
}

bool LIOMemoryMan::isSeekable() const
{
    return true;
}

int64_t LIOMemoryMan::sizeg() const
{
    if(m_pIs != nullptr){
        return m_stream.span().size_bytes();
    }
    return -1;
}

int64_t LIOMemoryMan::sizep() const
{
    if(m_pOs != nullptr){
        return m_stream.span().size_bytes();
    }
    return -1;
}




LK_NAMESPACE_END
