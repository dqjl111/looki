#include "lgradient.h"
#include "lkgui/paint/lpainter.h"

LK_NAMESPACE_BEGIN

LGradient::LGradient()
{
    memset((char*)(m_data), 0, sizeof(LRadialGradientData));
}

std::shared_ptr<LImage> LGradient::toImage(const LSize<int> s) const
{
    if(m_type == Type::No){
        return nullptr;
    }

    std::shared_ptr<LImage> image = std::make_shared<LImage>(GRADIENT_STOPTABLE_SIZE, GRADIENT_STOPTABLE_SIZE, LImage::Format::ABGR32);
    LPainter p(image);
    std::shared_ptr<LGradient> g = getPtrT<LGradient>();
    std::shared_ptr<LBrush> b= std::make_shared<LBrush>(g);
    p.fillRect(LRect<double>(0, 0, s.m_x, s.m_y, false), b);
    return image;
}

void LGradient::ensureColorTable()
{
    if(m_dirty || m_colorTable == nullptr){
        m_dirty = false;
        if(m_colorTable == nullptr){
            m_colorTable = std::make_shared<uint32_t[GRADIENT_STOPTABLE_SIZE]>();
        }
        generateColorTable();
    }
}

LGradient &LGradient::operator=(const LGradient &b)
{
    m_flags = b.m_flags;
    m_stops = b.m_stops;
    m_dirty = true;
    memcpy((char*)(m_data), (char*)(b.m_data), sizeof(LRadialGradientData));
    return *this;
}

