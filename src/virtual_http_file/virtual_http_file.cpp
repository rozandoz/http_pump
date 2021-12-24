#include "virtual_http_file.h"

#include <exception>
#include <regex>

#include "http_helper.h"

using namespace std;
using namespace httplib;

Headers VirtualHttpFile::CreateRangeHeaders(size_t offset, size_t size)
{
    auto range = make_range_header({{offset, offset + size}});

    Headers headers;
    headers.insert(range);

    return headers;
}

VirtualHttpFile::VirtualHttpFile()
    : size_(0)
{
}

void VirtualHttpFile::Open(const string &url)
{
    if (client_)
        throw runtime_error("file is already opened");

    auto url_parts = ParseUrl(url);

    client_ = make_unique<Client>(get<0>(url_parts));
    path_ = get<1>(url_parts);

    auto result = client_->Head(path_.c_str());

    if (!result)
        throw runtime_error(to_string(result.error()));

    auto res = result.value();

    if (res.has_header(ContetnLengthKey))
        size_ = stoull(res.get_header_value(ContetnLengthKey));
    else
        throw runtime_error(ContetnLengthKey);

    if (res.has_header(ContentTypKey))
        type_ = res.get_header_value(ContentTypKey);
    else
        throw runtime_error(ContentTypKey);

    headers_ = res.headers;
}

std::vector<char> VirtualHttpFile::Read(size_t offset, size_t size)
{
    if (!client_)
        throw logic_error("file must be opened first");
        
    auto headers = CreateRangeHeaders(offset, size);
    auto result = client_->Get(path_.c_str(), headers);

    vector<char> buffer;

    if (result)
    {
        auto res = result.value();
        buffer.resize(res.body.size());
        memcpy(buffer.data(), res.body.data(), buffer.size());
    }

    return buffer;
}
