#include "limagereader.h"
#include "lkcore/io/lresource.h"
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "lkgui/3rd/stb_image/stb_image.h"

LK_NAMESPACE_BEGIN

namespace rop
{
    // stb_image callbacks that operate on a sf::InputStream
    int read(void* user, char* data, int size)
    {
        LIOMan* stream = static_cast<LIOMan*>(user);
        return static_cast<int>(stream->readRawData((uint8_t*)data, size));
    }
    void skip(void* user, int size)
    {
        LIOMan* stream = static_cast<LIOMan*>(user);
        stream->seekg(stream->tellg() + std::streamoff(size));
    }
    int eof(void* user)
    {
        LIOMan* stream = static_cast<LIOMan*>(user);
        return stream->atReadEnd();
    }
}

LImageReader::LImageReader()
{

}

std::shared_ptr<LImage> LImageReader::loadFromIO(LIOMan& io, LImage::Format fmt)
{
    if(io.open()){
        // Setup the stb_image callbacks
        stbi_io_callbacks callbacks;
        callbacks.read = &rop::read;
        callbacks.skip = &rop::skip;
        callbacks.eof  = &rop::eof;

        // Load the image and get a pointer to the pixels in memory
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* ptr = stbi_load_from_callbacks(&callbacks, &io, &width, &height, &channels, STBI_rgb_alpha);

        if (ptr)
        {
            std::shared_ptr<LImage> image;
            if(fmt == LImage::Format::ABGR32 || fmt == LImage::Format::Invalid){
                image = std::make_shared<LImage>(ptr, width, height, LImage::Format::ABGR32, true);
            }else{
                image = std::make_shared<LImage>(ptr, width, height, LImage::Format::ABGR32, false);
                image = image->convertToFormat(fmt);
            }

            // Free the loaded pixels (they are now in our own pixel buffer)
            stbi_image_free(ptr);
            return image;
        }
        else
        {
            // Error, failed to load the image
            lWarn() << "Failed to load image from stream. Reason: " << stbi_failure_reason();
            return nullptr;
        }
    }
    return nullptr;

}

std::shared_ptr<LImage> LImageReader::loadFromFile(const std::string& fileName, LImage::Format fmt)
{
    if(fileName.starts_with(":")){
        LResource resource(fileName);
        std::vector<uint8_t> data = resource.uncompressedData();
        if(!data.empty()){
            if(fileName.ends_with(".xpm")){
                std::string s((char*)(data.data()), data.size());
                return loadXpmFromString(s, fmt);
            }else{
                return LImageReader::loadFromMemory(data.data(), data.size());
            }
        }else{
            lWarn() << "Failed to open resource: " << fileName;
            return nullptr;
        }
    }

    if(fileName.ends_with(".xpm")){
        return loadXpmFromFile(fileName, fmt);
    }else{
        return loadFileByStbi(fileName, fmt);
    }
}

std::shared_ptr<LImage> LImageReader::loadFromMemory(const void *data, size_t dataSize, LImage::Format fmt)
{
    // Check input parameters
    if (data && dataSize)
    {
        // Load the image and get a pointer to the pixels in memory
        int width = 0;
        int height = 0;
        int channels = 0;
        const unsigned char* buffer = static_cast<const unsigned char*>(data);
        unsigned char* ptr = stbi_load_from_memory(buffer, static_cast<int>(dataSize), &width, &height, &channels, STBI_rgb_alpha);

        if (ptr)
        {
            std::shared_ptr<LImage> image
                = std::make_shared<LImage>(ptr, width, height, LImage::Format::ABGR32, true);
            // Free the loaded pixels (they are now in our own pixel buffer)
            stbi_image_free(ptr);
            if(fmt != LImage::Format::ABGR32 &&  fmt != LImage::Format::Invalid){
                return image->convertToFormat(fmt);
            }
            return image;
        }
        else
        {
            // Error, failed to load the image
            lWarn() << "Failed to load image from memory. Reason: " << stbi_failure_reason();
            return nullptr;
        }
    }
    else
    {
        lWarn() << "Failed to load image from memory, no data provided";
        return nullptr;
    }
}

