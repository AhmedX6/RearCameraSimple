#ifndef SEM_
#define SEM_

#include <mutex>
#include <condition_variable>

class Sem
{
public:
    void notify()
    {
        std::unique_lock<decltype(mMutex)> lock(mMutex);
        ++mCount;
        mCondition.notify_one();
    }

    void wait()
    {
        std::unique_lock<decltype(mMutex)> lock(mMutex);
        while (!mCount)
            mCondition.wait(lock);
        --mCount;
    }

    bool try_wait()
    {
        std::unique_lock<decltype(mMutex)> lock(mMutex);
        if (mCount)
        {
            --mCount;
            return true;
        }
        return false;
    }

private:
    std::mutex mMutex;
    std::condition_variable mCondition;
    unsigned long mCount = 0;
};

#endif //!SEM_