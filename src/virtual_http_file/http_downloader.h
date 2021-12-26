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
    struct RangeRequest
    {
        std::pair<size_t, size_t> Range;
        std::shared_ptr<cppf::memory::buffer> Buffer;
    };

    typedef std::function<void(bool completed, const RangeRequest &request)> RequestCallback;

private:
    httplib::Headers CreateRangeHeaders(size_t offset, size_t size);

public:
    explicit HttpDownloader(const std::shared_ptr<httplib::Client>& client, const std::string &path);
    virtual ~HttpDownloader();

    void SetRequestCallback(const RequestCallback &callback) { request_callback_ = callback; }
    bool EnqueueRequest(const RangeRequest &range);
    void CancellRequest() { request_cancel_event_.set(); }

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