#include "llayoutitem.h"
#include "lkwidgets/core/lwidget.h"
#include "lkgui/lguiapplication.h"

LK_NAMESPACE_BEGIN
LLayoutItem::LLayoutItem(LFlags<LAlignment> alignment) : m_alignment(alignment)
{

}

LLayoutItem::~LLayoutItem()
{

}

bool LLayoutItem::hasHeightForWidth() const
{
    return false;
}

int LLayoutItem::heightForWidth(int w) const
{
    return -1;
}

int LLayoutItem::minimumHeightForWidth(int w) const
{
    return heightForWidth(w);
}

void LLayoutItem::invalidate()
{

}

void LLayoutItem::raise()
{

}

void LLayoutItem::lower()
{

}

void LLayoutItem::show()
{

}

void LLayoutItem::hide()
{

}

void LLayoutItem::clearFocus()
{

}

bool LLayoutItem::setFocus()
{
    return false;
}

bool LLayoutItem::containWidget(std::shared_ptr<LWidget>)
{
    return false;
}

std::shared_ptr<LWidget> LLayoutItem::widget() const
{
    return nullptr;
}

std::shared_ptr<LLayout> LLayoutItem::layout()
{
    return nullptr;
}

std::shared_ptr<LSpacerItem> LLayoutItem::spacerItem()
{
    return nullptr;
}

uint32_t LLayoutItem::controlTypes() const
{
    return LSizePolicy::DefaultType;
}

LSize<int> LLayoutItem::smartMinSize(const LSize<int> &sizeHint,
                                     const LSize<int> &minSizeHint,
                                     const LSize<int> &minSize, const
                                     LSize<int> &maxSize,
                                     const LSizePolicy &sizePolicy)
{
    LSize<int> s;

    if (sizePolicy.m_horPolicy != LSizePolicy::Ignored) {
        if (sizePolicy.m_horPolicy & LSizePolicy::ShrinkFlag){
            s.setWidth(minSizeHint.width());
        }
        else{
            s.setWidth(std::max(sizeHint.width(), minSizeHint.width()));
        }
    }

    if (sizePolicy.m_verPolicy != LSizePolicy::Ignored) {
        if (sizePolicy.m_verPolicy & LSizePolicy::ShrinkFlag) {
            s.setHeight(minSizeHint.height());
        } else {
            s.setHeight(std::max(sizeHint.height(), minSizeHint.height()));
        }
    }

    s = s.boundedTo(maxSize);
    if (minSize.width() > 0){
        s.setWidth(minSize.width());
    }
    if (minSize.height() > 0){
        s.setHeight(minSize.height());
    }
    return s.expandedTo(LSize<int>(0,0));
}

LSize<int> LLayoutItem::smartMaxSize(const LSize<int> &sizeHint,
                                     const LSize<int> &minSize,
                                     const LSize<int> &maxSize,
                                     const LSizePolicy &sizePolicy,
                                     LFlags<LAlignment> align)
{
    if ((align & LAlignment::HorizontalMask) && (align & LAlignment::VerticalMask)){
        return LSize<int>(LLAYOUTSIZE_MAX, LLAYOUTSIZE_MAX);
    }
    LSize<int> s = maxSize;
    LSize<int> hint = sizeHint.expandedTo(minSize);
    if (s.width() == LWIDGETSIZE_MAX && !(align & LAlignment::HorizontalMask)){
        if (!(sizePolicy.m_horPolicy & LSizePolicy::GrowFlag)){
            s.setWidth(hint.width());
        }
    }

    if (s.height() == LWIDGETSIZE_MAX && !(align & LAlignment::VerticalMask)){
        if (!(sizePolicy.m_verPolicy & LSizePolicy::GrowFlag)){
            s.setHeight(hint.height());
        }
    }

    if (align & LAlignment::HorizontalMask){
        s.setWidth(LWIDGETSIZE_MAX);
    }
    if (align & LAlignment::VerticalMask){
        s.setHeight(LWIDGETSIZE_MAX);
    }
    return s;
}

LSize<int> LLayoutItem::smartMinSize(const std::shared_ptr<LWidget> w)
{
    return smartMinSize(w->sizeHint(), w->minimumSizeHint(),
                        w->minimumSize(), w->maximumSize(),
                        w->sizePolicy());
}

LSize<int> LLayoutItem::smartMaxSize(const std::shared_ptr<LWidget> w, LFlags<LAlignment> align)
{
    return smartMaxSize(w->sizeHint().expandedTo(w->minimumSizeHint()), w->minimumSize(), w->maximumSize(),
                        w->sizePolicy(), align);
}

