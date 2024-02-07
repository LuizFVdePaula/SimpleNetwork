#pragma once

#include "net_common.hpp"

namespace net {

    template <typename T>
    class tsqueue {
    public:
        tsqueue() = default;
        tsqueue(const tsqueue<T>&) = delete;
        ~tsqueue() {
            clear();
        }

        const T& front() {
            std::scoped_lock lock(muxQueue);
            return deqQueue.front();
        }

        const T& back() {
            std::scoped_lock lock(muxQueue);
            return deqQueue.back();
        }

        void emplace_back(const T& item) {
            std::scoped_lock lock(muxQueue);
            deqQueue.emplace_back(std::move(item));

            std::unique_lock<std::mutex> ul(muxBlocking);
            cvBlocking.notify_one();
        }

        void emplace_front(const T& item) {
            std::scoped_lock lock(muxQueue);
            deqQueue.emplace_front(std::move(item));

            std::unique_lock<std::mutex> ul(muxBlocking);
            cvBlocking.notify_one();
        }

        bool empty() {
            std::scoped_lock lock(muxQueue);
            return deqQueue.empty();
        }

        std::size_t size() {
            std::scoped_lock lock(muxQueue);
            return deqQueue.size();
        }

        void clear() {
            std::scoped_lock lock(muxQueue);
            deqQueue.clear();
        }

        T pop_back() {
            std::scoped_lock lock(muxQueue);
            auto t = std::move(deqQueue.back());
            deqQueue.pop_back();
            return t;
        }

        T pop_front() {
            std::scoped_lock lock(muxQueue);
            auto t = std::move(deqQueue.front());
            deqQueue.pop_front();
            return t;
        }

        void wait() {
            while (empty()) {
                std::unique_lock<std::mutex> ul(muxBlocking);
                cvBlocking.wait(ul);
            }
        }

    protected:
        std::mutex muxQueue;
        std::deque<T> deqQueue;
        std::condition_variable cvBlocking;
		std::mutex muxBlocking;
    };

}