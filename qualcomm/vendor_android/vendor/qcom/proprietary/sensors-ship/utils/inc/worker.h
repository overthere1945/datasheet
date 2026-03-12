/*
 * Copyright (c) 2017-2020, 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once
#include <atomic>
#include <thread>
#include <atomic>
#include <queue>
#include <functional>
#include <memory>
#include <string.h>
#ifdef USE_GLIB
#include <glib.h>
#define strlcpy g_strlcpy
#endif
#include "sensors_log.h"
#ifndef _WIN32
#include <pthread.h>
#endif
#include <condition_variable>

#define UNUSED(x) (void)(x)
/**
 * type alias for worker's task function
 */
using worker_task = std::function<void()>;

/**
 * Implementation of a worker thread with its own task-queue.
 * Worker thread starts running when constructed and stops when
 * destroyed. Tasks can be assigned to the worker
 * asynchronously and they will be performed in order.
 */
class worker
{
public:
    /**
     * @brief creates a worker thread and starts processing tasks
     */
    worker(): _alive(true)
    {
        _thread = std::thread([this] { run(); });
    }

    /**
     * @brief terminates the worker thread, waits until all
     *        outstanding tasks are finished.
     *
     * @note this destructor should never be called from the worker
     *       thread itself.
     */
    ~worker()
    {
        std::unique_lock<std::mutex> lk(_mutex);
        _alive = false;
        _cv.notify_one();
        lk.unlock();
        _thread.join();
        size_t num_of_task = _task_q.size();
    }

    void setname(const char *name) {
#ifndef _WIN32
        pthread_setname_np(_thread.native_handle() , name);
#endif
    }

     /**
     * @brief add a new task for the worker to do
     *
     * Tasks are performed in order in which they are added
     *
     * @param task task to perform
     */
    void add_task(const worker_task& task)
    {
        std::lock_guard<std::mutex> lk(_mutex);
        try {
            _task_q.push(task);
        } catch (std::exception& e) {
            sns_loge("failed to add new task, %s", e.what());
            return;
        }
        _cv.notify_one();
    }

private:

    /* worker thread's mainloop */
    void run()
    {
        while (_alive) {
            std::unique_lock<std::mutex> lk(_mutex);
            while (_task_q.empty() && _alive) {
                _cv.wait(lk);
            }
            if (_alive && !_task_q.empty()) {
                auto task = std::move(_task_q.front());
                _task_q.pop();
                lk.unlock();
                try {
                    if(nullptr != task) {
                        task();
                    }
                } catch (const std::exception& e) {
                    /* if an unhandled exception happened when running
                       the task, just log it and move on */
                    sns_loge("task failed, %s", e.what());
                }
                lk.lock();
            }
            lk.unlock();
        }
    }

    std::atomic<bool> _alive;
    std::queue<worker_task> _task_q;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _thread;
    /* flag to make the worker thread prevent target to go to suspend */
};
