#pragma once

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

// One picture in the group gallery.
struct GalleryPhoto
{
    std::wstring id;    // 16 hex characters
    std::wstring name;  // uploader's nickname
    std::int64_t created = 0;
    int width = 0;
    int height = 0;
};

struct GalleryListResult
{
    bool success = false;
    std::vector<GalleryPhoto> photos;
};

struct GalleryUploadResult
{
    bool success = false;
    std::wstring id;
    std::string deleteToken;
    bool pending = false;  // the server waits for an admin before showing it
    std::wstring errorMessage;
};

// A skin the community shared from a library (the Explorar tab of the Skins page).
struct CommunitySkin
{
    std::wstring id;     // 16 hex characters
    std::wstring name;   // who shared it
    std::wstring title;
    bool slim = false;
    std::int64_t created = 0;
    std::string png;     // filled by fetchSkins
};

struct CommunitySkinList
{
    bool success = false;
    std::vector<CommunitySkin> skins;
};

// Client of server/gallery_api.py, published under /api/gallery/.
namespace GalleryApi
{
    // Community skins: the list and every texture (a few KB each).
    CommunitySkinList parseSkins(std::string_view json);
    CommunitySkinList fetchSkins(std::wstring_view baseUrl, std::stop_token stop = {});
    GalleryUploadResult shareSkin(std::wstring_view baseUrl, const std::string& png, const std::wstring& nickname, const std::wstring& title,
                                  bool slim, std::stop_token stop = {});
    bool removeSkin(std::wstring_view baseUrl, const std::wstring& id, const std::string& token, std::wstring& error);
    bool reportSkin(std::wstring_view baseUrl, const std::wstring& id, std::wstring& error);
    // A library file name as a title the server accepts: letters, digits, spaces and
    // ()'-.,!_ only, at most 32 characters. "Skin" when nothing is left.
    std::wstring skinTitle(std::wstring_view name);

    // The file server, or ETERNAL_GALLERY_URL when set (to try a local server).
    std::wstring baseUrl();
    GalleryListResult parseList(std::string_view json);
    GalleryListResult fetchList(std::wstring_view baseUrl, std::stop_token stop = {});
    // Downloads and decodes a thumbnail or the full picture; null when unavailable.
    std::unique_ptr<Gdiplus::Bitmap> fetchImage(std::wstring_view baseUrl, const std::wstring& id, bool full, std::stop_token stop = {});
    // Scales the screenshot (1920 x 1080 at most, plus a 416 x 234 thumbnail), encodes
    // both as JPEG and uploads them under `nickname`.
    GalleryUploadResult upload(std::wstring_view baseUrl, const std::filesystem::path& screenshot, const std::wstring& nickname,
                               std::stop_token stop = {});
    bool remove(std::wstring_view baseUrl, const std::wstring& id, const std::string& token, std::wstring& error);
    bool report(std::wstring_view baseUrl, const std::wstring& id, std::wstring& error);

    // JPEG bytes of a bitmap, or empty on failure.
    std::string encodeJpeg(Gdiplus::Bitmap& bitmap, long quality);
    std::unique_ptr<Gdiplus::Bitmap> decode(const std::string& bytes);
    // The upload body: a 4-byte big-endian thumbnail length, the thumbnail, the picture.
    std::string uploadBody(const std::string& thumbnail, const std::string& picture);
}

// What this installation shared: the remove token of each picture and which local
// file it came from, kept in gallery-shared.txt in the data folder.
class GallerySharedStore
{
public:
    struct Entry { std::wstring id; std::string token; std::wstring source; };
    void load(const std::filesystem::path& directory, const wchar_t* fileName = L"gallery-shared.txt");
    bool save() const;
    void add(Entry entry);
    void erase(const std::wstring& id);
    const Entry* byId(const std::wstring& id) const;
    const Entry* bySource(const std::wstring& source) const;
    // A local screenshot's identity: its file name and size.
    static std::wstring sourceKey(const std::filesystem::path& file, std::uintmax_t size);
private:
    std::filesystem::path file_;
    std::vector<Entry> entries_;
};
