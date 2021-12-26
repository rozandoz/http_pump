#include <iostream>
#include <math.h>

#include "httplib.h"
#include "cxxopts.hpp"
#include "virtual_http_file.h"

using namespace std;
using namespace httplib;

constexpr size_t ToMBytes(size_t bytes)
{
    return bytes * 1024 * 1024;
}

//#define DEBUG

int main(int count, char **args)
{

    try
    {

        VirtualHttpFile file;

        cxxopts::Options options("http-pump", "http-pump");

        options.add_options()
        ("u,url", "Url", cxxopts::value<string>())
        ("c,cache", "Overall cache size (MB)", cxxopts::value<size_t>()->default_value("300"))
        ("b,block", "Block size (MB)", cxxopts::value<size_t>()->default_value("1"))
        ("t,treads", "Max threads", cxxopts::value<size_t>()->default_value("8"))
        ("h,help", "Print usage");

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

        svr.Get("/stream", [&](const Request &req, Response &res)
                {
                    res.set_content_provider(
                        file.size(),
                        file.type().c_str(),
                        [&](size_t offset, size_t length, DataSink &sink)
                        {
                            auto buff = file.Read(offset, config.BlockSize);

                            if (!buff.empty())
                            {
                                sink.write(buff.data(), buff.size());
                                return true;
                            }

                            return false;
                        },
                        [](bool success) {
                        });
                });

        svr.listen("localhost", 8080);
    }
    catch (const exception &error)
    {
        cout << error.what() << endl;
    }
}