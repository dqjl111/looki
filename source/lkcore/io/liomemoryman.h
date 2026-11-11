#ifndef LIOMEMORYMAN_H
#define LIOMEMORYMAN_H

#include "lioman.h"
#include <spanstream>

LK_NAMESPACE_BEGIN

class LKCOREDEF LIOMemoryMan  : public LIOMan
{
public:
    LIOMemoryMan(uint8_t* base, int64_t size,
                 std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out| std::ios_base::binary);

    LIOMemoryMan(const std::vector<uint8_t>& vec);

    virtual bool isSeekable() const override;
    virtual int64_t sizeg() const override;
    virtual int64_t sizep() const override;

protected:
    std::spanstream	m_stream;
};

//可以使用std::vector<uint8_t, std::polymorphic_allocator<uint8_t>>;
template <class _Alloc = std::allocator<uint8_t>>
class LVecMan : public LIOMemoryMan
{
public:
    LVecMan(std::vector<uint8_t, _Alloc>& vec, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out| std::ios_base::binary)
        : LIOMemoryMan(vec.data(), vec.size(), mode), m_vec(vec){
    }

    virtual std::vector<uint8_t, _Alloc> readAll() override{
        if(m_pIs != nullptr){
            return m_vec;
        }
        return std::vector<uint8_t>();
    }
protected:
    virtual void resizeForWrite(int64_t s) override{
        std::streampos org = tellp();
        int64_t len = org + (1 + s);
        if(len > (int64_t)m_vec.size()){
            m_vec.resize(len);
            m_stream.span(std::span<char, std::dynamic_extent>((char*)m_vec.data(), m_vec.size()));
            m_stream.seekp(org);
        }
    }
    std::vector<uint8_t, _Alloc>& m_vec;
};

LK_NAMESPACE_END

#endif // LIOMEMORYMAN_H
