#include "lwidgetstyle.h"
#include "lkwidgets/core/lwidget.h"

LK_NAMESPACE_BEGIN

class LWidgetTag : public lcss::ElementTag
{
public:
    LWidgetTag(const LIVarTag * _tag = nullptr)
        : tag(_tag){

    }
    virtual bool isMatch(const lcss::ElementTag* other) const override{
        if(this == other){
            return true;
        }

        if(tag != nullptr){
            const LWidgetTag* wTag = dynamic_cast<const LWidgetTag*>(other);
            if(wTag != nullptr){
                return tag->isMatch(wTag->tag);
            }
        }
        return false;
    }

    virtual bool isMatchBefore(const ElementTag* other )const override{
        if(this == other){
            return false;
        }

        if(tag != nullptr){
            const LWidgetTag* wTag = dynamic_cast<const LWidgetTag*>(other);
            if(wTag != nullptr){
                return LIVarTag::isMatchBefore(tag, wTag->tag);
            }else{
                return true;
            }
        }
        return false;
    }

    virtual bool isMatchAfter(const ElementTag* other )const override{
        if(this == other){
            return false;
        }

        if(tag != nullptr){
            const LWidgetTag* wTag = dynamic_cast<const LWidgetTag*>(other);
            if(wTag != nullptr){
                return LIVarTag::isMatchAfter(tag, wTag->tag);
            }else{
                return false;
            }
        }else{
            if(other != nullptr){
                return true;
            }
            return false;
        }
    }
private:
    const LIVarTag * tag = nullptr;
};


LWidgetStyleSheet::LWidgetStyleSheet() :lcss::StyleSheet(lcss::StyleSheet::NoRaw)
{

}

LWidgetStyleSheet::~LWidgetStyleSheet()
{
}

const lcss::ElementTag* LWidgetStyleSheet::getRegistTag(const std::string &tagName, bool create) const
{
    static std::unordered_map<std::string, LWidgetTag> tags;
    std::unordered_map<std::string, LWidgetTag>::const_iterator iter = tags.find(tagName);
    if (iter != tags.cend()) {
        return &(iter->second);
    }


    LIVarTag * tag = LIVarTag::tag(tagName);
    if(tag == nullptr){
        return nullptr;
    }

    if (create) {
        tags[tagName] = LWidgetTag(tag);
        iter = tags.find(tagName);
        if (iter != tags.cend()) {
            return &(iter->second);
        }
    }
    return nullptr;
}

void LWidgetStyleSheet::clear()
{
    m_rules.clear();
}

bool LWidgetStyleSheet::isSelectorMatch(const lcss::Selector& selector,
                                        const LIVarTag* tag,
                                        const uint16_t states,
                                        const std::string& objName,
                                        const LUiElementType element) const
{
    LWidgetTag wTag(tag);
    return lcss::StyleSheet::isSelectorMatch(selector, &wTag, states, objName, element);
}

bool LWidgetStyleSheet::computeStyle(std::shared_ptr<lcss::StyleComputed> style, const LIVarTag *tag,
                                     const uint16_t states, const std::string &objName, const LUiElementType element) const
{
    LWidgetTag wTag(tag);
    return lcss::StyleSheet::computeStyle(style, &wTag, states, objName, element);
}


void LWidgetStyleSheet::setCachedRender(const LIVarTag *tag, uint32_t key, std::shared_ptr<LRenderRule> render)
{
    m_cachedRender[key][tag] = render;
}

std::shared_ptr<LRenderRule> LWidgetStyleSheet::getCachedRender(const LIVarTag *tag, uint32_t key) const
{
    auto tMapIter = m_cachedRender.find(key);
    if(tMapIter != m_cachedRender.cend()){
        auto iter = tMapIter->second.find(tag);
        if(iter != tMapIter->second.cend()){
            return iter->second;
        }
    }
    return nullptr;
}

void LWidgetStyleSheet::clearCachedRender()
{
    m_cachedRender.clear();
}



void LWidgetStyleSheet::buildSelector()
{
    lcss::StyleSheet::buildSelector();
    m_ids.clear();
    for(const std::shared_ptr<lcss::StyleRule>& rule : m_rules){
        int partIndex = (int)rule->selector.size() - 1;
        if (partIndex >= 0){
            const lcss::SelectorPart& part = rule->selector[partIndex];
            if(part.identifier.type == lcss::SelectorIdentifierType::Tag){
                continue;
            }

            if(part.identifier.type == lcss::SelectorIdentifierType::Id){
                m_ids.insert(part.identifier.value);
                continue;
            }

            if(part.identifier.type == lcss::SelectorIdentifierType::PseudoElement){
                partIndex --;
                if(partIndex >=0){
                    const lcss::SelectorPart& part1 = rule->selector[partIndex];
                    if(part1.identifier.type == lcss::SelectorIdentifierType::Id){
                        m_ids.insert(part.identifier.value);
                    }
                }
            }
        }
    }
}


LWidgetStyle::LWidgetStyle()
{
    m_shieldParentStyleSheet = true;
}

bool LWidgetStyle::clearCachedStyle()
{
    m_hasCachedWithCssId = false;
    if(m_cachedRender.empty()){
        return false;
    }
    m_cachedRender.clear();
    return true;
}

std::shared_ptr<LRenderRule> LWidgetStyle::getRenderRule(LUiElementType element, uint16_t state) const
{
    uint32_t key = cachedStyleKey(element, state);
    std::map<uint32_t, std::shared_ptr<LRenderRule> >::const_iterator iter = m_cachedRender.find(key);
    if(iter != m_cachedRender.cend()){
        return iter->second;
    }

    std::shared_ptr<LWidget> widget = m_widget.lock();
    if(widget == nullptr){
        return nullptr;
    }
    std::list<std::shared_ptr<LWidgetStyleSheet> > styleSheets = widget->effectStyleSheets();
    if(!styleSheets.empty()){
        bool isObjectStyle = false;

        const std::string objName = widget->name();
        if(!objName.empty()){
            for(const std::shared_ptr<LWidgetStyleSheet>& sheet : styleSheets){
                if(sheet->isObjectStyle(objName)){
                    isObjectStyle = true;
                    break;
                }
            }
        }

        const LIVarTag* tag = widget->tag();
        std::shared_ptr<LRenderRule> render = nullptr;
        if(!isObjectStyle){
            std::shared_ptr<LWidgetStyleSheet> sheet = styleSheets.back();
            render = sheet->getCachedRender(tag, key);
            if(render != nullptr){
                const_cast<LWidgetStyle*>(this)->m_cachedRender[key] = render;
                return render;
            }
        }


        std::shared_ptr<lcss::StyleComputed> computed = std::make_shared<lcss::StyleComputed>();
        bool hasMatch = false;
        for(const std::shared_ptr<LWidgetStyleSheet>& sheet : styleSheets){
            if(sheet->computeStyle(computed, tag, state, objName, element)){
                hasMatch = true;
            }
        }

        if(hasMatch){
            render = std::make_shared<LRenderRule>(computed);
            //m_cachedRender[key] = computed;
            if(!isObjectStyle){
                std::shared_ptr<LWidgetStyleSheet> sheet = styleSheets.back();
                sheet->setCachedRender(tag, key, render);
            }
            return render;
        }else{
            return nullptr;
        }
    }else{
        return nullptr;
    }
}


LK_NAMESPACE_END
