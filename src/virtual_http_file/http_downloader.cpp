#include "http_downloader.h"

#include <functional>
#include <math.h>

#include "httplib.h"

using namespace std;
using namespace cppf::memory;
using namespace cppf::threading;
using namespace httplib;

HttpDownloader::RangeRequest::RangeRequest(const shared_ptr<cppf::memory::buffer> &buffer,
                                           const pair<size_t, size_t> &range)
    : cancel_event_(make_shared<blocking_event>(false, false))
    , buffer_(buffer)
    , range_(range)
{
    if (!buffer) throw std::invalid_argument("buffer");
}

HttpDownloader::RangeRequest::RangeRequest(const RangeRequest &request)
    : cancel_event_(request.cancel_event())
    , buffer_(request.buffer())
    , range_(request.range())
{
}

Headers HttpDownloader::CreateRangeHeaders(size_t offset, size_t size)
{
    auto range = make_range_header({{offset, offset + size}});

    Headers headers;
    headers.insert(range);

    return headers;
}

HttpDownloader::HttpDownloader(const shared_ptr<Client> &http_client, const string &http_path)
    : http_client_(http_client)
    , http_path_(http_path)
    , request_cancel_event_(false, true)
    , requests_queue_(1)
    , requests_thread_(bind(&HttpDownloader::OnRequestsThread, this))
{
    if (!http_client_) throw runtime_error("http client not set");
}

HttpDownloader::~HttpDownloader()
{
    requests_queue_.complete();
    requests_queue_.clear();

    if (requests_thread_.joinable()) requests_thread_.join();
}

bool HttpDownloader::EnqueueRequest(const RangeRequest &range)
{
    return requests_queue_.try_push(range, chrono::milliseconds(0));
}

void HttpDownloader::OnRequestsThread()
{
    while (!requests_queue_.completed())
    {
        RangeRequest request;
        if (!requests_queue_.try_pop(request)) continue;

        auto buffer = request.buffer();
        auto range = request.range();
        auto cancel_event = request.cancel_event();

        buffer->actual_size(0);

        if (!request.is_cancelled())
        {
            auto header = CreateRangeHeaders(range.first, range.second);
            http_client_->Get(http_path_.c_str(), header, [&](const char *data, size_t size) {
                if (request.is_cancelled()) return false;

                auto actual_size = buffer->actual_size();
                auto size_to_copy = min(buffer->max_size() - actual_size, size);
                memcpy(static_cast<char *>(buffer->ptr()) + actual_size, data, size_to_copy);
                buffer->actual_size(actual_size + size_to_copy);

                return true;
            });
        }

        auto callback = request_callback_;

        if (callback) callback(request);
    }
}