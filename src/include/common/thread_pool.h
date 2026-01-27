#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>
#include <iostream>

namespace chronosdb {

class ThreadPool {
public:
    // Initialize with a fixed number of threads
    explicit ThreadPool(size_t threads) : stop(false) {
        for(size_t i = 0; i < threads; ++i)
            workers.emplace_back(
                [this] {
                    while(true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            // Wait for work or stop signal
                            this->condition.wait(lock,
                                [this]{ return this->stop || !this->tasks.empty(); });
                            
                            if(this->stop && this->tasks.empty())
                                return;
                            
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        // Do the work
                        try {
                            task();
                        } catch (const std::exception& e) {
                            std::cerr << "[THREAD ERROR] Worker exception: " << e.what() << std::endl;
                        }
                    }
                }
            );
    }

    // Add new work to the pool
    template<class F, class... Args>
    auto Enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type> {
        
        using return_type = typename std::result_of<F(Args...)>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if(stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            
            tasks.emplace([task](){ (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    // Shutdown the pool politely
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker: workers) {
            if(worker.joinable()) worker.join();
        }
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};

} // namespace chronosdb