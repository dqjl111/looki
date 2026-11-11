#include "ltreeobj.h"
#include "lthreaddata.h"
#include "lobjectdata.h"
#include "lcoreapplication.h"
#include "ldebug.h"

LK_NAMESPACE_BEGIN

LTreeObj::LTreeObj(std::shared_ptr<LTreeObj> parent)
{
    if(parent != nullptr){
        m_initObj = std::bind(&LTreeObj::initParent, this, parent);
    }
}

void LTreeObj::moveTothread(LIThread *thread)
{
    LObject::moveTothread(thread);
    for(std::shared_ptr<LTreeObj> child : m_children){
        child->moveTothread(thread);
    }
    std::shared_ptr<LTreeObj> parent = m_parent.lock();
    if(parent != nullptr){
        if(parent->m_data->m_threadData.lock() != m_data->m_threadData.lock()){
            setParent(nullptr);
        }
    }
}


void LTreeObj::initParent(std::shared_ptr<LTreeObj> parent) {
    if (parent != nullptr) {
        if (m_data->m_threadData.lock() != parent->objData()->m_threadData.lock()) {
            lWarn() << "LTreeObj::initParent: cannot init parent is in a different thread";
            return;
        }

        m_parent = parent;
        std::shared_ptr<LTreeObj> tThis = getPtrT<LTreeObj>();
        parent->m_children.push_back(tThis);

        if (m_sendChildEvents && parent->m_receiveChildEvents){
            std::shared_ptr<LObject> tThis = objPtr();
            std::shared_ptr<LChildEvent> e = std::make_shared<LChildEvent>(LEvent::ChildAdded, tThis);
            LCoreApplication::sendEvent(parent, e);
        }
    }
    // updateAsParentChange();
}

// void LTreeObj::updateAsParentChange()
// {

// }

void LTreeObj::setParent(std::shared_ptr<LTreeObj> o)
{
    std::shared_ptr<LTreeObj> parent = m_parent.lock();
    if (o == parent){
        return;
    }

    if (parent != nullptr) {
        std::shared_ptr<LTreeObj> tThis = getPtrT<LTreeObj>();
        parent->removeChild(tThis);
    }
    m_parent = o;
    initParent(o);
}

void LTreeObj::removeChild(std::shared_ptr<LTreeObj> child) {
    if(child != nullptr){
        if(m_children.remove(child)>0){
            std::shared_ptr<LTreeObj> tThis = getPtrT<LTreeObj>();
            if(child->parent() == tThis){
                child->m_parent.reset();
                if(m_receiveChildEvents && child->m_sendChildEvents){
                    std::shared_ptr<LChildEvent> e = std::make_shared<LChildEvent>(LEvent::ChildRemoved, child);
                    LCoreApplication::sendEvent(tThis, e);
                }
                // updateAsParentChange();
            }
        }
    }
}

void LTreeObj::pushChild(std::shared_ptr<LTreeObj> child) {
    if(child != nullptr){
        std::shared_ptr<LTreeObj> tThis = getPtrT<LTreeObj>();
        child->setParent(tThis);
    }
}

void LTreeObj::init()
{
    if(m_initObj != nullptr){
        m_initObj();
        m_initObj = nullptr;
    }
}

void LTreeObj::releaseParent()
{
    std::shared_ptr<LTreeObj> parent = m_parent.lock();
    if(parent != nullptr){
        parent->removeChild(getPtrT<LTreeObj>());
    }
}

void LTreeObj::moveChild(int from, int to)
{
    if(from <0 || to <0){
        return;
    }

    if((from != to) && (from < m_children.size()) && (to <m_children.size())){

        if(from <to){
            std::shared_ptr<LTreeObj> fromObj;
            int i=0;
            std::list<std::shared_ptr<LTreeObj> >::iterator iter = m_children.begin();
            for(;iter != m_children.end(); ++iter, ++i){
                if(i == from){
                    fromObj = *iter;
                }

                if(i >= from && i< to){
                    std::list<std::shared_ptr<LTreeObj> >::iterator next = iter;
                    next++;
                    if(next != m_children.end()){
                        *iter = *next;
                    }
                }

                if(i == to){
                    *iter = fromObj;
                    break;
                }
            }
        }else{
            std::shared_ptr<LTreeObj> fromObj;
            int i= (int)m_children.size()-1;
            std::list<std::shared_ptr<LTreeObj> >::reverse_iterator iter = m_children.rbegin();
            for(;iter != m_children.rend(); ++iter, --i){
                if(i == from){
                    fromObj = *iter;
                }

                if(i <= from && i> to){
                    std::list<std::shared_ptr<LTreeObj> >::reverse_iterator next = iter;
                    next++;
                    if(next != m_children.rend()){
                        *iter = *next;
                    }
                }

                if(i == to){
                    *iter = fromObj;
                    break;
                }
            }
        }
    }
}

LK_NAMESPACE_END
