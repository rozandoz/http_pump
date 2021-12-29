#include "virtual_http_file.h"

#include <exception>
#include <functional>
#include <math.h>
#include <regex>

#include "plog/Log.h"

#include "http_helper.h"

using namespace std;
using namespace cppf::memory;
using namespace httplib;

inline std::string GetBlockLog(size_t number, const std::string &action)
{
    return "Block: " + to_string(number) + " --- " + action;
}

void VirtualHttpFile::Config::Validate() const
{
    if (Url.empty()) throw invalid_argument("url");
    if (!BlockSize) throw invalid_argument("block size");
    if (!CacheSize) throw invalid_argument("cache size");
    if (!MaxThreads) throw invalid_argument("max threads");
    if (BlockSize > CacheSize) throw invalid_argument("block size cannot exceed cache size");
}

VirtualHttpFile::VirtualHttpFile()
    : size_(0)
    , blocks_updated_event_(false, true)
    , block_number_(0)
    , last_block_number_(0)
{
}

void VirtualHttpFile::Open(const Config &config)
{
    config.Validate();

    if (client_) throw runtime_error("file is already opened");

    auto url_parts = ParseUrl(config.Url);
    client_ = make_shared<Client>(url_parts.FullHost);
    auto result = client_->Head(url_parts.FullPath.c_str());

    if (!result) throw runtime_error(to_string(result.error()));

    auto res = result.value();

    if (res.has_header(ContetnLengthKey))
        size_ = stoull(res.get_header_value(ContetnLengthKey));
    else
        throw runtime_error(ContetnLengthKey);

    if (res.has_header(ContentTypKey))
        type_ = res.get_header_value(ContentTypKey);
    else
        throw runtime_error(ContentTypKey);

    PLOG_INFO << "Threads: " << config.MaxThreads;

    threads_allocator_ = blocking_allocator<HttpDownloader>::create(config.MaxThreads, [&]() {
        auto client = make_shared<Client>(url_parts.FullHost);
        auto downloader = new HttpDownloader(client, url_parts.FullPath);
        auto callback = bind(&VirtualHttpFile::OnRequestEnded, this, placeholders::_1);

        downloader->SetRequestCallback(callback);
        return downloader;
    });

    auto buffers_count = (size_t)(config.CacheSize + config.BlockSize - 1) / config.BlockSize;

    PLOG_INFO << "Buffers: " << buffers_count << " (" << config.BlockSize << ")";

    memory_allocator_ = buffer_allocator::create<heap_buffer>(config.BlockSize, buffers_count);
    scheduler_thread_ = thread(bind(&VirtualHttpFile::OnSchedulerThread, this));

    config_ = config;
}

std::vector<char> VirtualHttpFile::Read(size_t position, size_t size)
{
    if (!client_) throw invalid_argument("file must be opened first");
    if (position >= size_) throw invalid_argument("offset");

    auto block_number = GetBlockNumber(position);

    PLOG_DEBUG << "Read data: " << position << " ==> " << block_number;

    block_number_ = block_number;

    if (block_number_ != last_block_number_ && block_number_ != last_block_number_ + 1)
    {
        ReleaseBlocks();
        PLOG_DEBUG << "Cache cleared!";
    }

    last_block_number_ = block_number_;

    while (true)
    {
        Block block;
        if (!TryGetReadyBlock(block_number, block))
        {
            Sleep(1);
            continue;
        }

        auto buffer = block.Buffer;

        auto block_ptr = static_cast<char *>(buffer->ptr());
        auto block_size = buffer->actual_size();

        auto block_offset = position - (block_number * config_.BlockSize);
        auto size_to_copy = min(size, block_size - block_offset);

        vector<char> result(size_to_copy);
        memcpy(result.data(), block_ptr + block_offset, size_to_copy);

        if (block_number)
        {
            ReleaseBlock(block_number - 1);
            PLOG_DEBUG << GetBlockLog(block_number - 1, "cleared");
        }

        return result;
    }
}

void VirtualHttpFile::ReleaseBlock(Block &block)
{
    block.CancelEvent->set();

    block.CancelEvent.reset();
    block.Thread.reset();
    block.Buffer.reset();
}

void VirtualHttpFile::ReleaseBlock(size_t block_number)
{
    scoped_lock locker(blocks_mutex_);

    if (blocks_.count(block_number))
    {
        ReleaseBlock(blocks_[block_number]);
        blocks_.erase(block_number);
    }
}

void VirtualHttpFile::ReleaseBlocks()
{
    scoped_lock locker(blocks_mutex_);

    for (auto it = blocks_.begin(); it != blocks_.end(); ++it)
        ReleaseBlock(it->second);

    blocks_.clear();
}

void VirtualHttpFile::OnSchedulerThread()
{
    while (true)
    {
        shared_ptr<buffer> buffer;
        if (!memory_allocator_->try_get(buffer)) break;

        shared_ptr<HttpDownloader> thread;
        if (!threads_allocator_->try_get(thread)) break;

        auto block_number = block_number_.load();

        for (auto i = block_number;; i++)
        {
            if (HasBlock(i)) continue;

            auto range = make_pair(i * config_.BlockSize, config_.BlockSize);
            HttpDownloader::RangeRequest request(buffer, range);

            Block block = {};
            block.Thread = thread;
            block.CancelEvent = request.cancel_event();

            {
                scoped_lock lock(blocks_mutex_);
                blocks_[i] = block;
            }

            thread->EnqueueRequest(request);
            PLOG_DEBUG << GetBlockLog(i, "scheduled");

            break;
        }
    }
}

void VirtualHttpFile::OnRequestEnded(const HttpDownloader::RangeRequest &request)
{
    auto block_number = GetBlockNumber(request.range().first);

    lock_guard<mutex> locker(blocks_mutex_);

    if (blocks_.count(block_number))
    {
        if (!request.is_cancelled())
        {
            blocks_[block_number].Buffer = request.buffer();
            blocks_[block_number].Thread.reset();
            blocks_updated_event_.set();
            PLOG_DEBUG << GetBlockLog(block_number, "ok");
        }
        else
        {
            blocks_.erase(block_number);
            PLOG_DEBUG << GetBlockLog(block_number, "cancelled");
        }
    }
    else
    {
        PLOG_DEBUG << GetBlockLog(block_number, "missed");
    }
}

bool VirtualHttpFile::HasBlock(size_t position)
{
    Block buffer = {};
    return TryGetBlock(position, buffer);
}

bool VirtualHttpFile::TryGetBlock(size_t position, Block &block)
{
    lock_guard<mutex> lock(blocks_mutex_);

    if (blocks_.count(position))
    {
        block = blocks_[position];
        return true;
    }
    return false;
}

bool VirtualHttpFile::TryGetReadyBlock(size_t block_number, Block &block)
{
    Block b;
    if (TryGetBlock(block_number, b))
    {
        if (b.Buffer)
        {
            block = b;
            return true;
        }
    }
    return false;
}

size_t VirtualHttpFile::GetBlockNumber(size_t position)
{
    return static_cast<size_t>(position / config_.BlockSize);
}