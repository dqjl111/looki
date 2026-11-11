#ifndef LIOMAN_H
#define LIOMAN_H

#include "lbyteswap.h"
#include "lkcore/io/lcbor.h"
#include "lkcore/ldebug.h"
#include <iostream>

LK_NAMESPACE_BEGIN

class LKCOREDEF LIOMan
{
public:
    LIOMan();

    virtual bool isOpen() const;
    virtual bool isSeekable() const;
    virtual bool open();
    virtual bool close();
    virtual int64_t sizeg() const;
    virtual int64_t sizep() const;

    virtual bool isReadable() const;
    virtual bool isWritable() const;

    static inline constexpr bool isSysLittleEndian(){
        return LOOKI_LITTLE_ENDIAN == 1;
    }

    inline bool isLittleEndian(){
        return m_isLittleEndian;
    }

    void setIsLittleEndian(bool isLittle);


    inline LIOMan& operator>>(int8_t &v){
        if(m_pIs != nullptr){
            m_pIs->read((char*)(&v), 1);
        }
        return *this;
    }
    inline LIOMan& operator>>(uint8_t &v){
        if(m_pIs != nullptr){
            m_pIs->read((char*)(&v), 1);
        }
        return *this;
    }
    inline LIOMan& operator>>(int16_t &i){
        return read<int16_t>(i);
    }
    inline LIOMan& operator>>(uint16_t &i){
        return read<uint16_t>(i);
    }
    inline LIOMan& operator>>(int32_t &i){
        return read<int32_t>(i);
    }
    inline LIOMan& operator>>(uint32_t &i){
        return read<uint32_t>(i);
    }
    inline LIOMan& operator>>(int64_t &i){
        return read<int64_t>(i);
    }
    LIOMan& operator>>(uint64_t &i){
        return read<uint64_t>(i);
    }
    LIOMan& operator>>(bool &i);
    LIOMan& operator>>(float &f);
    LIOMan& operator>>(double &f);
    LIOMan& operator>>(std::string& s);
    LIOMan& operator>>(std::wstring& s);
    LIOMan& operator>>(LCbor &f);

    inline LIOMan& operator<<(const int8_t &v){
        if(m_pOs != nullptr){
            resizeForWrite(1);
            m_pOs->write((const char*)(&v), 1);
        }
        return *this;
    }
    inline LIOMan& operator<<(const uint8_t &v){
        if(m_pOs != nullptr){
            resizeForWrite(1);
            m_pOs->write((const char*)(&v), 1);
        }
        return *this;
    }
    inline LIOMan& operator<<(const int16_t &i){
        return write<int16_t>(i);
    }
    inline LIOMan& operator<<(const uint16_t &i){
        return write<uint16_t>(i);
    }
    inline LIOMan& operator<<(const int32_t &i){
        return write<int32_t>(i);
    }
    inline LIOMan& operator<<(const uint32_t &i){
        return write<uint32_t>(i);
    }
    inline LIOMan& operator<<(const int64_t &i){
        return write<uint64_t>(i);
    }
    LIOMan& operator<<(const uint64_t &i){
        return write<uint64_t>(i);
    }

    LIOMan& operator<<(const bool& i);
    LIOMan& operator<<(const float& f);
    LIOMan& operator<<(const double& f);
    LIOMan& operator<<(const std::string& s);
    LIOMan& operator<<(const std::wstring& s);
    LIOMan& operator<<(const LCbor &f);

    int64_t readRawData(uint8_t *, int64_t len);
    virtual std::vector<uint8_t> readAll();

    bool writeRawData(const uint8_t *, int64_t len);
    bool peek(uint8_t *data, int64_t maxSize);
    inline bool getByte(uint8_t& c){
        return readRawData(&c, 1);
    }

    inline bool putByte(const uint8_t& c){
        return writeRawData(&c, 1);
    }

    /*变更输入位置*/
    bool seekg(const std::streampos& pos);
    /*变更输出位置*/
    bool seekp(const std::streampos& pos);

    /*获取输入位置*/
    std::streampos tellg();
    /*获取输出位置*/
    std::streampos tellp();

    bool readSkip(int64_t num = 1);
    bool hasReadError();
    bool hasWriteError();

    bool atReadEnd();
    bool atWriteEnd();

    void flush();

    static std::vector<uint8_t> uncompressData(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> compressData(const std::vector<uint8_t>& data);
protected:
    virtual void resizeForWrite(int64_t s);

    std::istream* m_pIs= nullptr;
    std::ostream* m_pOs = nullptr;

    template<typename T>
    LIOMan& read(T& v){
        if(m_pIs != nullptr){
            m_pIs->read((char*)(&v), sizeof(T));
            if(m_needSwap){
                v = byteswap<T>(v);
            }
        }
        return *this;
    }

    template<typename T>
    LIOMan& write(const T& v){
        if(m_pOs != nullptr){
            resizeForWrite(sizeof(T));
            if(m_needSwap){
                T t = byteswap<T>(v);
                m_pOs->write((const char*)(&t), sizeof(T));
            }else{
                m_pOs->write((const char*)(&v), sizeof(T));
            }
        }
        return *this;
    }

    bool m_isLittleEndian = true;
    bool m_needSwap = false;
};

LK_NAMESPACE_END

#endif // LIOMAN_H
