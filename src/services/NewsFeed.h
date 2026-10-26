#pragma once

#include "types/LauncherState.h"

#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

struct NewsFeedResult
{
    bool success = false;
    bool published = false;
    std::vector<NewsItem> items;
    std::wstring errorMessage;
};

// Optional announcements published as /news.json on the file server:
// {"schemaVersion":1,"items":[{"category":"EVENTO","title":"...","date":"2026-10-25","body":"...","url":"https://..."}]}
class NewsFeed
{
public:
    static NewsFeedResult parse(std::string_view json);
    static NewsFeedResult fetch(std::wstring_view baseUrl, std::stop_token stop = {});
    // Notes for the running launcher version, shown even when the server publishes nothing.
    static NewsItem releaseNotes();
    static std::wstring formatDate(std::wstring_view isoDate);
};
