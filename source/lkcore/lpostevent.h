#ifndef LPOSTEVENT_H
#define LPOSTEVENT_H
#include "lobjectdata.h"
#include "levent.h"
#include <functional>

LK_NAMESPACE_BEGIN
class LKCOREDEF LTimerEvent : public LEvent
{
public:
    explicit LTimerEvent( int timerId );
    int timerId() const { return m_id; }
protected:
    int m_id;
};


class LKCOREDEF LChildEvent : public LEvent
{
public:
    LChildEvent( Type type, std::shared_ptr<LObject> child );
    ~LChildEvent();
    std::shared_ptr<LObject> child() const { return c.lock(); }
    bool added() const { return type() == ChildAdded; }
    bool polished() const { return type() == ChildPolished; }
    bool removed() const { return type() == ChildRemoved; }
protected:
    std::weak_ptr<LObject> c;
};


class LKCOREDEF LAsyncEvent
{
public:
    LAsyncEvent() {}
    enum{
        Post,
        Slot,
        BlockSlot
    };

    virtual uint8_t asyncType() = 0;
};

class LKCOREDEF LPostEvent : public LAsyncEvent
{
public:
    LPostEvent() {}
    virtual bool isPostEvent(){
        return true;
    }
    std::weak_ptr<LObject> m_receiver;
    std::shared_ptr<LEvent> m_event;

    virtual uint8_t asyncType(){
        return Post;
    }
};

// SlotEvent
class LKCOREDEF LSlotEvent : public LAsyncEvent
{
public:
    LSlotEvent(std::function<void()> &&slot) : m_slot(std::move(slot))
    {
        // empty
    }

    ~LSlotEvent()
    {
        // empty
    }

    void invoke()
    {
        //std::cout  << "Slot event" << std::endl;
        m_slot();
    }

    virtual uint8_t asyncType(){
        return Slot;
    }
private:
    std::function<void()> m_slot;
};

class LKCOREDEF LBlockSlotEvent : public LAsyncEvent
{
public:
    LBlockSlotEvent(std::function<void()> &&slot) : m_slot(std::move(slot))
    {
        // empty
    }

    ~LBlockSlotEvent()
    {
        // empty
    }

    void invoke()
    {
        //std::cout  << "Block slot event" << std::endl;
        m_slot();
        // wake any waiting threads
        m_mutex.lock();
        m_executed = true;
        m_cond.notify_all();
        m_mutex.unlock();
    }

    void wait(){
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this] { return m_executed; });
    }

    virtual uint8_t asyncType(){
        return BlockSlot;
    }
private:
    std::function<void()> m_slot;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_executed = false;
};

LK_NAMESPACE_END

#endif // LPOSTEVENT_H
