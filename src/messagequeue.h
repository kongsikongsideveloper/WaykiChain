#ifndef MESSAGEQUEUE__H_INCLUDED
#define MESSAGEQUEUE__H_INCLUDED

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

constexpr std::chrono::milliseconds POP_DEFAULT_TIMEOUT{20};
constexpr size_t MSG_QUEUE_MAX_LEN = 10000;

template <typename T>
class MsgQueue final {
public:
    using SizeType = typename std::queue<T>::size_type;
    using Timeout  = std::chrono::milliseconds;

public:
    bool Pop(T* t = nullptr, const Timeout& timeout = POP_DEFAULT_TIMEOUT);
    void Push(const T& t);
    void Push(T&& t);

public:
    bool Empty();
    bool Full();
    SizeType Len();

private:
    std::queue<T> mq;
    std::condition_variable popCond;
    std::condition_variable pushCond;
    std::mutex mtx;
};

template <typename T>
bool MsgQueue<T>::Pop(T* t, const Timeout& timeout) {
    std::unique_lock<std::mutex> lock(mtx);

    if (mq.empty()) {
        // `wait_for' will return after popCond has been notified or
        // spucious wake-up happens or times out.
        // so `Pop' may return before times out though popCond is not notified
        popCond.wait_for(lock, timeout);
    }

    if (!mq.empty()) {
        if (mq.size() == MSG_QUEUE_MAX_LEN) {
            pushCond.notify_all();
        }
        if (t) {
            *t = std::move(mq.front());
        }
        mq.pop();
        return true;
    }

    return false;
}

template <typename T>
void MsgQueue<T>::Push(const T& t) {
    Push(T(t));
}

template <typename T>
void MsgQueue<T>::Push(T&& t) {
    std::unique_lock<std::mutex> lock(mtx);

    while (mq.size() == MSG_QUEUE_MAX_LEN) {
        pushCond.wait(lock);
    }
    if (mq.empty()) {
        popCond.notify_all();
    }
    mq.emplace(std::move(t));
}

template <typename T>
bool MsgQueue<T>::Empty() {
    std::unique_lock<std::mutex> lock(mtx);
    return mq.empty();
}

template <typename T>
bool MsgQueue<T>::Full() {
    std::unique_lock<std::mutex> lock(mtx);
    return mq.size() == MSG_QUEUE_MAX_LEN;
}

template <typename T>
typename MsgQueue<T>::SizeType MsgQueue<T>::Len() {
    std::unique_lock<std::mutex> lock(mtx);
    return mq.size();
}

#endif  // MESSAGEQUEUE__H_INCLUDED