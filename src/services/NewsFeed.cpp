#include "services/NewsFeed.h"

#include "config/LauncherConfig.h"
#include "services/HttpDownloadClient.h"
#include "utils/JsonValue.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>

namespace
{
    std::wstring wide(const std::string& value)
    {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring result(static_cast<std::size_t>(std::max(size, 0)), L'\0');
        if (size > 0) MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
        return result;
    }

    std::wstring text(const JsonValue& object, std::string_view key, std::size_t limit)
    {
        const auto* value = object.find(key);
        const auto* string = value ? value->stringValue() : nullptr;
        if (!string) return {};
        auto result = wide(*string);
        result.erase(std::remove_if(result.begin(), result.end(), [](wchar_t c) { return c < 32 && c != L'\n'; }), result.end());
        if (result.size() > limit) result = result.substr(0, limit - 1) + L"…";
        return result;
    }
}

NewsFeedResult NewsFeed::parse(std::string_view json)
{
    NewsFeedResult result;
    const auto document = JsonValue::parse(json);
    if (!document || !document->objectValue()) {
        result.errorMessage = L"O arquivo de notícias não é um JSON válido.";
        return result;
    }
    const auto* items = document->find("items");
    const auto* list = items ? items->arrayValue() : nullptr;
    if (!list) {
        result.errorMessage = L"O arquivo de notícias não tem a lista \"items\".";
        return result;
    }
    for (const auto& entry : *list) {
        if (!entry.objectValue()) continue;
        NewsItem item;
        item.category = text(entry, "category", 24);
        item.title = text(entry, "title", 90);
        item.description = text(entry, "body", 600);
        item.date = formatDate(text(entry, "date", 10));
        item.url = text(entry, "url", 300);
        if (item.url.rfind(L"https://", 0) != 0) item.url.clear();
        std::transform(item.category.begin(), item.category.end(), item.category.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
        if (item.category.empty()) item.category = L"AVISO";
        if (item.title.empty()) continue;
        result.items.push_back(std::move(item));
        if (result.items.size() == 20) break;
    }
    result.success = true;
    result.published = true;
    return result;
}

NewsFeedResult NewsFeed::fetch(std::wstring_view baseUrl, std::stop_token stop)
{
    std::wstring url(baseUrl);
    if (!url.empty() && url.back() == L'/') url.pop_back();
    url += L"/news.json";
    const auto response = HttpDownloadClient{}.getText(url, stop);
    if (response.success) return parse(response.content);
    NewsFeedResult result;
    if (response.statusCode == 404) {
        // Nothing published yet is a normal state, not an error.
        result.success = true;
        return result;
    }
    result.errorMessage = response.errorMessage.empty() ? L"Não foi possível carregar as notícias." : response.errorMessage;
    return result;
}

NewsItem NewsFeed::releaseNotes()
{
    NewsItem item;
    item.category = L"ATUALIZAÇÃO";
    item.title = L"Federation: seus mods agora ficam";
    item.description = L"O launcher passou a lembrar quais mods ele mesmo instalou e só remove esses quando o servidor "
                       L"deixa de usá-los. Mods que você coloca na pasta continuam lá.";
    item.date = L"Versão " + std::wstring(LauncherConfig::LauncherVersion);
    item.url = L"https://github.com/ryanbworks/Eternal-Launcher/blob/master/CHANGELOG.md";
    item.local = true;
    return item;
}

std::wstring NewsFeed::formatDate(std::wstring_view iso)
{
    if (iso.size() != 10 || iso[4] != L'-' || iso[7] != L'-') return std::wstring(iso);
    static constexpr const wchar_t* Months[] = {L"jan", L"fev", L"mar", L"abr", L"mai", L"jun",
                                                L"jul", L"ago", L"set", L"out", L"nov", L"dez"};
    const int month = (iso[5] - L'0') * 10 + (iso[6] - L'0');
    if (month < 1 || month > 12) return std::wstring(iso);
    std::wstring day(iso.substr(8, 2));
    if (day[0] == L'0') day.erase(0, 1);
    return day + L" " + Months[month - 1] + L" " + std::wstring(iso.substr(0, 4));
}
