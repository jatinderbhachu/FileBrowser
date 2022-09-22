#pragma once

#include <queue>
#include <condition_variable>

template<typename T>
class ThreadedQueue {
public:
    void push(T op) {
        std::scoped_lock<std::mutex> lock(mMutex);
        mWorkQueue.push(op);
        mAwake.notify_one();
    }

    inline bool pop(T& op) {
        std::scoped_lock<std::mutex> lock(mMutex);

        while(mWorkQueue.empty()) {
            return false;
        }

        op = std::move(mWorkQueue.front());
        mWorkQueue.pop();
        return true;
    }

    inline bool empty() {
        std::scoped_lock<std::mutex> lock(mMutex);
        return mWorkQueue.empty();
    }

private:
    std::queue<T> mWorkQueue;
    std::condition_variable mAwake;
    std::mutex mMutex;
};

