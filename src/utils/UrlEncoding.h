#pragma once

#include <string>
#include <string_view>

// Encodes the path's UTF-8 bytes so names with spaces, + or brackets
// stay valid in HTTP URLs without changing the local file name.
inline std::string encodeRelativeUrlPath(std::string_view path)
{
    constexpr char digits[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(path.size());
    for (const unsigned char character : path)
    {
        const bool unreserved = (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' ||
            character == '_' || character == '.' || character == '~' || character == '/';
        if (unreserved)
        {
            encoded += static_cast<char>(character);
        }
        else
        {
            encoded += '%';
            encoded += digits[character >> 4];
            encoded += digits[character & 0x0F];
        }
    }
    return encoded;
}
