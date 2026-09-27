#include "services/JavaRuntimePackage.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace
{
    std::wstring utf8ToWide(std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }
        const int size = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            nullptr, 0);
        if (size <= 0)
        {
            return {};
        }

        std::wstring converted(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            converted.data(), size);
        return converted;
    }

    std::optional<std::string> jsonString(std::string_view object, std::string_view key)
    {
        const std::string marker = "\"" + std::string(key) + "\"";
        auto position = object.find(marker);
        if (position == std::string_view::npos)
        {
            return std::nullopt;
        }
        position = object.find(':', position + marker.size());
        position = object.find('"', position);
        if (position == std::string_view::npos)
        {
            return std::nullopt;
        }

        std::string result;
        bool escaped = false;
        for (++position; position < object.size(); ++position)
        {
            const char character = object[position];
            if (escaped)
            {
                result += character;
                escaped = false;
            }
            else if (character == '\\')
            {
                escaped = true;
            }
            else if (character == '"')
            {
                return result;
            }
            else
            {
                result += character;
            }
        }
        return std::nullopt;
    }

    std::uint64_t jsonUnsigned(std::string_view object, std::string_view key)
    {
        const std::string marker = "\"" + std::string(key) + "\"";
        auto position = object.find(marker);
        if (position == std::string_view::npos)
        {
            return 0;
        }
        position = object.find(':', position + marker.size());
        if (position == std::string_view::npos)
        {
            return 0;
        }
        ++position;
        while (position < object.size() && object[position] == ' ')
        {
            ++position;
        }

        std::uint64_t value = 0;
        while (position < object.size() && object[position] >= '0' && object[position] <= '9')
        {
            value = value * 10 + static_cast<unsigned>(object[position] - '0');
            ++position;
        }
        return value;
    }

    std::string calculateSha256(const std::filesystem::path& file)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectSize = 0;
        DWORD hashSize = 0;
        DWORD returnedSize = 0;

        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
            BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                              reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
                              &returnedSize, 0) < 0 ||
            BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                              reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize),
                              &returnedSize, 0) < 0)
        {
            if (algorithm != nullptr)
            {
                BCryptCloseAlgorithmProvider(algorithm, 0);
            }
            return {};
        }

        std::vector<unsigned char> hashObject(objectSize);
        std::vector<unsigned char> digest(hashSize);
        if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize,
                             nullptr, 0, 0) < 0)
        {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return {};
        }

        std::ifstream input(file, std::ios::binary);
        std::array<char, 64 * 1024> buffer{};
        bool success = static_cast<bool>(input);
        while (success && input)
        {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto bytesRead = input.gcount();
            if (bytesRead > 0 &&
                BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                               static_cast<ULONG>(bytesRead), 0) < 0)
            {
                success = false;
            }
        }
        success = success && BCryptFinishHash(
            hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;

        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (!success)
        {
            return {};
        }

        std::ostringstream formatted;
        formatted << std::hex << std::setfill('0');
        for (const auto byte : digest)
        {
            formatted << std::setw(2) << static_cast<unsigned>(byte);
        }
        return formatted.str();
    }
}

std::optional<JavaRuntimePackage> JavaRuntimePackage::parse(std::string_view metadata)
{
    // O objeto "installer" aparece antes de "package" na resposta. Isolar o
    // segundo evita escolher acidentalmente o MSI em vez do ZIP portátil.
    const auto packageStart = metadata.find("\"package\"");
    if (packageStart == std::string_view::npos)
    {
        return std::nullopt;
    }
    const auto objectStart = metadata.find('{', packageStart);
    const auto objectEnd = metadata.find('}', objectStart);
    if (objectStart == std::string_view::npos || objectEnd == std::string_view::npos)
    {
        return std::nullopt;
    }

    const auto object = metadata.substr(objectStart, objectEnd - objectStart + 1);
    const auto link = jsonString(object, "link");
    auto checksum = jsonString(object, "checksum");
    if (!link || !checksum || checksum->size() != 64)
    {
        return std::nullopt;
    }
    std::transform(checksum->begin(), checksum->end(), checksum->begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return JavaRuntimePackage{utf8ToWide(*link), *checksum, jsonUnsigned(object, "size")};
}

bool JavaRuntimePackage::matchesFile(const std::filesystem::path& file) const
{
    return calculateSha256(file) == sha256;
}