std::shared_ptr<LImage> LImageReader::loadFileByStbi(const std::string &fileName, LImage::Format fmt)
{
    // Load the image and get a pointer to the pixels in memory
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* ptr = stbi_load(fileName.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (ptr)
    {
        std::shared_ptr<LImage> image
            = std::make_shared<LImage>(ptr, width, height, LImage::Format::ABGR32, true);
        // Free the loaded pixels (they are now in our own pixel buffer)
        stbi_image_free(ptr);

        if(fmt != LImage::Format::ABGR32 &&  fmt != LImage::Format::Invalid){
            return image->convertToFormat(fmt);
        }
        return image;
    }
    else
    {
        // Error, failed to load the image
        lWarn() << "Failed to load image \"" << fileName << "\". Reason: " << stbi_failure_reason();
        return nullptr;
    }
}

template<class T>
std::shared_ptr<LImage> loadXpmFromStream(T& in, LImage::Format fmt){
    std::string strTrash;
    in >> strTrash;
    if(strTrash.starts_with("!")){
        in >> strTrash;
    }else{
        in.seekg(0);
    }

    int w, h, n, c;
    in >> w >> h >> n >> c;

    std::map<char, uint32_t> colors;
    std::shared_ptr<LImage> image = std::make_shared<LImage>(w, h, LImage::Format::Indexed8);
    image->setColorCount(n);

    LColor color;
    for (int i = 0; i < n; i++) { //loop to find all the colors in the image
        char key, chrTrash;
        std::string hexa;
        in >> key >> chrTrash >> chrTrash>> hexa;
        uint32_t v = 0;
        try
        {
            if(chrTrash == '#'){
                v = (uint32_t)std::stoi(hexa, 0, 16);
                color.fromUInt(v);
                if(color.m_alpha == 0){
                    color.m_alpha = 0xFF;
                }
            }else{
                color.fromUInt(0);
            }
        }
        catch (std::invalid_argument const& ex)
        {
            lWarn() << "Xpm file read error invalid argument: "<< ex.what();
            color.fromUInt(0);
        }
        catch (std::out_of_range const& ex)
        {
            lWarn() << "Xpm file read error out_of_range: "<< ex.what();
            color.fromUInt(0);
        }

        image->setColor(i, color);
        colors[key] = i;
    }

    for (int row = 0; row < h; row++) { //loop to give the right colors to the image
        for (int column = 0; column < w; column++){
            char symbol;
            in >> symbol;

            std::map<char, uint32_t>::iterator iter = colors.find(symbol);
            if(iter != colors.end()){
                image->setPixel(row, column, iter->second);
            }
        }
    }

    if(fmt != LImage::Format::Indexed8 &&  fmt != LImage::Format::Invalid){
        return image->convertToFormat(fmt);
    }
    return image;
}


std::shared_ptr<LImage> LImageReader::loadXpmFromFile(const std::string &fileName, LImage::Format fmt)
{
    std::ifstream in(fileName);
    return loadXpmFromStream<std::ifstream>(in, fmt);
}

std::shared_ptr<LImage> LImageReader::loadXpmFromString(const std::string& str, LImage::Format fmt)
{
    std::stringstream in(str);
    return loadXpmFromStream(in, fmt);
}

static uint32_t calcShift(uint32_t mask)
{
    uint32_t result = 0;
    while ((mask >= 0x100) || (!(mask & 1) && mask)) {
        result++;
        mask >>= 1;
    }
    return result;
}

static uint32_t calcScale(uint32_t lowMask)
{
    uint32_t result = 8;
    while (lowMask && result) {
        result--;
        lowMask >>= 1;
    }
    return result;
}

static inline uint32_t applyScale(uint32_t value, uint32_t scale)
{
    if (!(scale & 0x07)){ // return immediately if scale == 8 or 0
        return value;
    }

    uint32_t filled = 8 - scale;
    uint32_t result = value << scale;

    do {
        result |= result >> filled;
        filled <<= 1;
    } while (filled < 8);

    return result;
}

static void swapPixel01(std::shared_ptr<LImage> image)        // 1-bpp: swap 0 and 1 pixels
{
    lsizetype i;
    if (image->depth() == 1 && image->colorCount() == 2) {
        uint32_t *p = (uint32_t *)image->bits();
        lsizetype nbytes = static_cast<lsizetype>(image->sizeInBytes());
        lsizetype s = nbytes/4;
        for (i=0; i<s; i++) {
            *p = ~*p;
            p++;
        }
        uint8_t *p2 = (uint8_t *)p;
        s = (nbytes&3);
        for (i=0; i<s; i++) {
            *p2 = ~*p2;
            p2++;
        }
        LColor t = image->color(0);                // swap color 0 and 1
        image->setColor(0, image->color(1));
        image->setColor(1, t);
    }
}



std::shared_ptr<LImage> LImageReader::loadFromDib5(LIOMan &io, LImage::Format fmt)
{
    if(io.open()){
        if(io.isReadable()){
            io.setIsLittleEndian(true);
            int64_t startPos = io.tellg();

            pixfmt::BitmapDIV5Header bi;
            memset((char*)(&bi), 0, sizeof(bi));
            if(io.readRawData((uint8_t*)(&bi), sizeof(pixfmt::BitmapDIV5Header))>0){
                const static int BMP_RGB  = 0;                             // no compression
                const static int BMP_RLE8 = 1;                             // run-length encoded, 8 bits
                const static int BMP_RLE4 = 2;                             // run-length encoded, 4 bits
                const static int BMP_BITFIELDS = 3;                        // RGB values encoded in data as bit-fields
                const static int BMP_ALPHABITFIELDS = 4;                   // RGBA values encoded in data as bit-fields

                const static int BMP_OLD  = 12;                        // old Windows/OS2 BMP size
                const static int BMP_WIN4 = 108;                       // Windows BMP v4 size

                int nbits = bi.bV5BitCount;
                int comp = bi.bV5Compression;
                if (!(nbits == 1 || nbits == 4 || nbits == 8 || nbits == 16 || nbits == 24 || nbits == 32) ||
                    bi.bV5Planes != 1 || comp > BMP_BITFIELDS){
                    return nullptr;                                        // weird BMP image
                }
                if (!(comp == BMP_RGB || (nbits == 4 && comp == BMP_RLE4) ||
                      (nbits == 8 && comp == BMP_RLE8) || ((nbits == 16 || nbits == 32) && comp == BMP_BITFIELDS))){
                    return nullptr;                                // weird compression type
                }
                if (bi.bV5Height == INT_MIN){
                    return nullptr; // out of range for positive int
                }
                if (bi.bV5Width <= 0 || !bi.bV5Height || uint64_t(bi.bV5Width) * std::abs(bi.bV5Height) > 16384 * 16384){
                    return nullptr;
                }

                int64_t dataPos = startPos;
                int64_t ioSize = io.sizeg() - dataPos;
                if (bi.bV5SizeImage > 0 && bi.bV5SizeImage < ioSize) {
                    dataPos = ioSize - bi.bV5SizeImage;
                } else {
                    // And sometimes biSizeImage is not filled in like when pasting from Microsoft Edge, so then we just
                    // have to assume the optional color mask values are there.
                    dataPos += bi.bV5Size;
                    if (bi.bV5BitCount == 16 || bi.bV5BitCount == 32) {
                        if (bi.bV5Compression == BMP_BITFIELDS) {
                            dataPos += 12;
                        } else if (bi.bV5Compression == BMP_ALPHABITFIELDS) {
                            dataPos += 16;
                        }
                    }
                }


                int w = bi.bV5Width;
                int h = bi.bV5Height;
                int t = bi.bV5Size;
                uint32_t red_mask = 0;
                uint32_t green_mask = 0;
                uint32_t blue_mask = 0;
                uint32_t alpha_mask = 0;
                uint32_t red_shift = 0;
                uint32_t green_shift = 0;
                uint32_t blue_shift = 0;
                uint32_t alpha_shift = 0;
                uint32_t red_scale = 0;
                uint32_t green_scale = 0;
                uint32_t blue_scale = 0;
                uint32_t alpha_scale = 0;
                bool bitfields = comp == BMP_BITFIELDS || comp == BMP_ALPHABITFIELDS;

                io.seekg(startPos + bi.bV5Size); // goto start of colormap or masks

                if (bi.bV5Size >= BMP_WIN4) {
                    red_mask = bi.bV5RedMask;
                    green_mask = bi.bV5GreenMask;
                    blue_mask = bi.bV5BlueMask;
                    alpha_mask = bi.bV5AlphaMask;
                } else if (bitfields && (nbits == 16 || nbits == 32)) {
                    if (io.readRawData((uint8_t *)&red_mask, sizeof(red_mask)) != sizeof(red_mask)){
                        return nullptr;
                    }
                    if (io.readRawData((uint8_t *)&green_mask, sizeof(green_mask)) != sizeof(green_mask)){
                        return nullptr;
                    }
                    if (io.readRawData((uint8_t *)&blue_mask, sizeof(blue_mask)) != sizeof(blue_mask)){
                        return nullptr;
                    }
                    if (comp == BMP_ALPHABITFIELDS && io.readRawData((uint8_t *)&alpha_mask, sizeof(alpha_mask)) != sizeof(alpha_mask)){
                        return nullptr;
                    }
                }

                bool transp = bitfields || (comp == BMP_RGB && nbits == 32 && alpha_mask == 0xff000000);
                transp = transp && alpha_mask;

                int ncols = 0;
                int depth = 0;
                LImage::Format format;
                switch (nbits) {
                case 32:
                case 24:
                case 16:{
                    depth = 32;
                    format = transp ? LImage::Format::ARGB32 : LImage::Format::RGB32;
                    break;
                }
                case 8:
                case 4:{
                    depth = 8;
                    format = LImage::Format::Indexed8;
                    break;
                }
                case 1:{
                    depth = 1;
                    format = LImage::Format::Mono;
                    break;
                }
                default:{
                    return nullptr;
                }
                }

                if (depth != 32) {
                    ncols = bi.bV5ClrUsed ? bi.bV5ClrUsed : 1 << nbits;
                    if ((ncols < 1) || (ncols > 256)){ // sanity check - don't run out of mem if color table is broken
                        return nullptr;
                    }
                }

                if (bi.bV5Height < 0){
                    h = -h;                  // support images with negative height
                }

                std::shared_ptr<LImage> image = std::make_shared<LImage>(w, h, format);
                if (ncols > 0) {                                // read color table
                    image->setColorCount(ncols);
                    uint8_t rgb[4];
                    int   rgb_len = t == BMP_OLD ? 3 : 4;
                    for (int i=0; i<ncols; i++) {
                        if (io.readRawData((uint8_t *)rgb, rgb_len) != rgb_len){
                            return nullptr;
                        }
                        image->setColor(i, LColor(rgb[2],rgb[1],rgb[0]));
                        if (io.atReadEnd()){                        // truncated file
                            return nullptr;
                        }
                    }
                } else if (bitfields && (nbits == 16 || nbits == 32)) {
                    red_shift = calcShift(red_mask);
                    if (((red_mask >> red_shift) + 1) == 0){
                        return nullptr;
                    }
                    red_scale = calcScale(red_mask >> red_shift);
                    green_shift = calcShift(green_mask);
                    if (((green_mask >> green_shift) + 1) == 0){
                        return nullptr;
                    }
                    green_scale = calcScale(green_mask >> green_shift);
                    blue_shift = calcShift(blue_mask);
                    if (((blue_mask >> blue_shift) + 1) == 0){
                        return nullptr;
                    }
                    blue_scale = calcScale(blue_mask >> blue_shift);
                    alpha_shift = calcShift(alpha_mask);
                    if (((alpha_mask >> alpha_shift) + 1) == 0){
                        return nullptr;
                    }
                    alpha_scale = calcScale(alpha_mask >> alpha_shift);
                } else if (comp == BMP_RGB && (nbits == 24 || nbits == 32)) {
                    blue_mask = 0x000000ff;
                    green_mask = 0x0000ff00;
                    red_mask = 0x00ff0000;
                    blue_shift = 0;
                    green_shift = 8;
                    red_shift = 16;
                    blue_scale = green_scale = red_scale = 0;
                    if (transp) {
                        alpha_shift = calcShift(alpha_mask);
                        if (((alpha_mask >> alpha_shift) + 1) == 0){
                            return nullptr;
                        }
                        alpha_scale = calcScale(alpha_mask >> alpha_shift);
                    }
                } else if (comp == BMP_RGB && nbits == 16) {
                    blue_mask = 0x001f;
                    green_mask = 0x03e0;
                    red_mask = 0x7c00;
                    blue_shift = 0;
                    green_shift = 5;
                    red_shift = 10;
                    blue_scale = green_scale = red_scale = 3;
                }

                image->setDotsPerMeterX(bi.bV5XPelsPerMeter);
                image->setDotsPerMeterY(bi.bV5YPelsPerMeter);


                if (dataPos >= 0 && dataPos > (int64_t)io.tellg()) {
                    io.seekg(dataPos); // start of image data
                }

                int bpl = image->bytesPerLine();
                uint8_t *data = image->bits();

                if (nbits == 1) {                                // 1 bit BMP image
                    while (--h >= 0) {
                        if (io.readRawData((uint8_t*)(data + h*bpl), bpl) != bpl){
                            break;
                        }
                    }
                    if (ncols == 2 && LColor::toGray(image->color(0)) < LColor::toGray(image->color(1))){
                        swapPixel01(image);                // pixel 0 is white!
                    }
                }

                else if (nbits == 4) {                        // 4 bit BMP image
                    int    buflen = ((w+7)/8)*4;
                    uint8_t *buf    = new uint8_t[buflen];
                    if (comp == BMP_RLE4) {                // run length compression
                        int x=0, y=0, c, i;
                        uint8_t b;
                        uint8_t *p = data + (h-1)*bpl;
                        const uint8_t *endp = p + w;
                        while (y < h) {
                            if (!io.getByte(b)){
                                break;
                            }
                            if (b == 0) {                        // escape code
                                if (!io.getByte(b) || b == 1) {
                                    y = h;                // exit loop
                                } else switch (b) {
                                    case 0:{                        // end of line
                                        x = 0;
                                        y++;
                                        p = data + (h-y-1)*bpl;
                                        break;
                                    }
                                    case 2:{                        // delta (jump)
                                        {
                                            uint8_t tmp;
                                            io.getByte(tmp);
                                            x += tmp;
                                            io.getByte(tmp);
                                            y += tmp;
                                        }

                                        // Protection
                                        if ((uint32_t)x >= (uint32_t)w)
                                            x = w-1;
                                        if ((uint32_t)y >= (uint32_t)h)
                                            y = h-1;

                                        p = data + (h-y-1)*bpl + x;
                                        break;
                                    }
                                    default:{                // absolute mode
                                        // Protection
                                        if (p + b > endp){
                                            b = endp-p;
                                        }

                                        i = (c = b)/2;
                                        while (i--) {
                                            io.getByte(b);
                                            *p++ = b >> 4;
                                            *p++ = b & 0x0f;
                                        }
                                        if (c & 1) {
                                            unsigned char tmp;
                                            io.getByte(tmp);
                                            *p++ = tmp >> 4;
                                        }
                                        if ((((c & 3) + 1) & 2) == 2){
                                            io.readSkip(1); // align on word boundary
                                        }
                                        x += c;
                                    }
                                    }
                            } else {                        // encoded mode
                                // Protection
                                if (p + b > endp){
                                    b = endp-p;
                                }

                                i = (c = b)/2;
                                io.getByte(b);                // 2 pixels to be repeated
                                while (i--) {
                                    *p++ = b >> 4;
                                    *p++ = b & 0x0f;
                                }
                                if (c & 1){
                                    *p++ = b >> 4;
                                }
                                x += c;
                            }
                        }
                    } else if (comp == BMP_RGB) {                // no compression
                        memset(data, 0, h*bpl);
                        while (--h >= 0) {
                            if (io.readRawData((uint8_t*)buf,buflen) != buflen){
                                break;
                            }
                            uint8_t *p = data + h*bpl;
                            uint8_t *b = buf;
                            for (int i=0; i<w/2; i++) {        // convert nibbles to bytes
                                *p++ = *b >> 4;
                                *p++ = *b++ & 0x0f;
                            }
                            if (w & 1){                        // the last nibble
                                *p = *b >> 4;
                            }
                        }
                    }
                    delete [] buf;
                }

                else if (nbits == 8) {                        // 8 bit BMP image
                    if (comp == BMP_RLE8) {                // run length compression
                        int x=0, y=0;
                        uint8_t b;
                        uint8_t *p = data + (h-1)*bpl;
                        const uint8_t *endp = p + w;
                        while (y < h) {
                            if (!io.getByte(b)){
                                break;
                            }
                            if (b == 0) {                        // escape code
                                if (!io.getByte(b) || b == 1) {
                                    y = h;                // exit loop
                                } else switch (b) {
                                    case 0:{                        // end of line
                                        x = 0;
                                        y++;
                                        p = data + (h-y-1)*bpl;
                                        break;
                                    }
                                    case 2:{                        // delta (jump)
                                        {
                                            uint8_t tmp;
                                            io.getByte(tmp);
                                            x += tmp;
                                            io.getByte(tmp);
                                            y += tmp;
                                        }

                                        // Protection
                                        if ((uint32_t)x >= (uint32_t)w)
                                            x = w-1;
                                        if ((uint32_t)y >= (uint32_t)h)
                                            y = h-1;

                                        p = data + (h-y-1)*bpl + x;
                                        break;
                                    }
                                    default:{                // absolute mode
                                        // Protection
                                        if (p + b > endp){
                                            b = endp-p;
                                        }

                                        if (io.readRawData((uint8_t *)p, b) != b){
                                            return nullptr;
                                        }
                                        if ((b & 1) == 1){
                                            io.readSkip(1); // align on word boundary
                                        }
                                        x += b;
                                        p += b;
                                    }
                                    }
                            } else {                        // encoded mode
                                // Protection
                                if (p + b > endp){
                                    b = endp-p;
                                }

                                uint8_t tmp;
                                io.getByte(tmp);
                                memset(p, tmp, b); // repeat pixel
                                x += b;
                                p += b;
                            }
                        }
                    } else if (comp == BMP_RGB) {                // uncompressed
                        while (--h >= 0) {
                            if (io.readRawData((uint8_t *)data + h*bpl, bpl) != bpl){
                                break;
                            }
                        }
                    }
                }

                else if (nbits == 16 || nbits == 24 || nbits == 32) { // 16,24,32 bit BMP image
                    uint32_t *p;
                    uint32_t *end;
                    uint8_t *buf24 = new uint8_t[bpl];
                    int    bpl24 = ((w*nbits+31)/32)*4;
                    uint8_t *b;
                    int c;

                    while (--h >= 0) {
                        p = (uint32_t *)(data + h*bpl);
                        end = p + w;
                        if (io.readRawData((uint8_t *)buf24,bpl24) != bpl24){
                            break;
                        }
                        b = buf24;
                        while (p < end) {
                            c = *(uint8_t*)b | (*(uint8_t*)(b+1)<<8);
                            if (nbits > 16){
                                c |= *(uint8_t*)(b+2)<<16;
                            }
                            if (nbits > 24){
                                c |= *(uint8_t*)(b+3)<<24;
                            }
                            *p++ = LColor::argbToUInt(applyScale((c & red_mask) >> red_shift, red_scale),
                                                     applyScale((c & green_mask) >> green_shift, green_scale),
                                                     applyScale((c & blue_mask) >> blue_shift, blue_scale),
                                                     transp ? applyScale((c & alpha_mask) >> alpha_shift, alpha_scale) : 0xff);
                            b += nbits/8;
                        }
                    }
                    delete[] buf24;
                }

                if (bi.bV5Height < 0) {
                    // Flip the image
                    uint8_t *buf = new uint8_t[bpl];
                    h = -bi.bV5Height;
                    for (int y = 0; y < h/2; ++y) {
                        memcpy(buf, data + y*bpl, bpl);
                        memcpy(data + y*bpl, data + (h-y-1)*bpl, bpl);
                        memcpy(data + (h-y-1)*bpl, buf, bpl);
                    }
                    delete [] buf;
                }

                if(fmt != format && fmt != LImage::Format::Invalid){
                    return image->convertToFormat(fmt);
                }
                return image;
            }
        }
    }
    return nullptr;
}

LK_NAMESPACE_END
