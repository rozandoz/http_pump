#pragma once

#include <string>
#include <tuple>
#include <memory>
#include <vector>

#include "httplib.h"

class VirtualHttpFile
{
    const char *ContetnLengthKey = "Content-Length";
    const char *ContentTypKey = "Content-Type";

    httplib::Headers CreateRangeHeaders(size_t offset, size_t size);

public:
    explicit VirtualHttpFile();

    void Open(const std::string &url);
    std::vector<char> Read(size_t offset, size_t size);

    size_t size() const { return size_; }
    std::string type() const { return type_; }
    httplib::Headers headers () const { return headers_; }

private:
    std::unique_ptr<httplib::Client> client_;
    size_t size_;
    std::string type_;
    std::string path_;
    httplib::Headers headers_;
};