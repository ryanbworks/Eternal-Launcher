#include "services/GalleryApi.h"

#include "config/LauncherConfig.h"
#include "services/HttpDownloadClient.h"
#include "services/ScreenshotLibrary.h"
#include "utils/JsonValue.h"

#include <shlwapi.h>

#include <algorithm>
#include <array>
#include <fstream>

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

    std::string utf8(const std::wstring& value)
    {
        if (value.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(std::max(size, 0)), '\0');
        if (size > 0) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    bool validId(std::wstring_view id)
    {
        return id.size() == 16 && std::all_of(id.begin(), id.end(), [](wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'); });
    }

    bool validNickname(std::wstring_view name)
    {
        return name.size() >= 3 && name.size() <= 16 && std::all_of(name.begin(), name.end(), [](wchar_t c) {
            return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || c == L'_';
        });
    }

    std::wstring endpoint(std::wstring_view baseUrl, std::wstring_view path)
    {
        std::wstring url(baseUrl);
        if (!url.empty() && url.back() == L'/') url.pop_back();
        return url + L"/api/gallery/" + std::wstring(path);
    }

    std::wstring uploadError(const HttpDownloadResult& response)
    {
        switch (response.statusCode) {
        case 429: return L"Você já compartilhou bastante hoje. Tente de novo amanhã.";
        case 413: return L"Esta captura é grande demais para a galeria.";
        case 400: case 415: return L"O servidor não aceitou esta captura.";
        case 0: return L"Sem conexão com o servidor agora. Tente de novo em instantes.";
        default: return L"A galeria não respondeu (HTTP " + std::to_wstring(response.statusCode) + L"). Tente mais tarde.";
        }
    }

    bool jpegEncoder(CLSID& clsid)
    {
        UINT count = 0, size = 0;
        if (Gdiplus::GetImageEncodersSize(&count, &size) != Gdiplus::Ok || size == 0) return false;
        std::vector<BYTE> bytes(size);
        auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(bytes.data());
        if (Gdiplus::GetImageEncoders(count, size, encoders) != Gdiplus::Ok) return false;
        for (UINT i = 0; i < count; ++i)
            if (std::wstring_view(encoders[i].MimeType) == L"image/jpeg") { clsid = encoders[i].Clsid; return true; }
        return false;
    }
}

std::wstring GalleryApi::baseUrl()
{
    std::array<wchar_t, 512> value{};
    const DWORD length = GetEnvironmentVariableW(L"ETERNAL_GALLERY_URL", value.data(), static_cast<DWORD>(value.size()));
    if (length > 0 && length < value.size()) return std::wstring(value.data(), length);
    return std::wstring(LauncherConfig::FileServerBaseUrl);
}

GalleryListResult GalleryApi::parseList(std::string_view json)
{
    GalleryListResult result;
    const auto document = JsonValue::parse(json);
    const auto* photos = document && document->objectValue() ? document->find("photos") : nullptr;
    const auto* list = photos ? photos->arrayValue() : nullptr;
    if (!list) return result;
    for (const auto& entry : *list) {
        if (!entry.objectValue()) continue;
        const auto* id = entry.find("id");
        const auto* name = entry.find("name");
        if (!id || !id->stringValue() || !name || !name->stringValue()) continue;
        GalleryPhoto photo;
        photo.id = wide(*id->stringValue());
        photo.name = wide(*name->stringValue());
        // Whatever the server sent, only ids and nicknames of the expected shape are used.
        if (!validId(photo.id) || !validNickname(photo.name)) continue;
        if (const auto* created = entry.find("created")) photo.created = static_cast<std::int64_t>(created->numberValue(0));
        if (const auto* width = entry.find("width")) photo.width = static_cast<int>(width->numberValue(0));
        if (const auto* height = entry.find("height")) photo.height = static_cast<int>(height->numberValue(0));
        result.photos.push_back(std::move(photo));
        if (result.photos.size() == 200) break;
    }
    result.success = true;
    return result;
}

GalleryListResult GalleryApi::fetchList(std::wstring_view baseUrl, std::stop_token stop)
{
    const auto response = HttpDownloadClient{}.getText(endpoint(baseUrl, L"photos?limit=120"), stop);
    if (!response.success) return {};
    return parseList(response.content);
}

std::unique_ptr<Gdiplus::Bitmap> GalleryApi::decode(const std::string& bytes)
{
    if (bytes.empty()) return nullptr;
    IStream* stream = SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<UINT>(bytes.size()));
    if (!stream) return nullptr;
    std::unique_ptr<Gdiplus::Bitmap> result;
    {
        // GDI+ reads lazily from the stream; copy the pixels so the stream can go.
        Gdiplus::Bitmap source(stream, FALSE);
        if (source.GetLastStatus() == Gdiplus::Ok && source.GetWidth() > 0 && source.GetHeight() > 0) {
            auto copy = std::make_unique<Gdiplus::Bitmap>(static_cast<INT>(source.GetWidth()), static_cast<INT>(source.GetHeight()),
                PixelFormat32bppPARGB);
            if (copy->GetLastStatus() == Gdiplus::Ok) {
                Gdiplus::Graphics g(copy.get());
                g.DrawImage(&source, 0, 0, static_cast<INT>(source.GetWidth()), static_cast<INT>(source.GetHeight()));
                result = std::move(copy);
            }
        }
    }
    stream->Release();
    return result;
}

std::unique_ptr<Gdiplus::Bitmap> GalleryApi::fetchImage(std::wstring_view baseUrl, const std::wstring& id, bool full, std::stop_token stop)
{
    if (!validId(id)) return nullptr;
    const auto response = HttpDownloadClient{}.getText(endpoint(baseUrl, L"photos/" + id + (full ? L"/full" : L"/thumb")), stop);
    return response.success ? decode(response.content) : nullptr;
}

std::string GalleryApi::encodeJpeg(Gdiplus::Bitmap& bitmap, long quality)
{
    CLSID jpeg{};
    if (!jpegEncoder(jpeg)) return {};
    IStream* stream = SHCreateMemStream(nullptr, 0);
    if (!stream) return {};
    Gdiplus::EncoderParameters parameters{};
    parameters.Count = 1;
    parameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
    parameters.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    parameters.Parameter[0].NumberOfValues = 1;
    parameters.Parameter[0].Value = &quality;
    std::string bytes;
    if (bitmap.Save(stream, &jpeg, &parameters) == Gdiplus::Ok) {
        STATSTG stat{};
        if (SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME)) && stat.cbSize.QuadPart > 0 && stat.cbSize.QuadPart < 64ULL * 1024 * 1024) {
            bytes.resize(static_cast<std::size_t>(stat.cbSize.QuadPart));
            const LARGE_INTEGER start{};
            ULONG read = 0;
            if (FAILED(stream->Seek(start, STREAM_SEEK_SET, nullptr)) ||
                FAILED(stream->Read(bytes.data(), static_cast<ULONG>(bytes.size()), &read)) || read != bytes.size())
                bytes.clear();
        }
    }
    stream->Release();
    return bytes;
}

