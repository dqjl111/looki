#ifndef LIMAGEREADER_H
#define LIMAGEREADER_H
#include "lkcore/io/lioman.h"
#include "lkgui/image/limage.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LImageReader
{
public:
    LImageReader();

    static std::shared_ptr<LImage> loadFromIO(LIOMan& io,
                                              LImage::Format fmt = LImage::Format::Invalid);
    static std::shared_ptr<LImage> loadFromFile(const std::string& fileName,
                                                LImage::Format fmt = LImage::Format::Invalid);
    static std::shared_ptr<LImage> loadFromMemory(const void* data,
                                                  std::size_t dataSize,
                                                  LImage::Format fmt = LImage::Format::Invalid);

    static std::shared_ptr<LImage> loadFileByStbi(const std::string& fileName,
                                                  LImage::Format fmt = LImage::Format::Invalid);

    static std::shared_ptr<LImage> loadXpmFromFile(const std::string& fileName,
                                               LImage::Format fmt = LImage::Format::Invalid);

    static std::shared_ptr<LImage> loadXpmFromString(const std::string& str,
                                                     LImage::Format fmt = LImage::Format::Invalid);

    template<size_t N>
    static std::shared_ptr<LImage> loadXpmFromArray(const char * const (&xpm)[N],
                                                    LImage::Format fmt = LImage::Format::Invalid){
        std::ostringstream ss;
        for(int i=0; i< N; ++i){
            ss << xpm[i] << '\n';
        }
        return loadXpmFromString(ss.str(), fmt);
    }

    static std::shared_ptr<LImage> loadFromDib5(LIOMan& io,
                                                LImage::Format fmt = LImage::Format::Invalid);
};


LK_NAMESPACE_END
#endif // LIMAGEREADER_H
