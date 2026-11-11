#include "services/SkinLookup.h"

#include "services/HttpDownloadClient.h"
#include "utils/JsonValue.h"

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <memory>

namespace
{
    constexpr std::uint32_t opaque(std::uint32_t rgb) { return 0xFF000000U | rgb; }

    bool hex(std::string_view text)
    {
        return std::all_of(text.begin(), text.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        });
    }

    std::uint32_t over(std::uint32_t base, std::uint32_t top)
    {
        const std::uint32_t alpha = top >> 24;
        if (alpha == 0xFF) return top;
        const auto channel = [&](int shift) {
            const std::uint32_t b = (base >> shift) & 0xFF, t = (top >> shift) & 0xFF;
            return ((t * alpha + b * (255 - alpha)) / 255) << shift;
        };
        return 0xFF000000U | channel(16) | channel(8) | channel(0);
    }
}

const SkinFace& SkinLookup::defaultFace()
{
    // Classic default skin face: brown hair, warm skin, blue eyes, short beard.
    static const SkinFace face{{
        opaque(0x2F200D), opaque(0x2B1E0D), opaque(0x2F1F0F), opaque(0x281C0B), opaque(0x241808), opaque(0x261A0A), opaque(0x2B1E0D), opaque(0x2A1D0D),
        opaque(0x2F1F0F), opaque(0x2F1F0F), opaque(0x2F200D), opaque(0x2F1F0F), opaque(0x2B1E0D), opaque(0x2F200D), opaque(0x2F1F0F), opaque(0x2B1E0D),
        opaque(0x2F1F0F), opaque(0xB6896C), opaque(0xBD8E72), opaque(0xC69680), opaque(0xBD8B72), opaque(0xBD8E74), opaque(0xAC765A), opaque(0x342512),
        opaque(0xAA7D66), opaque(0xB4846D), opaque(0xAA7D66), opaque(0xAD806D), opaque(0x9C725C), opaque(0xBB8972), opaque(0x9C694C), opaque(0x9C694C),
        opaque(0xB4846D), opaque(0xFFFFFF), opaque(0x523D89), opaque(0xB57B67), opaque(0xBB8972), opaque(0x523D89), opaque(0xFFFFFF), opaque(0xAA7D66),
        opaque(0x9C6346), opaque(0xB37B62), opaque(0xB78272), opaque(0x6A4030), opaque(0x6A4030), opaque(0xBE886C), opaque(0xA26A47), opaque(0x805334),
        opaque(0x905E43), opaque(0x965F40), opaque(0x774235), opaque(0x774235), opaque(0x774235), opaque(0x774235), opaque(0x8F5E3E), opaque(0x815339),
        opaque(0x6F452C), opaque(0x6D432A), opaque(0x7A4E33), opaque(0x83553B), opaque(0x83553B), opaque(0x7A4E33), opaque(0x6D432A), opaque(0x6F452C),
    }};
    return face;
}

std::optional<std::string> SkinLookup::parseProfileId(std::string_view json)
{
    const auto root = JsonValue::parse(json);
    if (!root) return std::nullopt;
    const auto* id = root->find("id");
    const auto* text = id ? id->stringValue() : nullptr;
    if (!text || text->size() != 32 || !hex(*text)) return std::nullopt;
    return *text;
}

std::string SkinLookup::base64Decode(std::string_view text)
{
    const auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    std::string output;
    output.reserve(text.size() * 3 / 4);
    std::uint32_t buffer = 0;
    int bits = 0;
    for (const char c : text) {
        if (c == '=') break;
        const int v = value(c);
        if (v < 0) continue;  // whitespace or line breaks
        buffer = (buffer << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<char>((buffer >> bits) & 0xFF));
        }
    }
    return output;
}

std::optional<std::wstring> SkinLookup::parseSkinUrl(std::string_view json)
{
    const auto root = JsonValue::parse(json);
    const auto* properties = root ? root->find("properties") : nullptr;
    const auto* list = properties ? properties->arrayValue() : nullptr;
    if (!list) return std::nullopt;
    for (const auto& property : *list) {
        const auto* name = property.find("name");
        const auto* value = property.find("value");
        if (!name || !value || !name->stringValue() || *name->stringValue() != "textures" || !value->stringValue()) continue;
        const auto textures = JsonValue::parse(base64Decode(*value->stringValue()));
        const auto* all = textures ? textures->find("textures") : nullptr;
        const auto* skin = all ? all->find("SKIN") : nullptr;
        const auto* url = skin ? skin->find("url") : nullptr;
        const auto* text = url ? url->stringValue() : nullptr;
        if (!text) return std::nullopt;
        // Only Mojang's texture host, whose paths are a plain hex hash.
        for (const std::string_view prefix : {"http://textures.minecraft.net/texture/", "https://textures.minecraft.net/texture/"}) {
            if (text->rfind(prefix, 0) != 0) continue;
            const std::string_view hash = std::string_view(*text).substr(prefix.size());
            if (hash.empty() || hash.size() > 128 || !hex(hash)) return std::nullopt;
            return L"https://textures.minecraft.net/texture/" + std::wstring(hash.begin(), hash.end());
        }
        return std::nullopt;
    }
    return std::nullopt;
}

