#include "lkcore/3rd/miniz_cpp.hpp"
#include "lioman.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

LIOMan::LIOMan()
{
    m_isLittleEndian = isSysLittleEndian();
}

bool LIOMan::isOpen() const
{
    return m_pIs != nullptr || m_pOs != nullptr;
}

bool LIOMan::isSeekable() const
{
    return false;
}

bool LIOMan::open()
{
    return true;
}

bool LIOMan::close()
{
    return true;
}

int64_t LIOMan::sizeg() const
{
    if(m_pIs != nullptr){
        if(isSeekable()){
            std::streampos pos = m_pIs->tellg();
            m_pIs->seekg(0, std::ios::end);
            int64_t size = m_pIs->tellg();
            m_pIs->seekg(pos);
            return size;
        }
    }
    return -1;
}

int64_t LIOMan::sizep() const
{
    if(m_pIs != nullptr){
        if(isSeekable()){
            std::streampos pos = m_pOs->tellp();
            m_pOs->seekp(0, std::ios::end);
            int64_t size = m_pOs->tellp();
            m_pOs->seekp(pos);
            return size;
        }
    }
    return -1;
}

bool LIOMan::isReadable() const
{
    return m_pIs != nullptr;
}

bool LIOMan::isWritable() const
{
    return m_pOs != nullptr;
}

void LIOMan::setIsLittleEndian(bool isLittle)
{
    if(isLittle != m_isLittleEndian){
        m_isLittleEndian = isLittle;
        if(m_isLittleEndian == isSysLittleEndian()){
            m_needSwap = false;
        }else{
            m_needSwap = true;
        }
    }
}

int64_t LIOMan::readRawData(uint8_t *data, int64_t len)
{
    if(m_pIs != nullptr){
        m_pIs->read((char*)data, len);
        return m_pIs->gcount();
    }
    return 0;
}

std::vector<uint8_t> LIOMan::readAll()
{
    std::vector<uint8_t> m_allBuf;
    if(m_pIs != nullptr){
        int64_t size = -1;
        if(isSeekable()){
            std::streampos pos = m_pIs->tellg();
            m_pIs->seekg(0, std::ios::end);
            size = m_pIs->tellg()-pos;
            m_pIs->seekg(pos);
        }

        if(size >0){
            m_allBuf.resize(size);
        }else{
            m_allBuf.resize(1024);
        }
        int64_t len = 0;
        int64_t currentPos = 0;
        while(!atReadEnd()){
            m_pIs->read((char*)(m_allBuf.data() + currentPos), m_allBuf.size()-currentPos);
            len = m_pIs->gcount();

            if(len >0){
                currentPos += len;
                if(!atReadEnd()){
                    m_allBuf.resize(m_allBuf.size()*2);
                }
            }else{
                break;
            }
        }
        m_allBuf.resize(currentPos);
    }
    return m_allBuf;
}

bool LIOMan::writeRawData(const uint8_t *s, int64_t len)
{
    if(m_pOs != nullptr){
        resizeForWrite(len);
        m_pOs->write((char*)s, len);
        return m_pOs->good();
    }
    return false;
}

bool LIOMan::peek(uint8_t *data, int64_t maxSize)
{
    if((maxSize>0) && (data != nullptr)){
        memset((char*)data, 0, maxSize);
        if(m_pIs != nullptr){
            if(maxSize == 1){
                data[0] = m_pIs->peek();
                return m_pIs->good();
            }else{
                auto pos = m_pIs->tellg();
                m_pIs->read((char*)data, maxSize);
                m_pIs->seekg(pos);
                return m_pIs->good();
            }
        }
    }
    return false;
}

bool LIOMan::seekg(const std::streampos& pos)
{
    if(m_pIs != nullptr){
        m_pIs->seekg(pos);
        return m_pIs->good();
    }
    return false;
}

bool LIOMan::seekp(const std::streampos& pos)
{
    if(m_pOs != nullptr){
        m_pOs->seekp(pos);
        return m_pOs->good();
    }
    return false;
}

std::streampos LIOMan::tellg()
{
    if(m_pIs != nullptr){
        return m_pIs->tellg();
    }
    return std::streampos();
}

