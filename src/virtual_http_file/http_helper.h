#pragma once

#include <regex>
#include <string>
#include <tuple>

struct UrlParts
{
    std::string Url;
    std::string FullHost;
    std::string FullPath;
};

UrlParts ParseUrl(const std::string &url)
{
    const std::string url_regex =
        "^(?:(?:(([^:\\/#\\?]+:)?(?:(?:\\/\\/)(?:(?:(?:([^:@\\/#\\?]+)(?:\\:([^:@\\/"
        "#\\?]*))?)@)?(([^:\\/#\\?\\]\\[]+|\\[[^\\/\\]@#?]+\\])(?:\\:([0-9]+))?))?)?)?((?:\\/"
        "?(?:[^\\/\\?#]+\\/+)*)(?:[^\\?#]*)))?(\\?[^#]+)?)(#.*)?";

    std::smatch match;
    std::regex regex(url_regex);
    if (!std::regex_match(url, match, regex)) throw std::invalid_argument("url");

    UrlParts parts = {};
    parts.Url = match.str(0);
    parts.FullHost = match.str(1);
    parts.FullPath = match[8].matched ? match.str(8) : "";
    if (match[9].matched) parts.FullPath += match.str(9);
    return parts;
}