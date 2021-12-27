#pragma once

#include <thread>
#include <atomic>
#include <string>
#include <memory>
#include <functional>

#include "httplib.h"

#include "cppf/threading/blocking_event.h"
#include "cppf/memory/blocking_queue.h"
#include "cppf/memory/buffer.h"

class HttpDownloader
{
public:
    class RangeRequest
    {
        friend class HttpDownloader;

    public:
        RangeRequest(const std::shared_ptr<cppf::memory::buffer> &buffer, const std::pair<size_t, size_t> &range);
        RangeRequest(const RangeRequest &request);

        std::shared_ptr<cppf::memory::buffer> buffer() const { return buffer_; }
        std::pair<size_t, size_t> range() const { return range_; }
        std::shared_ptr<cppf::threading::blocking_event> cancel_event() const { return cancel_event_; }
        
        const bool is_cancelled() const {return cancel_event_->wait(std::chrono::milliseconds(0)); }

    private:
        RangeRequest() = default;

        std::shared_ptr<cppf::threading::blocking_event> cancel_event_;
        std::shared_ptr<cppf::memory::buffer> buffer_;
        std::pair<size_t, size_t> range_;
    };

    typedef std::function<void(const RangeRequest &request)> RequestCallback;

private:
    httplib::Headers CreateRangeHeaders(size_t offset, size_t size);

public:
    explicit HttpDownloader(const std::shared_ptr<httplib::Client> &client, const std::string &path);
    virtual ~HttpDownloader();

    void SetRequestCallback(const RequestCallback &callback) { request_callback_ = callback; }
    bool EnqueueRequest(const RangeRequest &range);

protected:
    void OnRequestsThread();

private:
    std::shared_ptr<httplib::Client> http_client_;
    std::string http_path_;

    RequestCallback request_callback_;
    cppf::threading::blocking_event request_cancel_event_;
    cppf::memory::blocking_queue<RangeRequest> requests_queue_;
    std::thread requests_thread_;
};