LSpacerItem::~LSpacerItem()
{

}

void LSpacerItem::changeSize(int w, int h, LSizePolicy::Policy hPolicy, LSizePolicy::Policy vPolicy)
{
    m_width = w;
    m_height = h;
    m_sizeP = LSizePolicy(hPolicy, vPolicy);
}

LSize<int> LSpacerItem::sizeHint() const
{
    return LSize<int> (m_width, m_height);
}

LSize<int> LSpacerItem::minimumSize() const
{
    return LSize<int>(m_sizeP.m_horPolicy & LSizePolicy::ShrinkFlag ? 0 : m_width,
                 m_sizeP.m_verPolicy & LSizePolicy::ShrinkFlag ? 0 : m_height);
}

LSize<int> LSpacerItem::maximumSize() const
{
    return LSize<int>(m_sizeP.m_horPolicy & LSizePolicy::GrowFlag ? LLAYOUTSIZE_MAX : m_width,
                 m_sizeP.m_verPolicy & LSizePolicy::GrowFlag ? LLAYOUTSIZE_MAX : m_height);
}

LFlags<LOrientation> LSpacerItem::expandingDirections() const
{
    return m_sizeP.expandingDirections();
}

bool LSpacerItem::isEmpty() const
{
    return true;
}

void LSpacerItem::setGeometry(const LRect<int> &r)
{
    m_rect = r;
}

LRect<int> LSpacerItem::geometry() const
{
    return m_rect;
}

std::shared_ptr<LSpacerItem> LSpacerItem::spacerItem()
{
    return getPtrT<LSpacerItem>();
}


LWidgetItem::~LWidgetItem()
{

}

LSize<int> LWidgetItem::sizeHint() const
{
    LSize<int> s;
    if (!isEmpty()) {
        std::shared_ptr<LWidget> w= m_wid.lock();
        if(w != nullptr){
            s = w->sizeHint().expandedTo(w->minimumSizeHint());
            s = s.boundedTo(w->maximumSize()).expandedTo(w->minimumSize());

            if (w->sizePolicy().m_horPolicy == LSizePolicy::Ignored){
                s.setWidth(0);
            }
            if (w->sizePolicy().m_verPolicy == LSizePolicy::Ignored){
                s.setHeight(0);
            }
        }
    }
    return s;

}

LSize<int> LWidgetItem::minimumSize() const
{
    LSize<int> s;
    if (!isEmpty()){
        std::shared_ptr<LWidget> w= m_wid.lock();
        if(w != nullptr){
            s = smartMinSize(w);
        }
    }
    return s;


}

LSize<int> LWidgetItem::maximumSize() const
{
    LSize<int> s;
    if (!isEmpty()){
        std::shared_ptr<LWidget> w= m_wid.lock();
        if(w != nullptr){
            s = smartMaxSize(w, m_alignment);
        }
    }
    return s;
}

LFlags<LOrientation> LWidgetItem::expandingDirections() const
{
    LFlags<LOrientation> e;

    if (!isEmpty()){
        std::shared_ptr<LWidget> w= m_wid.lock();
        if(w != nullptr){
            e = w->sizePolicy().expandingDirections();
            /*
              If the layout is expanding, we make the widget expanding, even if
              its own size policy isn't expanding.
            */
            std::shared_ptr<LLayout> layout = w->layout();
            if (layout != nullptr) {
                if (w->sizePolicy().m_horPolicy & LSizePolicy::GrowFlag
                    && (layout->expandingDirections() & LOrientation::Horizontal)){
                    e |= LOrientation::Horizontal;
                }
                if (w->sizePolicy().m_verPolicy & LSizePolicy::GrowFlag
                    && (layout->expandingDirections() & LOrientation::Vertical)){
                    e |= LOrientation::Vertical;
                }
            }

            if (m_alignment & LAlignment::HorizontalMask){
                e &= ~uint8_t(LOrientation::Horizontal);
            }
            if (m_alignment & LAlignment::VerticalMask){
                e &= ~uint8_t(LOrientation::Vertical);
            }
        }
    }
    return e;

}

bool LWidgetItem::isEmpty() const
{
    std::shared_ptr<LWidget> w= m_wid.lock();
    if(w == nullptr){
        return true;
    }
    return (w->isHidden() && !w->sizePolicy().m_retainSizeWhenHidden) || w->isWindow();
}

