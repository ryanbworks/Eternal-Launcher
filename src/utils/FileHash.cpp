#include "utils/FileHash.h"

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace
{
    std::string calculate(const std::filesystem::path& file, const wchar_t* algorithmName, std::stop_token stop)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectSize = 0;
        DWORD hashSize = 0;
        DWORD returnedSize = 0;

        if (BCryptOpenAlgorithmProvider(&algorithm, algorithmName, nullptr, 0) < 0 ||
            BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                              reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
                              &returnedSize, 0) < 0 ||
            BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                              reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize),
                              &returnedSize, 0) < 0)
        {
            if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0);
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
        while (success && input && !stop.stop_requested())
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
        success = success && !stop.stop_requested() && BCryptFinishHash(
            hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;

        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (!success) return {};

        std::ostringstream formatted;
        formatted << std::hex << std::setfill('0');
        for (const auto byte : digest)
        {
            formatted << std::setw(2) << static_cast<unsigned>(byte);
        }
        return formatted.str();
    }
}

std::string FileHash::sha1(const std::filesystem::path& file, std::stop_token stop)
{
    return calculate(file, BCRYPT_SHA1_ALGORITHM, stop);
}

std::string FileHash::sha256(const std::filesystem::path& file, std::stop_token stop)
{
    return calculate(file, BCRYPT_SHA256_ALGORITHM, stop);
}
