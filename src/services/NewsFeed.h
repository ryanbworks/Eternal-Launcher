#pragma once

#include "types/LauncherState.h"

#include <filesystem>
#include <set>
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

// Optional announcements published as /launcher/news.json on the file server:
// {"schemaVersion":1,"items":[{"category":"EVENTO","title":"...","date":"2026-10-25","body":"...","url":"https://..."}]}
class NewsFeed
{
public:
    static NewsFeedResult parse(std::string_view json);
    static NewsFeedResult fetch(std::wstring_view baseUrl, std::stop_token stop = {});
    // Notes for the running launcher version, shown even when the server publishes nothing.
    static NewsItem releaseNotes();
    static std::wstring formatDate(std::wstring_view isoDate);
    // Stable identity of an item, used to remember what this player already read.
    static std::wstring key(const NewsItem& item);
    // Days from `isoDate` to `todayIso` (both "YYYY-MM-DD"); empty or invalid dates give -1.
    static int ageInDays(std::wstring_view isoDate, std::wstring_view todayIso);
};

// What the player has read ("seen") or closed on the Home spotlight ("dismissed"),
// kept in news-state.txt in the data folder. Unread items keep their badge until read;
// dismissing only stops the spotlight for that item.
class NewsReadState
{
public:
    // Server items older than this are never announced as new.
    static constexpr int FreshDays = 21;

    void load(const std::filesystem::path& directory);
    bool save() const;
    bool exists() const { return existed_; }
    bool seen(const NewsItem& item) const { return seen_.count(NewsFeed::key(item)) != 0; }
    bool dismissed(const NewsItem& item) const { return dismissed_.count(NewsFeed::key(item)) != 0; }
    void markSeen(const NewsItem& item) { seen_.insert(NewsFeed::key(item)); }
    void markDismissed(const NewsItem& item) { dismissed_.insert(NewsFeed::key(item)); }
    // Sets `unread` on each item: never seen, and (server items) published recently.
    void apply(std::vector<NewsItem>& items, std::wstring_view todayIso) const;
private:
    std::filesystem::path file_;
    std::set<std::wstring> seen_;
    std::set<std::wstring> dismissed_;
    bool existed_ = false;
};
