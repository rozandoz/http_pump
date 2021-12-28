#pragma once

#include <regex>
#include <string>
#include <tuple>

std::tuple<std::string, std::string> ParseUrl(const std::string &url)
{
    const std::string url_regex =
        "^(?:(?:(([^:\\/#\\?]+:)?(?:(?:\\/\\/)(?:(?:(?:([^:@\\/#\\?]+)(?:\\:([^:@\\/"
        "#\\?]*))?)@)?(([^:\\/#\\?\\]\\[]+|\\[[^\\/\\]@#?]+\\])(?:\\:([0-9]+))?))?)?)?((?:\\/"
        "?(?:[^\\/\\?#]+\\/+)*)(?:[^\\?#]*)))?(\\?[^#]+)?)(#.*)?";

    std::smatch match;
    std::regex regex(url_regex);
    if (std::regex_match(url, match, regex) && match.size() >= 9)
        return std::tuple<std::string, std::string>(match[1],
                                                    std::string(match[8]) + std::string(match[9]));

    throw std::invalid_argument("url");
}