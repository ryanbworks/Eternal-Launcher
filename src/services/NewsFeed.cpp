#include "services/NewsFeed.h"

#include "config/LauncherConfig.h"
#include "services/HttpDownloadClient.h"
#include "utils/JsonValue.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <fstream>
#include <optional>

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
        item.isoDate = text(entry, "date", 10);
        item.date = formatDate(item.isoDate);
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
    // Lives next to the launcher releases, where the publishing workflow uploads it.
    url += L"/launcher/news.json";
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
    item.title = L"Capturas no launcher e um Top 3 que brilha";
    item.description = L"Nova aba Capturas: veja, copie e mande para o grupo as fotos que você tira com F2. "
                       L"O Top 3 do ranking ganhou medalhas que ficam mais enfeitadas a cada marco de horas, "
                       L"e as novidades agora aparecem em destaque até você ler.";
    item.date = L"Versão " + std::wstring(LauncherConfig::LauncherVersion);
    item.url = L"https://github.com/ryanbworks/native-game-launcher/blob/master/CHANGELOG.md";
    item.local = true;
    return item;
}

std::wstring NewsFeed::key(const NewsItem& item)
{
    if (item.local) return L"launcher|" + std::wstring(LauncherConfig::LauncherVersion);
    return item.title + L"|" + item.isoDate;
}

int NewsFeed::ageInDays(std::wstring_view isoDate, std::wstring_view todayIso)
{
    const auto parse = [](std::wstring_view iso) -> std::optional<std::chrono::sys_days> {
        if (iso.size() != 10 || iso[4] != L'-' || iso[7] != L'-') return std::nullopt;
        int parts[3]{};
        const std::size_t offsets[] = {0, 5, 8}, lengths[] = {4, 2, 2};
        for (int i = 0; i < 3; ++i)
            for (std::size_t c = offsets[i]; c < offsets[i] + lengths[i]; ++c) {
                if (iso[c] < L'0' || iso[c] > L'9') return std::nullopt;
                parts[i] = parts[i] * 10 + (iso[c] - L'0');
            }
        const std::chrono::year_month_day date{std::chrono::year(parts[0]), std::chrono::month(static_cast<unsigned>(parts[1])),
            std::chrono::day(static_cast<unsigned>(parts[2]))};
        if (!date.ok()) return std::nullopt;
        return std::chrono::sys_days(date);
    };
    const auto published = parse(isoDate), today = parse(todayIso);
    if (!published || !today) return -1;
    return static_cast<int>((*today - *published).count());
}

void NewsReadState::load(const std::filesystem::path& directory)
{
    file_ = directory / L"news-state.txt";
    seen_.clear();
    dismissed_.clear();
    std::error_code ignored;
    existed_ = std::filesystem::exists(file_, ignored);
    std::ifstream input(file_, std::ios::binary);
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 3 || line[1] != '\t') continue;
        auto value = wide(line.substr(2));
        if (line[0] == 's') seen_.insert(std::move(value));
        else if (line[0] == 'd') dismissed_.insert(std::move(value));
    }
}

bool NewsReadState::save() const
{
    if (file_.empty()) return false;
    std::error_code ignored;
    std::filesystem::create_directories(file_.parent_path(), ignored);
    std::ofstream output(file_, std::ios::binary | std::ios::trunc);
    const auto narrow = [](const std::wstring& value) {
        const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(std::max(size, 0)), '\0');
        if (size > 0) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    };
    for (const auto& key : seen_) output << "s\t" << narrow(key) << "\n";
    for (const auto& key : dismissed_) output << "d\t" << narrow(key) << "\n";
    return static_cast<bool>(output);
}

void NewsReadState::apply(std::vector<NewsItem>& items, std::wstring_view todayIso) const
{
    for (auto& item : items) {
        if (seen(item)) { item.unread = false; continue; }
        if (item.local) { item.unread = true; continue; }
        // Undated items count as new until read; dated ones only while recent.
        const int age = NewsFeed::ageInDays(item.isoDate, todayIso);
        item.unread = item.isoDate.empty() || (age >= -1 && age <= FreshDays);
    }
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
