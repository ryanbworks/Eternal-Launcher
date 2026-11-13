#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

// The 8 x 8 face of a Minecraft skin, with the hat layer already applied.
// Pixels are ARGB (0xAARRGGBB), row-major, top-left first.
struct SkinFace
{
    std::array<std::uint32_t, 64> pixels{};
};

enum class SkinLookupOutcome
{
    Found,        // the nickname belongs to a Java Edition account; `face` is its skin
    NotFound,     // no such account: show the default face
    Unavailable,  // network or Mojang error: show the default face, try again later
};

struct SkinLookupResult
{
    std::wstring name;
    SkinLookupOutcome outcome = SkinLookupOutcome::Unavailable;
    SkinFace face;
};

// Finds the skin of a Java Edition nickname through Mojang's public APIs:
// name -> profile id -> textures property -> skin PNG -> face.
namespace SkinLookup
{
    // The default face shown for nicknames without an original account.
    [[nodiscard]] const SkinFace& defaultFace();

    // {"id":"<32 hex>","name":"..."} from api.mojang.com/users/profiles/minecraft/<name>.
    [[nodiscard]] std::optional<std::string> parseProfileId(std::string_view json);
    // Skin URL from a sessionserver profile: its "textures" property is base64 JSON.
    // Only Mojang's texture host is accepted, always over HTTPS.
    [[nodiscard]] std::optional<std::wstring> parseSkinUrl(std::string_view json);
    [[nodiscard]] std::string base64Decode(std::string_view text);

    // Face from a skin's pixels (64 x 64 or legacy 64 x 32, ARGB, `stride` pixels per row).
    // A hat layer that is one flat color is a legacy "no hat" fill and is ignored.
    [[nodiscard]] SkinFace composeFace(const std::uint32_t* pixels, int width, int height, int stride);
    // Decodes a PNG skin with GDI+ (which must already be started).
    [[nodiscard]] std::optional<SkinFace> decodeFace(const std::string& png);

    // Blocking; run it on a worker thread.
    [[nodiscard]] SkinLookupResult lookup(const std::wstring& name, std::stop_token stop);

    // The whole skin of an original account, to explore and copy.
    struct OriginalSkin
    {
        SkinLookupOutcome outcome = SkinLookupOutcome::Unavailable;
        std::wstring name;   // as the account writes it
        bool custom = false; // false: the account uses a default skin
        bool slim = false;
        std::string png;
    };
    // The "slim" model flag of a sessionserver profile's skin.
    [[nodiscard]] bool parseSlim(std::string_view json);
    // Blocking; NotFound for names that are not valid nicknames or have no account.
    [[nodiscard]] OriginalSkin fetchOriginal(const std::wstring& name, std::stop_token stop);
}