std::streampos LIOMan::tellp()
{
    if(m_pOs != nullptr){
        return m_pOs->tellp();
    }
    return std::streampos();
}

bool LIOMan::readSkip(int64_t num)
{
    if(m_pIs != nullptr){
        m_pIs->seekg(m_pIs->tellg()+num);
        return m_pIs->good();
    }
    return false;
}

bool LIOMan::hasReadError()
{
    if(m_pIs != nullptr){
        //lWarn() <<"read state:" <<  m_pIs->rdstate();
        return !m_pIs->good();
    }
    return true;
}

bool LIOMan::hasWriteError()
{
    if(m_pOs != nullptr){
        return !m_pOs->good();
    }
    return true;
}

bool LIOMan::atReadEnd()
{
    if(m_pIs != nullptr){
        return m_pIs->eof();
    }
    return true;
}

bool LIOMan::atWriteEnd()
{
    if(m_pOs != nullptr){
        return m_pOs->eof();
    }
    return true;
}

void LIOMan::flush()
{
    if(m_pOs != nullptr){
        m_pOs->flush();
    }
}

std::vector<uint8_t> LIOMan::uncompressData(const std::vector<uint8_t> &data)
{
    miniz_cpp::zip_file zip;
    zip.load(data);

    std::vector<miniz_cpp::zip_info> infos = zip.infolist();
    if(!infos.empty()){
        return zip.readData(infos[0]);
    }
    return std::vector<uint8_t>();

}

std::vector<uint8_t> LIOMan::compressData(const std::vector<uint8_t> &data)
{
    miniz_cpp::zip_file zip;
    zip.writeData("", data);
    std::vector<uint8_t> ret;
    zip.save(ret);
    return ret;
}

void LIOMan::resizeForWrite(int64_t s)
{

}

LIOMan& LIOMan::operator>>(bool &i){
    uint8_t t = 0;
    if(m_pIs != nullptr){
        m_pIs->read((char*)(&t), 1);
    }
    i = (t != 0);
    return *this;
}

LIOMan& LIOMan::operator>>(float &f){
    union {
        float val1 =0;
        uint32_t val2;
    } x;
    read<uint32_t>(x.val2);
    f = x.val1;
    return *this;
}

LIOMan& LIOMan::operator>>(double &f){
    union {
        double val1 = 0;
        uint64_t val2;
    } x;
    read<uint64_t>(x.val2);
    f =  x.val1;
    return *this;
}

LIOMan& LIOMan::operator>>(std::string& s){
    uint32_t size = 0;
    read<uint32_t>(size);
    s.resize(size);
    if(size >0){
        readRawData((uint8_t*)s.data(), size);
    }
    return *this;
}

LIOMan& LIOMan::operator>>(std::wstring& s){
    std::string t;
    *this >> t;
    s = LStringUtil::utf8ToWide(t);
    return *this;
}


LIOMan& LIOMan::operator>>(LCbor &f){
    if(m_pIs != nullptr){
        f.read(*m_pIs);
    }
    return *this;
}

LIOMan& LIOMan::operator<<(const bool& i){
    if(m_pOs != nullptr){
        m_pOs->write((char*)(&i), 1);
    }
    return *this;
}

LIOMan& LIOMan::operator<<(const float &f){
    union {
        float val1;
        uint32_t val2;
    } x;
    x.val1 = f;
    return write<uint32_t>(x.val2);
}

LIOMan& LIOMan::operator<<(const double &f){
    union {
        double val1;
        uint64_t val2;
    } x;
    x.val1 = f;
    return write<uint64_t>(x.val2);
}

LIOMan& LIOMan::operator<<(const std::string& s){
    write<uint32_t>((uint32_t)s.length());
    if(s.length() >0){
        writeRawData((uint8_t*)s.data(), s.length());
    }
    return *this;
}


LIOMan& LIOMan::operator<<(const std::wstring& s){
    std::string ts = LStringUtil::wideToUtf8(s);
    *this << ts;
    return *this;
}


LIOMan& LIOMan::operator<<(const LCbor &f){
    if(m_pOs != nullptr){
        f.write(*m_pOs);
    }
    return *this;
}


LK_NAMESPACE_END
