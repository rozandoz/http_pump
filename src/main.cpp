#include <iostream>
#include <math.h>

#include "cxxopts.hpp"
#include "httplib.h"

#include "plog/Appenders/ColorConsoleAppender.h"
#include "plog/Appenders/RollingFileAppender.h"
#include "plog/Log.h"

#include "virtual_http_file.h"

using namespace std;
using namespace httplib;

constexpr size_t ToMBytes(size_t bytes)
{
    return bytes * 1024 * 1024;
}

#define DEBUG

int main(int count, char **args)
{
    try
    {
        static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
        static plog::RollingFileAppender<plog::TxtFormatter> fileAppeder("log.txt");

        plog::init(plog::debug).addAppender(&consoleAppender).addAppender(&fileAppeder);

        PLOG_INFO << "-----------------------------------------------";

        VirtualHttpFile file;

        cxxopts::Options options("http-pump", "http-pump");

        auto op = options.add_options();
        op("u,url", "Url", cxxopts::value<string>());
        op("c,cache", "Overall cache size (MB)", cxxopts::value<size_t>()->default_value("300"));
        op("b,block", "Block size (MB)", cxxopts::value<size_t>()->default_value("1"));
        op("t,treads", "Max threads", cxxopts::value<size_t>()->default_value("8"));
        op("h,help", "Print usage");

        auto result = options.parse(count, args);

        if (result.count("help"))
        {
            cout << options.help() << std::endl;
            exit(0);
        }

#ifndef DEBUG

        VirtualHttpFile::Config config = {};
        config.Url = result["url"].as<string>();
        config.BlockSize = ToMBytes(result["block"].as<size_t>());
        config.CacheSize = ToMBytes(result["cache"].as<size_t>());
        config.MaxThreads = result["treads"].as<size_t>();

#else

        VirtualHttpFile::Config config = {};
        config.Url = "http://localhost:8080/dummy";
        config.BlockSize = 16 * 1024 * 1024;
        config.CacheSize = config.BlockSize * 20;
        config.MaxThreads = 16;

#endif

        file.Open(config);

        Server svr;

        svr.Get("/stream", [&](const Request &req, Response &res) {
            if (req.has_header("Range"))
                PLOG_DEBUG << "--- Range request: " << req.get_header_value("Range");

            res.set_content_provider(
                file.size(),
                file.type().c_str(),
                [&](size_t offset, size_t length, DataSink &sink) {
                    auto buff = file.Read(offset, config.BlockSize);

                    if (!buff.empty())
                    {
                        sink.write(buff.data(), buff.size());
                        return true;
                    }

                    return false;
                },
                [](bool success) {});
        });

        svr.new_task_queue = [] { return new ThreadPool(1); };
        svr.listen("0.0.0.0", 8080);
    }
    catch (const exception &error)
    {
        PLOG_FATAL << error.what();
    }
}