void LWidgetItem::setGeometry(const LRect<int> &rect)
{
    if (!isEmpty()){
        std::shared_ptr<LWidget> w= m_wid.lock();
        if(w != nullptr){
            LSize<int> s = rect.size().boundedTo(maximumSize());
            int x = rect.m_x;
            int y = rect.m_y;
            if (m_alignment & (LAlignment::HorizontalMask | LAlignment::VerticalMask)) {
                LSize<int> pref(sizeHint());
                LSizePolicy sp = w->sizePolicy();
                if (sp.m_horPolicy == LSizePolicy::Ignored){
                    pref.setWidth(w->sizeHint().expandedTo(w->minimumSize()).width());
                }
                if (sp.m_verPolicy == LSizePolicy::Ignored){
                    pref.setHeight(w->sizeHint().expandedTo(w->minimumSize()).height());
                }
                if (m_alignment & LAlignment::HorizontalMask){
                    s.setWidth(std::min(s.width(), pref.width()));
                }
                if (m_alignment & LAlignment::VerticalMask) {
                    if (hasHeightForWidth()){
                        s.setHeight(std::min(s.height(), heightForWidth(s.width())));
                    }
                    else{
                        s.setHeight(std::min(s.height(), pref.height()));
                    }
                }
            }
            LFlags<LAlignment> alignHoriz = LGuiApplication::visualAlignment(w->layoutDirection(), m_alignment);
            if (alignHoriz & LAlignment::Right){
                x = x + (rect.width() - s.width());
            }
            else if (!(alignHoriz & LAlignment::Left)){
                x = x + (rect.width() - s.width()) / 2;
            }

            if (m_alignment & LAlignment::Bottom){
                y = y + (rect.height() - s.height());
            }
            else if (!(m_alignment & LAlignment::Top)){
                y = y + (rect.height() - s.height()) / 2;
            }

            if (x < 0) {
                s.rwidth() += x;
                x = 0;
            }
            if (y < 0) {
                s.rheight() += y;
                y = 0;
            }

            w->setGeometry(x, y, s.width(), s.height());

            //lWarn() << "set geo " << i << ":" << x << ',' << y << ',' <<  w << ',' << h;
        }
    }
}

LRect<int> LWidgetItem::geometry() const
{
    if (!isEmpty()){
        std::shared_ptr<LWidget> w= m_wid.lock();
        if(w != nullptr){
            return w->geometry();
        }
    }
    return LRect<int>();
}

std::shared_ptr<LWidget> LWidgetItem::widget() const
{
    return m_wid.lock();
}

bool LWidgetItem::hasHeightForWidth() const
{
    if(!isEmpty()){
        std::shared_ptr<LWidget> w= m_wid.lock();
        if(w != nullptr){
            return w->hasHeightForWidth();
        }
    }
    return false;
}

int LWidgetItem::heightForWidth(int width) const
{
    if(!isEmpty()){
        std::shared_ptr<LWidget> w= m_wid.lock();
        if(w != nullptr){
            int hfw = w->heightForWidth(width);

            if (hfw > w->maximumHeight()){
                hfw = w->maximumHeight();
            }
            if (hfw < w->minimumHeight()){
                hfw = w->minimumHeight();
            }

            if (hfw < 0){
                hfw = 0;
            }
            return hfw;
        }
    }
    return -1;
}

uint32_t LWidgetItem::controlTypes() const
{
    std::shared_ptr<LWidget> w= m_wid.lock();
    if(w == nullptr){
        return w->sizePolicy().controlType();
    }
    return 0;
}

void LWidgetItem::raise()
{
    std::shared_ptr<LWidget> w= m_wid.lock();
    if(w != nullptr){
        w->raise();
    }
}

void LWidgetItem::lower()
{
    std::shared_ptr<LWidget> w= m_wid.lock();
    if(w != nullptr){
        w->lower();
    }
}

void LWidgetItem::show()
{
    std::shared_ptr<LWidget> w= m_wid.lock();
    if(w != nullptr){
        w->setVisible(true);
    }
}

void LWidgetItem::hide()
{
    std::shared_ptr<LWidget> w= m_wid.lock();
    if(w != nullptr){
        w->setVisible(false);
    }
}

void LWidgetItem::clearFocus()
{
    std::shared_ptr<LWidget> w= m_wid.lock();
    if(w != nullptr){
        w->clearFocus();
    }
}

bool LWidgetItem::containWidget(std::shared_ptr<LWidget> child)
{
    std::shared_ptr<LWidget> w= m_wid.lock();
    if(w != nullptr){
        return w->isAncestorOf(child);
    }
    return false;
}


bool LWidgetItem::setFocus()
{
    std::shared_ptr<LWidget> w= m_wid.lock();
    if(w != nullptr){
        std::shared_ptr<LWidget> f=  w->focusWidget();
        if(f != nullptr){
            f->setFocus();
            return true;
        }

        w->setFocus();
        return true;
    }
    return false;
}