std::string GalleryApi::uploadBody(const std::string& thumbnail, const std::string& picture)
{
    const auto length = static_cast<std::uint32_t>(thumbnail.size());
    std::string body;
    body.reserve(4 + thumbnail.size() + picture.size());
    for (int shift = 24; shift >= 0; shift -= 8) body.push_back(static_cast<char>((length >> shift) & 0xFF));
    body += thumbnail;
    body += picture;
    return body;
}

GalleryUploadResult GalleryApi::upload(std::wstring_view baseUrl, const std::filesystem::path& screenshot, const std::wstring& nickname,
                                       std::stop_token stop)
{
    GalleryUploadResult result;
    if (!validNickname(nickname)) { result.errorMessage = L"Escolha um nickname válido antes de compartilhar."; return result; }
    auto picture = ScreenshotLibrary::load(screenshot, 1920, 1080, false);
    auto thumbnail = ScreenshotLibrary::load(screenshot, 416, 234, true);
    if (!picture.bitmap || !thumbnail.bitmap) { result.errorMessage = L"Não foi possível abrir esta captura."; return result; }
    // Quality drops until the picture fits the server's 3 MB.
    std::string full;
    for (const long quality : {88L, 80L, 70L, 60L}) {
        full = encodeJpeg(*picture.bitmap, quality);
        if (!full.empty() && full.size() <= 3 * 1024 * 1024) break;
    }
    const auto thumb = encodeJpeg(*thumbnail.bitmap, 82);
    if (full.empty() || full.size() > 3 * 1024 * 1024 || thumb.empty()) { result.errorMessage = L"Não foi possível preparar esta captura."; return result; }
    const auto response = HttpDownloadClient{}.postBinary(endpoint(baseUrl, L"photos?name=" + nickname), uploadBody(thumb, full), stop);
    if (!response.success) { result.errorMessage = uploadError(response); return result; }
    const auto document = JsonValue::parse(response.content);
    const auto* id = document ? document->find("id") : nullptr;
    const auto* token = document ? document->find("deleteToken") : nullptr;
    if (!id || !id->stringValue() || !token || !token->stringValue() || !validId(wide(*id->stringValue()))) {
        result.errorMessage = L"A galeria respondeu de um jeito inesperado.";
        return result;
    }
    result.id = wide(*id->stringValue());
    result.deleteToken = *token->stringValue();
    if (const auto* pending = document->find("pending")) result.pending = pending->boolValue(false);
    result.success = true;
    return result;
}

