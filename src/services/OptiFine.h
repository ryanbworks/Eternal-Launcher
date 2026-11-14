#pragma once

#include <regex>
#include <string>
#include <string_view>

// OptiFine for the 1.8.9, fetched from optifine.net on each computer (its license does
// not allow hosting the file elsewhere).
namespace OptiFine
{
    // The optifine.net download page links the file with a one-time token:
    // downloadx?f=<file>&x=<hex>. Returns the absolute URL, or empty when the page changed.
    inline std::wstring downloadLink(std::string_view page, std::string_view fileName)
    {
        std::string escaped;
        for (const char c : fileName) {
            if (std::string_view(".^$|()[]{}*+?\\").find(c) != std::string_view::npos) escaped.push_back('\\');
            escaped.push_back(c);
        }
        const std::regex link("downloadx\\?f=" + escaped + "&x=([0-9a-fA-F]{8,64})");
        std::match_results<std::string_view::const_iterator> match;
        if (!std::regex_search(page.begin(), page.end(), match, link)) return {};
        const std::string url = "https://optifine.net/downloadx?f=" + std::string(fileName) + "&x=" + match[1].str();
        return std::wstring(url.begin(), url.end());
    }
}
