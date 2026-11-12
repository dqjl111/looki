#ifndef LSIGNAL_H
#define LSIGNAL_H
#include <shared_mutex>
#include <cassert>
#include <iostream>
#include <optional>
#include "lpostevent.h"
#include "lobject.h"
#include "lthreaddata.h"
#include "ldebug.h"

LK_NAMESPACE_BEGIN

enum class ConnectionType {
    Auto,
    Direct,
    Queued,
    QueuedBlocking,
    AutoUnique = 0x80,
    DirectUnique = Direct|AutoUnique,
    QueuedUnique = Queued|AutoUnique,
    QueuedBlockingUnique = QueuedBlocking|AutoUnique,
};

template<typename ...Args>
class LBaseInvoker {
public:
    LBaseInvoker()
    {}

    virtual ~LBaseInvoker() = default;

    virtual void invoke(Args... args) const = 0;
    virtual const void* receiverPointer() const = 0;
    virtual const void* receiverFuncPointer() const = 0;
    virtual bool isReceiverAlive() const = 0;
};


template<class Receiver, typename ...Args>
class LInvoker final : public LBaseInvoker<Args...> {
public:
    using ReceiverMemFunc = void(Receiver::*)(Args...);

    LInvoker(Receiver* receiver, ReceiverMemFunc receiverFunc, ConnectionType type)
        : m_receiverFunc(receiverFunc)
        , m_recieverData(((LObject*)receiver)->objData())
        , m_type(type)
    {
    }

    void invokeAuto(Args...args) const {
        std::shared_ptr<LIThreadData> threadData = m_recieverData->threadData();
        if (threadData != nullptr) {
            if (threadData->isThreadId(std::this_thread::get_id())) {
                invokeDirect(std::forward<Args>(args)...);
            } else {
                invokeQueued(std::forward<Args>(args)...);

            }
        }
    }

    void invokeQueued(Args...args) const {
        std::shared_ptr<LIThreadData> threadData = m_recieverData->threadData();
        if (threadData != nullptr) {

            ReceiverMemFunc receiverFunc = m_receiverFunc;
            std::weak_ptr<LObjectData> recieverData = m_recieverData;

            std::function<void(Args&...)> fn =
                [receiverFunc, recieverData](Args&... args) {
                    std::shared_ptr<LObjectData> tData = recieverData.lock();
                    if (tData != nullptr) {
                        Receiver* receiver = (Receiver*)(tData->m_Obj.load(std::memory_order_relaxed));
                        if(receiver != nullptr){
                            (receiver->*receiverFunc)(std::move(args)...);
                        }
                    }
                };
            // Post the slot to the receivers thread
            std::shared_ptr<LSlotEvent> event
                = std::make_shared<LSlotEvent>(std::bind(fn, args...));
            threadData->pushEvent(std::move(event));
        }
    }

    void invokeQueuedBlocking(Args...args) const {
        std::shared_ptr<LIThreadData> threadData = m_recieverData->threadData();
        if (threadData != nullptr) {
            std::function<void(Args&...)> fn =
                [this](Args&... args) {
                    Receiver* receiver = (Receiver*)(m_recieverData->m_Obj.load(std::memory_order_relaxed));
                    if (receiver != nullptr) {
                        (receiver->*m_receiverFunc)(std::move(args)...);
                    }
                };

            std::shared_ptr<LBlockSlotEvent> event
                = std::make_shared<LBlockSlotEvent>(std::bind(fn, args...));
            threadData->pushEvent(std::move(event));
            event->wait();
        }
    }

    void invokeDirect(Args...args) const {
        Receiver* receiver = (Receiver*)(m_recieverData->m_Obj.load(std::memory_order_relaxed));
        if (receiver != nullptr) {
            (receiver->*m_receiverFunc)(std::move(args)...);
        }
    }


    virtual void invoke(Args...args) const final {
        switch (m_type) {
        case ConnectionType::Auto:{
            invokeAuto(std::move(args)...);
            break;
        }
        case ConnectionType::Direct:{
            invokeDirect(std::move(args)...);
            break;
        }
        case ConnectionType::Queued:{
            invokeQueued(std::move(args)...);
            break;
        }
        case ConnectionType::QueuedBlocking:{
            invokeQueuedBlocking(std::move(args)...);
            break;
        }
        default:{
            break;
        }
        }
    }

    virtual const void* receiverPointer() const final {
        return m_recieverData->m_Obj.load();
    }

    virtual const void* receiverFuncPointer() const final {
        return &m_receiverFunc;
    }

    virtual bool isReceiverAlive() const {
        return m_recieverData->m_Obj.load() != nullptr;
    }
private:
    ReceiverMemFunc m_receiverFunc = nullptr;
    std::shared_ptr<LObjectData> m_recieverData;
    const ConnectionType m_type;
};

template<class Receiver, typename ...Args>
std::shared_ptr<LBaseInvoker<Args...>> makeInvoker(Receiver* receiver, void(Receiver::*func)(Args...), ConnectionType type) {
    return std::make_shared<LInvoker<Receiver, Args...>>(receiver, func, type);
}