bool GalleryApi::remove(std::wstring_view baseUrl, const std::wstring& id, const std::string& token, std::wstring& error)
{
    std::string escaped;
    for (const char c : token) if (c != '"' && c != '\\' && static_cast<unsigned char>(c) >= 32) escaped.push_back(c);
    const auto response = HttpDownloadClient{}.postJson(endpoint(baseUrl, L"photos/" + id + L"/delete"), "{\"token\":\"" + escaped + "\"}");
    if (response.success || response.statusCode == 404) return true;
    error = response.statusCode == 403 ? L"Esta captura não foi compartilhada por este computador." : L"Não foi possível remover agora. Tente de novo.";
    return false;
}

bool GalleryApi::report(std::wstring_view baseUrl, const std::wstring& id, std::wstring& error)
{
    const auto response = HttpDownloadClient{}.postJson(endpoint(baseUrl, L"photos/" + id + L"/report"), "{}");
    if (response.success) return true;
    error = L"Não foi possível enviar a denúncia agora. Tente de novo.";
    return false;
}

// ---------------------------------------------------------------------------------------------
// Community skins

namespace
{
    std::wstring percentEncoded(const std::wstring& value)
    {
        static constexpr char Hex[] = "0123456789ABCDEF";
        std::wstring out;
        for (const unsigned char c : utf8(value)) {
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.') {
                out.push_back(static_cast<wchar_t>(c));
            } else {
                out.push_back(L'%');
                out.push_back(static_cast<wchar_t>(Hex[c >> 4]));
                out.push_back(static_cast<wchar_t>(Hex[c & 15]));
            }
        }
        return out;
    }

    bool titleCharacter(wchar_t c)
    {
        // Python's \w for Latin letters, plus the punctuation the server allows.
        if ((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')) return true;
        if (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7) return true;
        return std::wstring_view(L" ()'-.,!_").find(c) != std::wstring_view::npos;
    }
}

std::wstring GalleryApi::skinTitle(std::wstring_view name)
{
    std::wstring title;
    for (const wchar_t c : name) {
        const wchar_t kept = titleCharacter(c) ? c : L' ';
        if (kept == L' ' && (title.empty() || title.back() == L' ')) continue;
        title.push_back(kept);
    }
    if (title.size() > 32) title.resize(32);
    while (!title.empty() && title.back() == L' ') title.pop_back();
    return title.empty() ? L"Skin" : title;
}

CommunitySkinList GalleryApi::parseSkins(std::string_view json)
{
    CommunitySkinList result;
    const auto document = JsonValue::parse(json);
    const auto* skins = document && document->objectValue() ? document->find("skins") : nullptr;
    const auto* list = skins ? skins->arrayValue() : nullptr;
    if (!list) return result;
    for (const auto& entry : *list) {
        const auto* id = entry.find("id");
        const auto* name = entry.find("name");
        const auto* title = entry.find("title");
        if (!id || !id->stringValue() || !name || !name->stringValue() || !title || !title->stringValue()) continue;
        CommunitySkin skin;
        skin.id = wide(*id->stringValue());
        skin.name = wide(*name->stringValue());
        skin.title = wide(*title->stringValue());
        if (!validId(skin.id) || !validNickname(skin.name) || skin.title.empty() || skin.title.size() > 32) continue;
        if (const auto* slim = entry.find("slim")) skin.slim = slim->boolValue(false);
        if (const auto* created = entry.find("created")) skin.created = static_cast<std::int64_t>(created->numberValue(0));
        result.skins.push_back(std::move(skin));
        if (result.skins.size() == 200) break;
    }
    result.success = true;
    return result;
}

CommunitySkinList GalleryApi::fetchSkins(std::wstring_view baseUrl, std::stop_token stop)
{
    const auto response = HttpDownloadClient{}.getText(endpoint(baseUrl, L"skins?limit=120"), stop);
    if (!response.success) return {};
    auto result = parseSkins(response.content);
    for (auto& skin : result.skins) {
        if (stop.stop_requested()) return {};
        const auto texture = HttpDownloadClient{}.getText(endpoint(baseUrl, L"skins/" + skin.id + L".png"), stop);
        if (texture.success && texture.content.size() <= 64 * 1024) skin.png = texture.content;
    }
    // A skin whose texture did not come is left out.
    result.skins.erase(std::remove_if(result.skins.begin(), result.skins.end(), [](const CommunitySkin& skin) { return skin.png.empty(); }),
        result.skins.end());
    return result;
}

GalleryUploadResult GalleryApi::shareSkin(std::wstring_view baseUrl, const std::string& png, const std::wstring& nickname, const std::wstring& title,
                                          bool slim, std::stop_token stop)
{
    GalleryUploadResult result;
    if (!validNickname(nickname)) { result.errorMessage = L"Entre na sua Conta Eternal para compartilhar."; return result; }
    const auto url = endpoint(baseUrl, L"skins?name=" + nickname + L"&title=" + percentEncoded(skinTitle(title)) +
        (slim ? L"&model=slim" : L"&model=classic"));
    const auto response = HttpDownloadClient{}.postBinary(url, png, stop);
    if (!response.success) {
        switch (response.statusCode) {
        case 429: result.errorMessage = L"Você já compartilhou bastante hoje. Tente de novo amanhã."; break;
        case 413: case 415: result.errorMessage = L"O servidor só aceita skins PNG de 64 × 64."; break;
        case 400: result.errorMessage = L"O servidor não aceitou esta skin."; break;
        case 0: result.errorMessage = L"Sem conexão com o servidor agora. Tente de novo em instantes."; break;
        default: result.errorMessage = L"O servidor não respondeu (HTTP " + std::to_wstring(response.statusCode) + L"). Tente mais tarde.";
        }
        return result;
    }
    const auto document = JsonValue::parse(response.content);
    const auto* id = document ? document->find("id") : nullptr;
    const auto* token = document ? document->find("deleteToken") : nullptr;
    if (!id || !id->stringValue() || !token || !token->stringValue() || !validId(wide(*id->stringValue()))) {
        result.errorMessage = L"O servidor respondeu de um jeito inesperado.";
        return result;
    }
    result.id = wide(*id->stringValue());
    result.deleteToken = *token->stringValue();
    if (const auto* pending = document->find("pending")) result.pending = pending->boolValue(false);
    result.success = true;
    return result;
}

bool GalleryApi::removeSkin(std::wstring_view baseUrl, const std::wstring& id, const std::string& token, std::wstring& error)
{
    if (!validId(id)) return true;
    std::string escaped;
    for (const char c : token) if (c != '"' && c != '\\' && static_cast<unsigned char>(c) >= 32) escaped.push_back(c);
    const auto response = HttpDownloadClient{}.postJson(endpoint(baseUrl, L"skins/" + id + L"/delete"), "{\"token\":\"" + escaped + "\"}");
    if (response.success || response.statusCode == 404) return true;
    error = response.statusCode == 403 ? L"Esta skin não foi compartilhada por este computador." : L"Não foi possível remover agora. Tente de novo.";
    return false;
}

bool GalleryApi::reportSkin(std::wstring_view baseUrl, const std::wstring& id, std::wstring& error)
{
    if (!validId(id)) return false;
    const auto response = HttpDownloadClient{}.postJson(endpoint(baseUrl, L"skins/" + id + L"/report"), "{}");
    if (response.success) return true;
    error = L"Não foi possível enviar a denúncia agora. Tente de novo.";
    return false;
}

// ---------------------------------------------------------------------------------------------

void GallerySharedStore::load(const std::filesystem::path& directory, const wchar_t* fileName)
{
    file_ = directory / fileName;
    entries_.clear();
    std::ifstream input(file_, std::ios::binary);
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto first = line.find('\t');
        const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
        if (second == std::string::npos) continue;
        Entry entry{wide(line.substr(0, first)), line.substr(first + 1, second - first - 1), wide(line.substr(second + 1))};
        if (validId(entry.id) && !entry.token.empty()) entries_.push_back(std::move(entry));
    }
}