int LLayoutItemData::hStretch(std::shared_ptr<LLayoutItem> item) {
    if(stretch == 0){
        if(item != nullptr){
            std::shared_ptr<LWidget> w = item->widget();
            if(w != nullptr){
                return w->sizePolicy().m_horStretch;
            }
        }
    }
    return stretch;
}
int LLayoutItemData::vStretch(std::shared_ptr<LLayoutItem> item) {
    if(stretch == 0){
        if(item != nullptr){
            std::shared_ptr<LWidget> w = item->widget();
            if(w != nullptr){
                return w->sizePolicy().m_verStretch;
            }
        }
    }
    return stretch;
}

void LLayoutItemData::copy(std::shared_ptr<LLayoutItemData> d)
{
    if(d!= nullptr && this != d.get()){
        stretch = d->stretch;
        sizeHint = d->sizeHint;
        maximumSize =d->maximumSize;
        minimumSize = d->minimumSize;
        spacing = d->spacing;
        expansive = d->expansive;
        empty = d->empty;

        done = d->done;
        size = d->size;
        pos = d->pos;
    }
}

void LLayoutItemData::geomCalc(std::vector<std::shared_ptr<LLayoutItemData>>& chain,
                               int start, int count, int pos, int space, int spacer)
{
    int cHint = 0;
    int cMin = 0;
    int cMax = 0;
    int sumStretch = 0;
    int sumSpacing = 0;
    int expandingCount = 0;

    bool allEmptyNonstretch = true;
    int pendingSpacing = -1;
    int spacerCount = 0;
    int i;

    for (i = start; i < start + count; i++) {
        std::shared_ptr<LLayoutItemData> data = chain[i];

        data->done = false;
        cHint += data->smartSizeHint();
        cMin += data->minimumSize;
        cMax += data->maximumSize;
        sumStretch += data->stretch;
        if (!data->empty) {
            /*
                Using pendingSpacing, we ensure that the spacing for the last
                (non-empty) item is ignored.
            */
            if (pendingSpacing >= 0) {
                sumSpacing += pendingSpacing;
                ++spacerCount;
            }
            pendingSpacing = data->effectiveSpacer(spacer);
        }
        if (data->expansive){
            expandingCount++;
        }
        allEmptyNonstretch = allEmptyNonstretch && data->empty && !data->expansive && data->stretch <= 0;
    }

    int extraspace = 0;

    if (space < cMin + sumSpacing) {
        /*
          Less space than minimumSize; take from the biggest first
        */

        int minSize = cMin + sumSpacing;

        // shrink the spacers proportionally
        if (spacer >= 0) {
            spacer = minSize > 0 ? spacer * space / minSize : 0;
            sumSpacing = spacer * spacerCount;
        }

        std::vector<int> minimumSizes;
        minimumSizes.reserve(count);

        for (i = start; i < start + count; i++){
            minimumSizes.push_back(chain.at(i)->minimumSize);
        }
        sort(minimumSizes.begin(), minimumSizes.end());

        int spaceLeft = space - sumSpacing;

        int sum = 0;
        int idx = 0;
        int space_used=0;
        int current = 0;
        while (idx < count && space_used < spaceLeft) {
            current = minimumSizes.at(idx);
            space_used = sum + current * (count - idx);
            sum += current;
            ++idx;
        }
        --idx;
        int deficit = space_used - spaceLeft;

        int items = count - idx;
        /*
         * If we truncate all items to "current", we would get "deficit" too many pixels. Therefore, we have to remove
         * deficit/items from each item bigger than maxval. The actual value to remove is deficitPerItem + remainder/items
         * "rest" is the accumulated error from using integer arithmetic.
        */
        int deficitPerItem = deficit/items;
        int remainder = deficit % items;
        int maxval = current - deficitPerItem;

        int rest = 0;
        for (i = start; i < start + count; i++) {
            int maxv = maxval;
            rest += remainder;
            if (rest >= items) {
                maxv--;
                rest-=items;
            }
            std::shared_ptr<LLayoutItemData> data = chain[i];
            data->size = std::min<int>(data->minimumSize, maxv);
            data->done = true;
        }
    } else if (space < cHint + sumSpacing) {
        /*
          Less space than smartSizeHint(), but more than minimumSize.
          Currently take space equally from each, as in Qt 2.x.
          Commented-out lines will give more space to stretchier
          items.
        */
        int n = count;
        int spaceLeft = space - sumSpacing;
        int overdraft = cHint - spaceLeft;

        // first give to the fixed ones:
        for (i = start; i < start + count; i++) {
            std::shared_ptr<LLayoutItemData> data = chain[i];
            if (!data->done
                && data->minimumSize >= data->smartSizeHint()) {
                data->size = data->smartSizeHint();
                data->done = true;
                spaceLeft -= data->smartSizeHint();
                n--;
            }
        }
        bool finished = n == 0;
        while (!finished) {
            finished = true;
            double fpOver = overdraft;
            double fpW = 0;

            for (i = start; i < start+count; i++) {
                std::shared_ptr<LLayoutItemData> data = chain[i];
                if (data->done){
                    continue;
                }
                fpW += fpOver / n;
                int w = std::lround(fpW);
                data->size = data->smartSizeHint() - w;
                fpW -= w; // give the difference to the next
                if (data->size < data->minimumSize) {
                    data->done = true;
                    data->size = data->minimumSize;
                    finished = false;
                    overdraft -= data->smartSizeHint() - data->minimumSize;
                    n--;
                    break;
                }
            }
        }
    } else { // extra space
        int n = count;
        int spaceLeft = space - sumSpacing;
        // first give to the fixed ones, and handle non-expansiveness
        for (i = start; i < start + count; i++) {
            std::shared_ptr<LLayoutItemData> data = chain[i];
            if (!data->done
                && (data->maximumSize <= data->smartSizeHint()
                    || (!allEmptyNonstretch && data->empty &&
                        !data->expansive && data->stretch == 0))) {
                data->size = data->smartSizeHint();
                data->done = true;
                spaceLeft -= data->size;
                sumStretch -= data->stretch;
                if (data->expansive){
                    expandingCount--;
                }
                n--;
            }
        }
        extraspace = spaceLeft;

        /*
          Do a trial distribution and calculate how much it is off.
          If there are more deficit pixels than surplus pixels, give
          the minimum size items what they need, and repeat.
          Otherwise give to the maximum size items, and repeat.

          Paul Olav Tvete has a wonderful mathematical proof of the
          correctness of this principle, but unfortunately this
          comment is too small to contain it.
        */
        int surplus, deficit;
        do {
            surplus = deficit = 0;
            double fpSpace = spaceLeft;
            double fpW = 0;
            for (i = start; i < start + count; i++) {
                std::shared_ptr<LLayoutItemData> data = chain[i];
                if (data->done){
                    continue;
                }
                extraspace = 0;
                if (sumStretch > 0) {
                    fpW += (fpSpace * data->stretch) / sumStretch;
                } else if (expandingCount > 0) {
                    fpW += (fpSpace * (data->expansive ? 1 : 0)) / expandingCount;
                } else {
                    fpW += fpSpace * 1 / n;
                }
                int w = std::lround(fpW);
                data->size = w;
                fpW -= w; // give the difference to the next
                if (w < data->smartSizeHint()) {
                    deficit +=  data->smartSizeHint() - w;
                } else if (w > data->maximumSize) {
                    surplus += w - data->maximumSize;
                }
            }
            if (deficit > 0 && surplus <= deficit) {
                // give to the ones that have too little
                for (i = start; i < start+count; i++) {
                    std::shared_ptr<LLayoutItemData> data = chain[i];
                    if (!data->done && data->size < data->smartSizeHint()) {
                        data->size = data->smartSizeHint();
                        data->done = true;
                        spaceLeft -= data->smartSizeHint();
                        sumStretch -= data->stretch;
                        if (data->expansive){
                            expandingCount--;
                        }
                        n--;
                    }
                }
            }
            if (surplus > 0 && surplus >= deficit) {
                // take from the ones that have too much
                for (i = start; i < start + count; i++) {
                    std::shared_ptr<LLayoutItemData> data = chain[i];
                    if (!data->done && data->size > data->maximumSize) {
                        data->size = data->maximumSize;
                        data->done = true;
                        spaceLeft -= data->maximumSize;
                        sumStretch -= data->stretch;
                        if (data->expansive){
                            expandingCount--;
                        }
                        n--;
                    }
                }
            }
        } while (n > 0 && surplus != deficit);
        if (n == 0){
            extraspace = spaceLeft;
        }
    }

    /*
      As a last resort, we distribute the unwanted space equally
      among the spacers (counting the start and end of the chain). We
      could, but don't, attempt a sub-pixel allocation of the extra
      space.
    */
    int extra = extraspace / (spacerCount + 2);
    int p = pos + extra;
    for (i = start; i < start+count; i++) {
        std::shared_ptr<LLayoutItemData> data = chain[i];
        data->pos = p;
        p += data->size;
        if (!data->empty){
            p += data->effectiveSpacer(spacer) + extra;
        }
    }
}



LK_NAMESPACE_END
