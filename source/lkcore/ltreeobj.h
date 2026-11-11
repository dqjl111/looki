#ifndef LTREEOBJ_H
#define LTREEOBJ_H
#include "lobject.h"
#include <functional>
#include <list>

LK_NAMESPACE_BEGIN

class LKCOREDEF LTreeObj : public LObject
{
public:
    LTreeObj(std::shared_ptr<LTreeObj> parent = nullptr);

    virtual void moveTothread(LIThread* thread) override;

    virtual bool isTreeType() const override{
        return true;
    }

    virtual void setParent(std::shared_ptr<LTreeObj> parent);
    virtual void removeChild(std::shared_ptr<LTreeObj> child);
    virtual void pushChild(std::shared_ptr<LTreeObj> child);

    inline std::shared_ptr<LTreeObj> parent() const{
        return m_parent.lock();
    }

    template<class T>
    std::shared_ptr<T> parentT() const{
        std::shared_ptr<LTreeObj> t = m_parent.lock();
        return std::dynamic_pointer_cast<T>(t);
    }

    inline const std::list<std::shared_ptr<LTreeObj> >& children() const{
        return m_children;
    }

    inline int childIndex(const std::shared_ptr<LTreeObj>& child) const{
        return lItemIndex(m_children, child);
    }

    virtual void init() override;

    inline uint32_t childrenCount() const{
        return (uint32_t)m_children.size();
    }

    inline void setGroupId(uint16_t id){
        m_groupId = id;
    }

    inline uint16_t groupId(){
        return m_groupId;
    }

    template<class T>
    std::list<std::shared_ptr<T> > findChildren(uint16_t groupId=uint16_t(-1), const std::string& name=""){
        std::list<std::shared_ptr<T> > ret;
        if(name.empty()){
            if(groupId == uint16_t(-1)){
                for(std::shared_ptr<LTreeObj>& obj : m_children){
                    std::shared_ptr<T> t = std::dynamic_pointer_cast<T>(obj);
                    if(t!= nullptr){
                        ret.push_back(t);
                    }
                }
            }else{
                for(std::shared_ptr<LTreeObj>& obj : m_children){
                    if(obj->m_groupId == groupId){
                        std::shared_ptr<T> t = std::dynamic_pointer_cast<T>(obj);
                        if(t!= nullptr){
                            ret.push_back(t);
                        }
                    }
                }
            }
        }else{
            if(m_groupId == uint16_t(-1)){
                for(std::shared_ptr<LTreeObj>& obj : m_children){
                    std::shared_ptr<T> t = std::dynamic_pointer_cast<T>(obj);
                    if(t!= nullptr){
                        if(obj->name() == name){
                            ret.push_back(t);
                        }
                    }
                }
            }else{
                for(std::shared_ptr<LTreeObj>& obj : m_children){
                    std::shared_ptr<T> t = std::dynamic_pointer_cast<T>(obj);
                    if(t!= nullptr){
                        if(obj->m_groupId == groupId && obj->name() == name){
                            ret.push_back(t);
                        }
                    }
                }
            }
        }

        return ret;
    }

    virtual void releaseParent();
    void moveChild(int from, int to);
protected:
    void initParent(std::shared_ptr<LTreeObj> parent);
    // virtual void updateAsParentChange();

    std::function<void()> m_initObj;
    std::weak_ptr<LTreeObj> m_parent;
    std::list<std::shared_ptr<LTreeObj> > m_children;

    union{
        uint32_t m_treeObjFlags = 0;
        struct{
            uint32_t m_blockSig : 1;  // not blocking signals
            uint32_t m_wasDeleted : 1; // double-delete catcher
            uint32_t m_isDeletingChildren : 1;  // set by deleteChildren()
            uint32_t m_sendChildEvents : 1;  // if we should send ChildAdded and ChildRemoved events to parent
            uint32_t m_receiveChildEvents : 1;
            uint32_t m_deleteLaterCalled : 1;
            uint32_t m_groupId : 16;
            uint32_t m_unused : 8;
        };
    };
};

LK_NAMESPACE_END

#endif // LTREEOBJ_H
