#include "services/JavaRuntimePackage.h"
#include "utils/FileHash.h"

#include <windows.h>

#include <algorithm>
#include <cctype>

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

}

std::optional<JavaRuntimePackage> JavaRuntimePackage::parse(std::string_view metadata)
{
    // The "installer" object appears before "package" in the response. Isolating the
    // latter avoids picking the MSI instead of the portable ZIP by accident.
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

bool JavaRuntimePackage::matchesFile(const std::filesystem::path& file, std::stop_token stop) const
{
    return FileHash::sha256(file,stop) == sha256;
}
