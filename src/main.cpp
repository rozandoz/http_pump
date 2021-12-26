#include <iostream>
#include <math.h>

#include "httplib.h"
#include "cxxopts.hpp"
#include "virtual_http_file.h"

using namespace std;
using namespace httplib;

int main(int count, char **args)
{

    try
    {
        cxxopts::Options options("http-pump", "http-pump");

        options.add_options()
        ("u,url", "Url", cxxopts::value<string>())
        ("c,cache", "Overall cache size (bytes)", cxxopts::value<size_t>()->default_value("314572800"))
        ("b,block", "Block size (bytes)", cxxopts::value<size_t>()->default_value("1048576"))
        ("t,treads", "Max threads", cxxopts::value<size_t>()->default_value("8"))
        ("h,help", "Print usage");

        auto result = options.parse(count, args);

        if (result.count("help"))
        {
            cout << options.help() << std::endl;
            exit(0);
        }

        VirtualHttpFile file;

        VirtualHttpFile::Config config = {};
        config.Url = result["url"].as<string>();
        config.BlockSize = result["block"].as<size_t>();
        config.CacheSize = result["cache"].as<size_t>();
        config.MaxThreads = result["treads"].as<size_t>();

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

        svr.new_task_queue = []
        { return new ThreadPool(1); };

        svr.listen("localhost", 8080);
    }
    catch (const exception &error)
    {
        cout << error.what() << endl;
    }
}