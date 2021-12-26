#include <iostream>
#include <math.h>

#include "httplib.h"
#include "virtual_http_file.h"

using namespace httplib;

int main(int count, char **args)
{
    VirtualHttpFile file;

    VirtualHttpFile::Config config = {};
    config.Url = "http://localhost:8080/dummy";
    config.BlockSize = 1 * 1024 * 1024;
    config.CacheSize = config.BlockSize * 300;
    config.MaxThreads = 32;

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

    svr.new_task_queue = [] { return new ThreadPool(1); };
 
    svr.listen("localhost", 8080);
}