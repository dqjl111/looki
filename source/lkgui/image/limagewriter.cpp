#include "limagewriter.h"
#include "lkcore/io/liofileman.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lkgui/3rd/stb_image/stb_image_write.h"
#include "lkcore/lstringutil.h"

LK_NAMESPACE_BEGIN

LImageWriter::LImageWriter()
{
}

bool LImageWriter::write(const std::string& filename, const std::shared_ptr<LImage> image)
{
    if(image == nullptr){
        return false;
    }

    std::shared_ptr<LImage> saveImage = nullptr;
    if(image->format() == LImage::Format::ABGR32){
        saveImage = image;
    }else{
        saveImage = image->convertToFormat(LImage::Format::ABGR32);
    }

    // Make sure the image is not empty
    if (saveImage->bits() != nullptr
        &&saveImage->sizeInBytes() >0
        && saveImage->width() > 0
        && saveImage->height()>0)
    {
        // Deduce the image type from its extension

        // Extract the extension
        const std::size_t dot = filename.find_last_of('.');
        const std::string extension = dot != std::string::npos ? LStringUtil::toLower(filename.substr(dot + 1)) : "";
        const LSize<int> convertedSize = saveImage->size();

        if (extension == "bmp")
        {
            // BMP format
            if (stbi_write_bmp(filename.c_str(), convertedSize.m_x, convertedSize.m_y, 4, saveImage->bits())){
                return true;
            }
        }
        else if (extension == "tga")
        {
            // TGA format
            if (stbi_write_tga(filename.c_str(), convertedSize.m_x, convertedSize.m_y, 4, saveImage->bits())){
                return true;
            }
        }
        else if (extension == "png")
        {
            // PNG format
            if (stbi_write_png(filename.c_str(), convertedSize.m_x, convertedSize.m_y, 4, saveImage->bits(), 0)){
                return true;
            }
        }
        else if (extension == "jpg" || extension == "jpeg")
        {
            // JPG format
            if (stbi_write_jpg(filename.c_str(), convertedSize.m_x, convertedSize.m_y, 4, saveImage->bits(), 90)){
                return true;
            }
        }
    }

    lWarn() << "Failed to save image \"" << filename;
    return false;
}

bool LImageWriter::writeToDib5(LIOMan &io, const std::shared_ptr<LImage> image)
{
    if(image == nullptr){
        return false;
    }

    if (image->sizeInBytes() <=0
        || image->width() <= 0
        || image->height()<=0)
    {
        return false;
    }

    if(io.open()){
        if(io.isWritable()){
            // Make sure the image is not empty
            io.setIsLittleEndian(true);

            //depth will be always 32
            lsizetype bplBmp = lsizetype(image->width()) * 4;
            lsizetype size = bplBmp * image->height();
            if (lsizetype(uint32_t(size)) != size){
                return false;
            }

            pixfmt::BitmapDIV5Header bi;
            memset((char*)(&bi), 0, sizeof(bi));


            bi.bV5Size          = sizeof(pixfmt::BitmapDIV5Header);
            bi.bV5Width         = image->width();
            bi.bV5Height        = image->height();
            bi.bV5Planes        = 1;
            bi.bV5BitCount      = 32;
            bi.bV5Compression   = 3; // RGB values encoded in data as bit-fields
            bi.bV5SizeImage     = uint32_t(bplBmp * image->height());
            bi.bV5XPelsPerMeter = 0;
            bi.bV5YPelsPerMeter = 0;
            bi.bV5ClrUsed       = 0;
            bi.bV5ClrImportant  = 0;
            bi.bV5BlueMask      = 0x000000ff;
            bi.bV5GreenMask     = 0x0000ff00;
            bi.bV5RedMask       = 0x00ff0000;
            bi.bV5AlphaMask     = 0xff000000;
            bi.bV5CSType        = 0x73524742;         //LCS_sRGB
            bi.bV5Intent        = 0x00000004L;    //LCS_GM_IMAGES

            if(io.writeRawData(reinterpret_cast<const uint8_t*>(&bi), bi.bV5Size) ==false){
                return false;
            }

            if(io.writeRawData(reinterpret_cast<const uint8_t*>(&bi.bV5RedMask), sizeof(bi.bV5RedMask)) ==false){
                return false;
            }

            if(io.writeRawData(reinterpret_cast<const uint8_t*>(&bi.bV5GreenMask), sizeof(bi.bV5GreenMask)) ==false){
                return false;
            }


            if(io.writeRawData(reinterpret_cast<const uint8_t*>(&bi.bV5BlueMask), sizeof(bi.bV5BlueMask)) ==false){
                return false;
            }


            std::shared_ptr<LImage> saveImage = nullptr;
            if(image->format() == LImage::Format::ARGB32){
                saveImage = image;
            }else{
                saveImage = image->convertToFormat(LImage::Format::ARGB32);
            }

            std::vector<uint8_t> buf(bplBmp);
            memset(buf.data(), 0, size_t(bplBmp));
            for (int y=image->height()-1; y>=0; y--) {
                // write the image bits
                const uint32_t *p = (uint32_t*)saveImage->scanLine(y);
                const uint32_t *end = p + image->width();
                uint8_t *b = buf.data();
                while (p < end) {
                    uint32_t alpha = LColor::uintToAlpha(*p);
                    if (alpha) {
                        *b++ = uint8_t(LColor::uintToBlue(*p));
                        *b++ = uint8_t(LColor::uintToGreen(*p));
                        *b++ = uint8_t(LColor::uintToRed(*p));
                    } else {
                        //white for fully transparent pixels.
                        *b++ = 0xff;
                        *b++ = 0xff;
                        *b++ = 0xff;
                    }
                    *b++ = uint8_t(alpha);
                    p++;
                }
                if(io.writeRawData(buf.data(), bplBmp) == false){
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}


LK_NAMESPACE_END