bool GallerySharedStore::save() const
{
    if (file_.empty()) return false;
    std::error_code ignored;
    std::filesystem::create_directories(file_.parent_path(), ignored);
    std::ofstream output(file_, std::ios::binary | std::ios::trunc);
    for (const auto& entry : entries_) output << utf8(entry.id) << '\t' << entry.token << '\t' << utf8(entry.source) << '\n';
    return static_cast<bool>(output);
}

void GallerySharedStore::add(Entry entry)
{
    erase(entry.id);
    entries_.push_back(std::move(entry));
}

void GallerySharedStore::erase(const std::wstring& id)
{
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&id](const Entry& entry) { return entry.id == id; }), entries_.end());
}

const GallerySharedStore::Entry* GallerySharedStore::byId(const std::wstring& id) const
{
    for (const auto& entry : entries_) if (entry.id == id) return &entry;
    return nullptr;
}

const GallerySharedStore::Entry* GallerySharedStore::bySource(const std::wstring& source) const
{
    for (const auto& entry : entries_) if (entry.source == source) return &entry;
    return nullptr;
}

std::wstring GallerySharedStore::sourceKey(const std::filesystem::path& file, std::uintmax_t size)
{
    std::wstring name = file.filename().wstring();
    for (auto& c : name) if (c == L'\t' || c == L'\n' || c == L'\r') c = L' ';
    return name + L"|" + std::to_wstring(size);
}