//用法 sigUpdate.connect(this, &XClass::xfun);
//如果函数有重名 sigUpdate.connect(this, (void (XClass::*)(XArgs))(&XClass::xfun));
template<class ... Args>
class LSignal
{
public:
    using SignalInvoker = std::shared_ptr<LBaseInvoker<Args...>>;

    LSignal() = default;
    LSignal(const LSignal& other) = delete;
    LSignal(LSignal&& other) noexcept = default;
    LSignal& operator=(const LSignal& other) = delete;
    LSignal& operator=(LSignal&& other) noexcept = default;

    ~LSignal(){
        disconnect();
    }

    void emt(Args const &... args) {
        std::list<SignalInvoker> invokers;
        { //invoke 的时候可能会有connect或disconnect动作影响m_mutex，所以要复制一遍再invoke，顺便清除无效的槽
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_objects.cbegin();
            for (; it != m_objects.cend();)
            {
                const SignalInvoker& invoker = *it;
                if(invoker->receiverPointer() != nullptr){
                    invokers.push_back(invoker);
                    ++it;
                }else{
                    it = m_objects.erase(it);
                }
            }
        }

        for(const SignalInvoker& invoker : invokers){
            invoker->invoke(args...);
        }
    }

    template<typename Receiver,
             typename ReceiverMemFunc = void(Receiver::*)(Args...args),
             typename std::enable_if<std::is_base_of<LObject, Receiver>::value, int>::type = 0>
    SignalInvoker connect(Receiver* receiver, void(Receiver::*func)(Args...args), ConnectionType type = ConnectionType::Auto) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if((uint8_t)type & (uint8_t)ConnectionType::AutoUnique){
            type = (ConnectionType)((uint8_t)type & 0x0F);
            for (const SignalInvoker& invoker : m_objects) {
                auto itFunc = static_cast<const ReceiverMemFunc*>(invoker->receiverFuncPointer());
                if (invoker->receiverPointer() == receiver && *itFunc == func) {
                    return invoker;
                }
            }
        }
        SignalInvoker invoker = makeInvoker(receiver, func, type);
        m_objects.push_back(invoker);
        return invoker;
    }


    template<typename Receiver,
             typename ReceiverMemFunc = void(Receiver::*)(Args...args),
             typename std::enable_if<std::is_base_of<LObject, Receiver>::value, int>::type = 0>
    bool disconnect(Receiver* receiver, void(Receiver::*func)(Args...args)) {
        bool hasFind = false;
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_objects.cbegin();
        for (; it != m_objects.cend();)
        {
            const SignalInvoker& invoker = *it;
            auto itFunc = static_cast<const ReceiverMemFunc*>(invoker->receiverFuncPointer());
            if (invoker->receiverPointer() == receiver && *itFunc == func) {
                it = m_objects.erase(it);
                hasFind = true;
            }else{
                // 不匹配时必须前进，否则会原地死循环（此处曾导致界面卡死）
                ++it;
            }
        }
        return hasFind;
    }

    template<typename Receiver,
             typename std::enable_if<std::is_base_of<LObject, Receiver>::value, int>::type = 0>
    bool disconnect(Receiver* receiver) {
        bool hasFind = false;
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_objects.cbegin();
        for (; it != m_objects.cend();)
        {
            const SignalInvoker& invoker = *it;
            if(invoker->receiverPointer() == receiver){
                it = m_objects.erase(it);
                hasFind = true;
            }else{
                ++it;
            }
        }

        return hasFind;
    }

    void disconnect(){
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_objects.clear();
    }

    template<typename Receiver,
             typename ReceiverMemFunc = void(Receiver::*)(Args...args),
             typename std::enable_if<std::is_base_of<LObject, Receiver>::value, int>::type = 0>
    bool hasConnect(Receiver* receiver, void(Receiver::*func)(Args...args)) {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_objects.cbegin();
        for (; it != m_objects.cend();)
        {
            const SignalInvoker& invoker = *it;
            auto itFunc = static_cast<const ReceiverMemFunc*>(invoker->receiverFuncPointer());
            if (invoker->receiverPointer() == receiver && *itFunc == func) {
                return true;
            }
            ++it;
        }
        return false;
    }


    template<typename Receiver,
             typename std::enable_if<std::is_base_of<LObject, Receiver>::value, int>::type = 0>
    bool hasConnect(Receiver* receiver){
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_objects.cbegin();
        for (; it != m_objects.cend();)
        {
            const SignalInvoker& invoker = *it;
            if(invoker->receiverPointer() == receiver){
                return true;
            }
            ++it;
        }
        return false;
    }

    bool isEmpty(){
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_objects.empty();
    }
private:
    mutable std::shared_mutex m_mutex;
    std::list<SignalInvoker> m_objects;
};

LK_NAMESPACE_END
#endif // LSIGNAL_H
