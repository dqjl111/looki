#ifndef LTHREAD_H
#define LTHREAD_H
#include "lobject.h"
#include "lthreaddata.h"
#include <shared_mutex>
LK_NAMESPACE_BEGIN

class LKCOREDEF LIThread{
public:
    LIThread();

    virtual ~LIThread();

    LIThreadData::State state();
    std::shared_ptr<LIThreadData> threadData();
    virtual bool isMain();
protected:
    std::shared_ptr<LIThreadData> m_threadData;
};

class LKCOREDEF LMainThread : public LIThread{
public:
    LMainThread();
    virtual bool isMain() override;
    std::thread::id id;
};

class LKCOREDEF LThreadDataRegistry {
public:
    static LThreadDataRegistry& instance();

    std::shared_ptr<LIThreadData> threadData(const std::thread::id& id) {
        if(id != m_mainThread.id)
        {
            std::shared_ptr<LIThreadData> ret;
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            auto it = m_registry.find(id);
            if (it == m_registry.end()) {
                return m_mainThread.threadData();
            }
            ret =  it->second.lock();

            if(ret == nullptr){
                return m_mainThread.threadData();
            }
            return ret;
        }else{
            return m_mainThread.threadData();
        }
    }

    std::shared_ptr<LIThreadData> currentThreadData() {
        return threadData(std::this_thread::get_id());
    }

    void unregisterThread(const std::thread::id& id){
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_registry.erase(id);
    }

    void registerThread(const std::thread::id& id, std::weak_ptr<LIThreadData> data);
    std::shared_ptr<LIThreadData> mainThreadData(){
        return m_mainThread.threadData();
    }

    LMainThread* mainThread(){
        return &m_mainThread;
    }
private:
    LThreadDataRegistry();
    LMainThread m_mainThread;
    std::shared_mutex m_mutex;
    std::unordered_map<std::thread::id, std::weak_ptr<LIThreadData> > m_registry;
};


class LKCOREDEF LThread : public LIThread
{
public:
    LThread();

    virtual ~LThread();

    void start();
    void quit(int returnCode = 0);
    void wake();

    std::thread::id threadId();
protected:
    virtual void run();
    void exec();
protected:
    void runThread();

    std::atomic_bool m_isExe = false;
};



LK_NAMESPACE_END
#endif // LTHREAD_H