SkinFace SkinLookup::composeFace(const std::uint32_t* pixels, int width, int height, int stride)
{
    if (!pixels || width < 64 || height < 32 || stride < width) return defaultFace();
    const auto at = [&](int x, int y) { return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(stride) + static_cast<std::size_t>(x)]; };
    SkinFace face;
    bool flatHat = true;
    const std::uint32_t firstHat = at(40, 8);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            face.pixels[static_cast<std::size_t>(y * 8 + x)] = 0xFF000000U | at(8 + x, 8 + y);
            flatHat &= at(40 + x, 8 + y) == firstHat;
        }
    if (flatHat) return face;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const std::uint32_t hat = at(40 + x, 8 + y);
            if (hat >> 24) {
                auto& pixel = face.pixels[static_cast<std::size_t>(y * 8 + x)];
                pixel = over(pixel, hat);
            }
        }
    return face;
}

std::optional<SkinFace> SkinLookup::decodeFace(const std::string& png)
{
    if (png.empty() || png.size() > 1024 * 1024) return std::nullopt;
    const auto memory = GlobalAlloc(GMEM_MOVEABLE, png.size());
    if (!memory) return std::nullopt;
    if (auto* target = GlobalLock(memory)) {
        std::copy(png.begin(), png.end(), static_cast<char*>(target));
        GlobalUnlock(memory);
    } else {
        GlobalFree(memory);
        return std::nullopt;
    }
    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) { GlobalFree(memory); return std::nullopt; }
    std::optional<SkinFace> face;
    {
        std::unique_ptr<Gdiplus::Bitmap> bitmap(Gdiplus::Bitmap::FromStream(stream));
        if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok) {
            const auto width = static_cast<int>(bitmap->GetWidth()), height = static_cast<int>(bitmap->GetHeight());
            if (width == 64 && (height == 64 || height == 32)) {
                Gdiplus::BitmapData data{};
                const Gdiplus::Rect all(0, 0, width, height);
                if (bitmap->LockBits(&all, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) == Gdiplus::Ok) {
                    face = composeFace(static_cast<const std::uint32_t*>(data.Scan0), width, height, data.Stride / 4);
                    bitmap->UnlockBits(&data);
                }
            }
        }
    }
    stream->Release();
    return face;
}

SkinLookupResult SkinLookup::lookup(const std::wstring& name, std::stop_token stop)
{
    SkinLookupResult result;
    result.name = name;
    if (name.empty() || name.size() > 16) { result.outcome = SkinLookupOutcome::NotFound; return result; }
    const HttpDownloadClient http;

    const auto profile = http.getText(L"https://api.mojang.com/users/profiles/minecraft/" + name, stop);
    if (stop.stop_requested()) return result;
    // Mojang answers 404 (older deployments: 204 with no body) for names without an account.
    if (profile.statusCode == 404 || profile.statusCode == 204 || (profile.success && profile.content.empty())) {
        result.outcome = SkinLookupOutcome::NotFound;
        return result;
    }
    if (!profile.success) return result;
    const auto id = parseProfileId(profile.content);
    if (!id) { result.outcome = SkinLookupOutcome::NotFound; return result; }

    const auto session = http.getText(L"https://sessionserver.mojang.com/session/minecraft/profile/" + std::wstring(id->begin(), id->end()), stop);
    if (stop.stop_requested() || !session.success) return result;
    const auto url = parseSkinUrl(session.content);
    if (!url) {
        // An account without a custom skin uses the default face.
        result.outcome = SkinLookupOutcome::Found;
        result.face = defaultFace();
        return result;
    }
    const auto skin = http.getText(*url, stop);
    if (stop.stop_requested() || !skin.success) return result;
    const auto face = decodeFace(skin.content);
    if (!face) return result;
    result.outcome = SkinLookupOutcome::Found;
    result.face = *face;
    return result;
}
