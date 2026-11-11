#ifndef LRCCWRITER_H
#define LRCCWRITER_H

#include "lkcore/io/lioman.h"
#include "lktool/rcc/lrccfileinfo.h"

LK_NAMESPACE_BEGIN

class LRccWriter
{
public:
    LRccWriter();

    enum Format { Binary, CppCode };

    bool output(const std::string& file);
    bool output(LIOMan &outMan);
    bool readFiles();

    std::string m_outName;
    std::list<std::string> m_rccFiles;
    Format m_format = Binary;
    bool m_compress = false;

protected:
    bool addFileInfo(const std::string &alias, std::shared_ptr<LRccFileInfo> info);
    bool interpretResourceFile(const std::filesystem::path& path);

    bool writeHeader();
    bool writeDataBlobs();
    bool writeDataNames();
    bool writeDataStructure();
    bool writeInitializer();


    int32_t writeDataBlob(std::shared_ptr<LRccFileInfo> info, int32_t offset);
    int32_t writeDataName(std::shared_ptr<LRccFileInfo> info, int32_t offset);
    void writeDataInfo(std::shared_ptr<LRccFileInfo> info);

    void writeDecimal(int value);
    void writeHex(uint8_t number);
    void write2HexDigits(uint8_t number);
    inline void writeuint8_t(uint8_t c) { m_out.push_back(c); }

    template<typename T>
    void writeNumber(T number){
        if (m_format == Binary) {
            writeuint8_t(number & 0xFF);
            for(uint8_t i=1; i<sizeof(T); ++i){
                number = number >> 8;
                writeuint8_t(number & 0xFF);
            }
        }else{
            writeHex(number & 0xFF);
            for(uint8_t i=1; i<sizeof(T); ++i){
                number = number >> 8;
                writeHex(number & 0xFF);
            }
        }
    }


    inline void writeNumber1(uint8_t number){
        if (m_format == Binary) {
            writeuint8_t(number & 0xFF);
        }else{
            writeHex(number & 0xFF);
        }
    }
    inline void writeNumber2(uint16_t number){
        writeNumber<uint16_t>(number);
    }
    inline void writeNumber4(uint32_t number){
        writeNumber<uint32_t>(number);
    }
    inline void writeNumber8(uint64_t number){
        writeNumber<uint64_t>(number);
    }


    void write(const uint8_t *, int len);
    inline void writeString(const char *s) { write((uint8_t*)s, static_cast<int>(strlen(s))); }
    inline void writeString(const std::string& s) { write((uint8_t*)s.data(), static_cast<int>(s.length())); }
    void sortFileInfos();
    inline void writeByteArray(std::vector<uint8_t> data){
        m_out.append_range(data);
    }

    inline void writeChar(char c) { m_out.push_back((uint8_t)c); }

    std::vector<uint8_t> m_out;
    std::shared_ptr<LRccFileInfo> m_root;

    int m_treeOffset = 0;
    int m_namesOffset = 0;
    int m_dataOffset = 0;
};

LK_NAMESPACE_END

#endif // LRCCWRITER_H
