#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "cppf/memory/buffer_allocator.h"
#include "cppf/threading/blocking_event.h"
#include "httplib.h"

#include "http_downloader.h"

class VirtualHttpFile
{
    const char *ContetnLengthKey = "Content-Length";
    const char *ContentTypKey = "Content-Type";

    struct Block
    {
        std::shared_ptr<cppf::memory::buffer> Buffer;
        std::shared_ptr<HttpDownloader> Thread;
        std::shared_ptr<cppf::threading::blocking_event> CancelEvent;
    };

public:
    struct Config
    {
        std::string Url;
        size_t CacheSize;
        size_t BlockSize;
        size_t MaxThreads;

        void Validate() const;
    };

    explicit VirtualHttpFile();

    void Open(const Config &config);
    std::vector<char> Read(size_t offset, size_t size);

    size_t size() const { return size_; }
    std::string type() const { return type_; }

protected:
    void ReleaseBlock(Block &block);
    void ReleaseBlock(size_t block_number);
    void ReleaseBlocks();

    void OnSchedulerThread();
    void OnRequestEnded(const HttpDownloader::RangeRequest &request);

private:
    bool HasBlock(size_t block_number);
    bool TryGetBlock(size_t block_number, Block &block);
    bool TryGetReadyBlock(size_t block_number, Block &block);
    size_t GetBlockNumber(size_t position);

    Config config_;

    std::string path_;
    std::string type_;
    size_t size_;

    std::shared_ptr<httplib::Client> client_;

    std::shared_ptr<cppf::memory::buffer_allocator> memory_allocator_;
    std::shared_ptr<cppf::memory::blocking_allocator<HttpDownloader>> threads_allocator_;

    std::mutex blocks_mutex_;
    cppf::threading::blocking_event blocks_updated_event_;
    std::map<size_t, Block> blocks_;

    size_t last_block_number_;
    std::atomic_size_t block_number_;
    std::thread scheduler_thread_;
};