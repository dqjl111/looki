#ifndef LIMAGECONVERSIONS_H
#define LIMAGECONVERSIONS_H

#include "lkgui/image/limage.h"
#include "lkgui/paint/ldrawhelper.h"
LK_NAMESPACE_BEGIN


class LKGUIDEF LImageConversions{
public:
    LImageConversions();
    static const uint8_t *lGetBitFlipArray();

    static void convertGeneric(std::shared_ptr<LImage> dst,
                               const std::shared_ptr<LImage> src,
                               LFlags<LImageConvertFlag> flags);

    static void convertGenericToRgb64(std::shared_ptr<LImage> dst,
                                      const std::shared_ptr<LImage> src,
                                      LFlags<LImageConvertFlag> flags);

    static void swapBitOrder(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);
    static void convertMonoToIndexed8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);
    static void convertMonoToX32(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);
    static void convertXToMono(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);

    static void convertIndexed8ToX32(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);
    static void convertIndexed8ToGrayscale8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);
    static void convertIndexed8ToAlpha8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);

    static void convertRGBToIndexed8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);
    static void maskAlphaConverter(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);
    static void convertARGBToIndexed8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);
    static void convertARGBToXBGR(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);
    static void convertARGBToABGR(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>);

    template<draw::PixelOrder order, bool RGBA>
    static void convertARGBToA2RGB30(std::shared_ptr<LImage> dest,
                                     const std::shared_ptr<LImage> src,
                                     LFlags<LImageConvertFlag>);

    template<bool RGBA>
    static void convertARGB32ToRGBA64(std::shared_ptr<LImage> dest,
                                      const std::shared_ptr<LImage> src,
                                      LFlags<LImageConvertFlag>);

    static void convertARGBPMToMono(std::shared_ptr<LImage> dest,
                                    const std::shared_ptr<LImage> src,
                                    LFlags<LImageConvertFlag>);

    static void convertARGBPMToIndexed8(std::shared_ptr<LImage> dest,
                                        const std::shared_ptr<LImage> src,
                                        LFlags<LImageConvertFlag>);

    template <bool rgbx>
    static void convertRGB888ToRGB(std::shared_ptr<LImage> dest,
                                   const std::shared_ptr<LImage> src,
                                   LFlags<LImageConvertFlag>);

    static void convertRGBSwapGeneric(std::shared_ptr<LImage> dest,
                                      const std::shared_ptr<LImage> src,
                                      LFlags<LImageConvertFlag>);

    static void convertRGBAToRGB(std::shared_ptr<LImage> dest,
                                 const std::shared_ptr<LImage> src,
                                 LFlags<LImageConvertFlag>);

    static void convertRGBAToARGB(std::shared_ptr<LImage> dest,
                                  const std::shared_ptr<LImage> src,
                                  LFlags<LImageConvertFlag>);

    static void convertPassthrough(std::shared_ptr<LImage> dest,
                                   const std::shared_ptr<LImage> src,
                                   LFlags<LImageConvertFlag>);

    static void maskAlphaConverterRGBx(std::shared_ptr<LImage> dest,
                                       const std::shared_ptr<LImage> src,
                                       LFlags<LImageConvertFlag>);

    template<draw::PixelOrder order, bool RGBA>
    static void convertA2RGB30PMToARGB(std::shared_ptr<LImage> dest,
                                       const std::shared_ptr<LImage> src,
                                       LFlags<LImageConvertFlag>);
    template<bool rgbswap>
    static void convertA2RGB30PMToRGB30(std::shared_ptr<LImage> dest,
                                        const std::shared_ptr<LImage> src,
                                        LFlags<LImageConvertFlag>);

    static void convertGrayScale8ToIndexed8(std::shared_ptr<LImage> dest,
                                            const std::shared_ptr<LImage> src,
                                            LFlags<LImageConvertFlag>);

    static void convertAlpha8ToIndexed8(std::shared_ptr<LImage> dest,
                                        const std::shared_ptr<LImage> src,
                                        LFlags<LImageConvertFlag>);

    static void convertRGBA64ToGray16(std::shared_ptr<LImage> dest,
                                      const std::shared_ptr<LImage> src,
                                      LFlags<LImageConvertFlag>);

    static std::vector<uint32_t>  defaultGrayTable();
    static std::vector<uint32_t>  defaultAlphaTable();

    template<bool RGBA>
    static void convertRGBA64ToARGB32(std::shared_ptr<LImage> dest,
                                      const std::shared_ptr<LImage> src,
                                      LFlags<LImageConvertFlag>);

    static void convertRGBA64ToRGBx64(std::shared_ptr<LImage> dest,
                                      const std::shared_ptr<LImage> src,
                                      LFlags<LImageConvertFlag>);

    static void convertRGBA64ToRGBA64PM(std::shared_ptr<LImage> dest,
                                        const std::shared_ptr<LImage> src,
                                        LFlags<LImageConvertFlag>);

    template<bool MaskAlpha>
    static void convertRGBA64PMToRGBA64(std::shared_ptr<LImage> dest,
                                        const std::shared_ptr<LImage> src,
                                        LFlags<LImageConvertFlag>);

    static void convertGray16ToRGBA64(std::shared_ptr<LImage> dest,
                                      const std::shared_ptr<LImage> src,
                                      LFlags<LImageConvertFlag>);
protected:
    static LImageConversions inst;
    static void initImageConversions();
    static void ditherToMono(std::shared_ptr<LImage> dest,
                              const std::shared_ptr<LImage> src,
                              LFlags<LImageConvertFlag>,
                              bool fromalpha);

    static void copy8bitPixels(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src);




};


LK_NAMESPACE_END
#endif // LIMAGESCALE_H
