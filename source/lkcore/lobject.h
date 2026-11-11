#ifndef LOBJECT_H
#define LOBJECT_H
#include <memory>
#include "lvariant.h"
#include "lcoreenums.h"

LK_NAMESPACE_BEGIN

class LIThread;
class LIThreadData;
class LObjectData;
class LEvent;
class LEventTimer;
class LKCOREDEF LObject : public LIVar
{
public:
    LObject();

    virtual ~LObject();
    virtual void moveTothread(LIThread* thread);

    std::shared_ptr<LObjectData> objData(){
        return m_data;
    }

    virtual bool isTreeType() const {
        return false;
    }

    virtual bool isWidgetType() const {
        return false;
    }

    virtual bool isWindowType() const {
        return false;
    }

    LIThread* thread();
    std::shared_ptr<LIThreadData> threadData();
    virtual bool handleEvent(std::shared_ptr<LEvent> event);
    virtual bool eventFilter(std::shared_ptr<LObject> watched, std::shared_ptr<LEvent> event);
    virtual LVariant inputMethodQuery(LInputMethodQuery, const LVariant& args = LVariant()) const;

    inline std::shared_ptr<LObject> objPtr() const{
        return getPtrT<LObject>();
    }

    std::shared_ptr<LEventTimer> startTimer(std::chrono::milliseconds interval, bool isRepeat=false);
    const std::string& name() const;
    virtual void setName(const std::string& v);

    const std::wstring wName() const;
    void setWName(const std::wstring& v);

    void addEventFilter(std::shared_ptr<LObject> obj);
    void removeEventFilter(std::shared_ptr<LObject> obj);
    void clearEventFilter();

    bool connectObjectPoll();
    bool disconnectObjectPoll();
    void objectPoll();
    bool hasObjectPoll();
protected:
    virtual void runObjectPoll();

    std::shared_ptr<LObjectData> m_data;
};



LK_NAMESPACE_END
#endif // LOBJECT_H
