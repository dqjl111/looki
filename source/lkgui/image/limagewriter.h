#ifndef LIMAGEWRITER_H
#define LIMAGEWRITER_H
#include "lkcore/io/lioman.h"
#include "lkgui/image/limage.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LImageWriter
{
public:
    LImageWriter();

    static bool write(const std::string& filename, const std::shared_ptr<LImage> image);
    static bool writeToDib5(LIOMan& io, const std::shared_ptr<LImage> image);
};

LK_NAMESPACE_END

#endif // LIMAGEWRITER_H