bool LGradient::operator==(const LGradient &b) const
{
    if(m_flags == b.m_flags){
        if(m_stops.size() == b.m_stops.size()){
            LGradientStops::const_iterator iter1 = m_stops.cbegin();
            LGradientStops::const_iterator iter2 = b.m_stops.cbegin();
            for(; iter1!= m_stops.cend(); ++iter1, ++iter2){
                if(iter1->first != iter2->first){
                    return false;
                }
                if(iter1->second != iter2->second){
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

LSize<int> LGradient::suggestSize()
{
    switch(m_type){
    case Type::Linear:{
        int w = std::lround(std::max<double>(m_linear.origin.x, m_linear.end.x));
        int h = std::lround(std::max<double>(m_linear.origin.y, m_linear.end.y));
        return LSize<int>(w, h);
    }
    case Type::Radial:{
        int w = std::lround(std::max<double>(m_radial.center.x+m_radial.center.radius,
                                             m_radial.focal.x+m_radial.focal.radius));
        int h = std::lround(std::max<double>(m_radial.center.y+m_radial.center.radius,
                                             m_radial.focal.y+m_radial.focal.radius));
        return LSize<int>(w, h);
        break;
    }
    case Type::Conical:{
        return LSize<int>(GRADIENT_STOPTABLE_SIZE*std::numbers::inv_pi, GRADIENT_STOPTABLE_SIZE*std::numbers::inv_pi);
    }
    default:{
        return LSize<int>();
    }
    }

}

void LGradient::setColorAt(double pos, const LColor &color)
{
    if ((pos > 1 || pos < 0) && !std::isnan(pos)) {
        lWarn() << "LGradient::setColorAt: Color position must be specified in the range 0 to 1";
        return;
    }

    LGradientStops::iterator iter = m_stops.begin();
    if (!std::isnan(pos)){
        while(iter != m_stops.end()){
            if(iter->first >= pos){
                break;
            }
            ++iter;
        }
    }

    if(iter != m_stops.end()){
        if(iter->first == pos){
            iter->second = color;
        }else{
            m_stops.insert(iter, LGradientStop(pos, color));
        }
    }else{
        m_stops.insert(iter, LGradientStop(pos, color));
    }
}

LPoint<double> LGradient::radialAdaptFocalPoint(const LPoint<double> &center, double radius, const LPoint<double> &focalPoint)
{
    const double compensatedRadius = radius - radius * double(0.001);
    LLine<double> line(center, focalPoint);
    if (line.length() > (compensatedRadius)){
        line.setLength(compensatedRadius);
    }
    return line.m_pt2;
}

std::string LGradient::toCss()
{
    std::stringstream s;
    switch(m_type){
    case Type::Linear:{
        break;
    }
    case Type::Radial:{
        break;
    }
    case Type::Conical:{
        break;
    }
    }
    return s.str();
}


uint64_t LGradient::stopColorValue(LColor c, bool needPremultiply){
    if(!m_hasAlpha){
        c.m_alpha = uint8_t(255);
    }
    uint64_t ret = c.toUInt64();
    if (needPremultiply) {
        ret = LColor::premultiply(ret);
    }
    return ret;
}


static inline uint64_t interpolate256(uint64_t x, uint32_t alpha1, uint64_t y, uint32_t alpha2)
{
    return LColor::argb16ToUInt64((LColor::red16(x)*alpha1 + LColor::red16(y)*alpha2)>>8,
                                  (LColor::green16(x)*alpha1 + LColor::green16(y)*alpha2)>>8,
                                  (LColor::blue16(x)*alpha1 + LColor::blue16(y)*alpha2)>>8,
                                  (LColor::alpha16(x)*alpha1 + LColor::alpha16(y)*alpha2)>>8);
}


void LGradient::generateColorTable()
{
    int stopCount = (int)m_stops.size();
    if(stopCount == 0){
        return;
    }

    bool colorInterpolation = (m_interpolation == Interpolation::Color);

    if (stopCount == 2) {
        LGradientStops::const_iterator stop0, stop1 = m_stops.cbegin();
        stop0 = stop1 = m_stops.cbegin();
        stop1++;

        uint64_t firstColor = stopColorValue(stop0->second, colorInterpolation);
        uint64_t secondColor = stopColorValue(stop1->second, colorInterpolation);
        double firstStop = stop0->first;
        double secondStop = stop1->first;

        if (secondStop < firstStop) {
            uint64_t tmp = firstColor;
            firstColor = secondColor;
            secondColor = tmp;
            std::swap(firstStop, secondStop);
        }

        int firstIndex = std::lround(firstStop * (GRADIENT_STOPTABLE_SIZE-1));
        int secondIndex = std::lround(secondStop * (GRADIENT_STOPTABLE_SIZE-1));

        uint32_t redFirst = (uint32_t)LColor::red16(firstColor) << 16;
        uint32_t greenFirst = (uint32_t)LColor::green16(firstColor) << 16;
        uint32_t blueFirst = (uint32_t)LColor::blue16(firstColor) << 16;
        uint32_t alphaFirst = (uint32_t)LColor::alpha16(firstColor) << 16;

        uint32_t redSecond = (uint32_t)LColor::red16(secondColor) << 16;
        uint32_t greenSecond = (uint32_t)LColor::green16(secondColor) << 16;
        uint32_t blueSecond = (uint32_t)LColor::blue16(secondColor) << 16;
        uint32_t alphaSecond = (uint32_t)LColor::alpha16(secondColor) << 16;

        int i = 0;
        int stage  = std::min<int>(GRADIENT_STOPTABLE_SIZE, firstIndex);
        uint32_t tValue;
        if(colorInterpolation){
            tValue = LColor::argb64ToArgb(firstColor);
        }else{
            tValue = LColor::argb64ToArgb(LColor::premultiply(firstColor));
        }
        for (; i <= stage; ++i) {
            m_colorTable[i] = tValue;
        }

        if (i < secondIndex) {
            double reciprocal = double(1) / (secondIndex - firstIndex);

            int redDelta = std::lround((double(redSecond) - redFirst) * reciprocal);
            int greenDelta = std::lround((double(greenSecond) - greenFirst) * reciprocal);
            int blueDelta = std::lround((double(blueSecond) - blueFirst) * reciprocal);
            int alphaDelta = std::lround((double(alphaSecond) - alphaFirst) * reciprocal);

            // rounding
            redFirst += 1 << 15;
            greenFirst += 1 << 15;
            blueFirst += 1 << 15;
            alphaFirst += 1 << 15;

            stage = std::min<int>(GRADIENT_STOPTABLE_SIZE, secondIndex);
            for (; i < stage; ++i) {
                redFirst += redDelta;
                greenFirst += greenDelta;
                blueFirst += blueDelta;
                alphaFirst += alphaDelta;

                const uint64_t color = LColor::argb16ToUInt64(redFirst >> 16, greenFirst >> 16, blueFirst >> 16, alphaFirst >> 16);
                if (m_interpolation == Interpolation::Color){
                    m_colorTable[i] = LColor::argb64ToArgb(color);
                }else{
                    m_colorTable[i] = LColor::argb64ToArgb(LColor::premultiply(color));
                }
            }
        }

        if(colorInterpolation){
            tValue = LColor::argb64ToArgb(secondColor);
        }else{
            tValue = LColor::argb64ToArgb(LColor::premultiply(secondColor));
        }
        for (; i < GRADIENT_STOPTABLE_SIZE; ++i) {
            m_colorTable[i] = tValue;
        }

        return;
    }


    if (stopCount == 1) {
        LGradientStops::const_iterator stop0 = m_stops.cbegin();
        uint32_t color = stop0->second.toUInt();
        for (int i = 0; i < GRADIENT_STOPTABLE_SIZE; ++i){
            m_colorTable[i] = color;
        }
        return;
    }
    {
        LGradientStops::const_iterator stopI = m_stops.cbegin();
        uint64_t currentColor = stopColorValue(stopI->second, true);
        // The position where the gradient begins and ends
        double beginPos = stopI->first;
        LGradientStops::const_reverse_iterator endStop =  m_stops.crbegin();
        double endPos = endStop->first;

        int pos = 0; // The position in the color table.
        uint64_t nextColor, tempColor;

        double incr = 1 / double(GRADIENT_STOPTABLE_SIZE); // the double increment.
        double dpos = 1.5 * incr; // current position in gradient stop list (0 to 1)

        // Up to first point
        uint32_t tValue = LColor::argb64ToArgb(currentColor);
        m_colorTable[pos++] = tValue;
        for (; dpos <= beginPos && pos < GRADIENT_STOPTABLE_SIZE; ++pos) {
            m_colorTable[pos] = tValue;
            dpos += incr;
        }

        double t; // position between current left and right stops
        double tDelta; // the t increment per entry in the color table

        if (dpos < endPos) {
            LGradientStops::const_iterator stopNext = stopI;
            stopNext++;

            // Gradient area
            while (dpos > stopNext->first){
                ++stopNext;

                if(stopNext == m_stops.cend()){
                    --stopNext;
                    break;
                }else{
                    ++stopI;
                }
            }

            if(stopNext != m_stops.cend()){
                if (stopI != m_stops.cbegin()){
                    currentColor = stopColorValue(stopI->second, colorInterpolation);
                }
                nextColor = stopColorValue(stopNext->second, colorInterpolation);


                double diff = stopNext->first - stopI->first;
                double c = (diff == 0) ? double(0) : 256 / diff;
                t = (dpos - stopI->first) * c;
                tDelta = incr * c;

                while (pos < GRADIENT_STOPTABLE_SIZE) {
                    int dist = std::lround(t);
                    int idist = 256 - dist;

                    if (m_interpolation == Interpolation::Color){
                        tempColor = interpolate256(currentColor, idist, nextColor, dist);
                    }else{
                        tempColor = LColor::premultiply(interpolate256(currentColor, idist, nextColor, dist));
                    }
                    m_colorTable[pos] = LColor::argb64ToArgb(tempColor);

                    ++pos;
                    dpos += incr;

                    if (dpos >= endPos){
                        break;
                    }

                    t += tDelta;
                    int skip = 0;
                    while (dpos > stopNext->first){
                        ++stopNext;
                        if(stopNext == m_stops.cend()){
                            --stopNext;
                            break;
                        }else{
                            skip++;
                        }
                    }

                    if (skip != 0) {
                        stopI = stopNext;
                        stopI--;
                        if (skip == 1){
                            currentColor = nextColor;
                        }else{
                            currentColor = stopColorValue(stopI->second, (skip != 1) && colorInterpolation);
                        }
                        nextColor = stopColorValue(stopNext->second, colorInterpolation);

                        double diff = stopNext->first - stopI->first;
                        double c = (diff == 0) ? double(0) : 256 / diff;
                        t = (dpos - stopI->first) * c;
                        tDelta = incr * c;
                    }
                }
            }
        }

        currentColor = stopColorValue(endStop->second, true);
        tValue = LColor::argb64ToArgb(currentColor);
        while (pos < GRADIENT_STOPTABLE_SIZE) {
            m_colorTable[pos] = tValue;
            ++pos;
        }
    }
}


LK_NAMESPACE_END
