#include <iostream>
#include <math.h>

#include "httplib.h"
#include "virtual_http_file.h"

using namespace httplib;

int main(int count, char **args)
{
    VirtualHttpFile file;
    //file.Open("http://localhost:8080/dummy");

    Server svr;

    const size_t DATA_CHUNK_SIZE = 4;

    svr.Get("/stream", [&file](const Request &req, Response &res)
            {
                res.set_content_provider(
                    file.size(),
                    file.type().c_str(),
                    [&file](size_t offset, size_t length, DataSink &sink)
                    {
                        const size_t chunk = 32 * 1024 * 1024;
                        auto buff = file.Read(offset, chunk);

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
