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

void InitLogger()
{
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    static plog::RollingFileAppender<plog::TxtFormatter> fileAppeder("log.txt");

    plog::init(plog::debug).addAppender(&consoleAppender).addAppender(&fileAppeder);
}

//#define DEBUG

int main(int count, char **args)
{
    try
    {
        cxxopts::Options options("http-pump", "http-pump");

        auto adder = options.add_options();
        adder("u,url", "Url", cxxopts::value<string>());
        adder("c,cache", "Overall cache size (MB)", cxxopts::value<size_t>()->default_value("300"));
        adder("b,block", "Block size (MB)", cxxopts::value<size_t>()->default_value("1"));
        adder("t,treads", "Max threads", cxxopts::value<size_t>()->default_value("8"));
        adder("h,help", "Print usage");

        auto result = options.parse(count, args);

        if (result.count("help"))
        {
            cout << options.help();
            return 0;
        }

        InitLogger();

        PLOG_INFO << "-----------------------------------------------";

        VirtualHttpFile file;

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

        PLOG_INFO << "Url: " << config.Url;
        PLOG_INFO << "Type: " << file.type();
        PLOG_INFO << "Size: " << file.size();

        Server svr;

        svr.Get("/stream", [&](const Request &req, Response &res) {
            if (req.has_header("Range"))
                PLOG_DEBUG << "Range request: " << req.get_header_value("Range");

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

    return 0;
}