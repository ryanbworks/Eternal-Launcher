#include "services/LauncherUpdateManifest.h"
#include "config/UpdatePublicKey.h"
#include "utils/JsonValue.h"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <vector>

namespace
{
    std::string field(const JsonValue& object, std::string_view name)
    {
        const auto value = object.find(name);
        const auto text = value ? value->stringValue() : nullptr;
        return text ? *text : std::string{};
    }
    std::optional<std::vector<unsigned char>> decode(std::string_view input)
    {
        if (input.empty() || input.size() % 4 != 0) return {};
        constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::vector<unsigned char> result;
        for (std::size_t i = 0; i < input.size(); i += 4) {
            unsigned word = 0, padding = 0;
            for (unsigned j = 0; j < 4; ++j) {
                const char c = input[i+j];
                if (c == '=') {
                    if (j < 2 || i + 4 != input.size()) return {};
                    ++padding; word <<= 6;
                } else {
                    const auto n = alphabet.find(c);
                    if (n == alphabet.npos || padding) return {};
                    word = (word << 6) | static_cast<unsigned>(n);
                }
            }
            if (padding > 2 || (padding == 2 && (word & 0xffff)) || (padding == 1 && (word & 0xff))) return {};
            result.push_back(static_cast<unsigned char>(word >> 16));
            if (padding < 2) result.push_back(static_cast<unsigned char>(word >> 8));
            if (!padding) result.push_back(static_cast<unsigned char>(word));
        }
        return result;
    }
    bool verifySignature(std::span<const unsigned char> payload, std::span<const unsigned char> signature,
        std::span<const unsigned char> publicKey)
    {
        BCRYPT_ALG_HANDLE rsa = nullptr, sha = nullptr;
        BCRYPT_KEY_HANDLE key = nullptr;
        unsigned char hash[32]{};
        bool valid = false;
        if (BCryptOpenAlgorithmProvider(&rsa, BCRYPT_RSA_ALGORITHM, nullptr, 0) >= 0 &&
            BCryptOpenAlgorithmProvider(&sha, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
            BCryptImportKeyPair(rsa, nullptr, BCRYPT_RSAPUBLIC_BLOB, &key,
                const_cast<PUCHAR>(publicKey.data()), static_cast<ULONG>(publicKey.size()), 0) >= 0 &&
            BCryptHash(sha, nullptr, 0, const_cast<PUCHAR>(payload.data()),
                static_cast<ULONG>(payload.size()), hash, sizeof(hash)) >= 0) {
            BCRYPT_PSS_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, 32};
            valid = BCryptVerifySignature(key, &padding, hash, sizeof(hash),
                const_cast<PUCHAR>(signature.data()), static_cast<ULONG>(signature.size()), BCRYPT_PAD_PSS) >= 0;
        }
        if (key) BCryptDestroyKey(key);
        if (sha) BCryptCloseAlgorithmProvider(sha, 0);
        if (rsa) BCryptCloseAlgorithmProvider(rsa, 0);
        return valid;
    }
}

std::optional<LauncherUpdateManifest::Version> LauncherUpdateManifest::parseVersion(std::string_view text)
{
    Version version{};
    for (std::size_t i = 0; i < version.size(); ++i) {
        const auto end = i == 2 ? text.size() : text.find('.');
        if (end == text.npos || !end || (end > 1 && text[0] == '0')) return {};
        const auto parsed = std::from_chars(text.data(), text.data()+end, version[i]);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data()+end || version[i] > 65535) return {};
        text.remove_prefix(end);
        if (i < 2) text.remove_prefix(1);
    }
    return text.empty() ? std::optional<Version>(version) : std::nullopt;
}

std::optional<LauncherRelease> LauncherUpdateManifest::verify(std::string_view envelope,
    std::wstring& error, std::span<const unsigned char> publicKey, std::string_view architecture)
{
    error = L"O manifesto de atualização é inválido.";
    if (envelope.size() > MaximumManifestSize) return {};
    const auto document = JsonValue::parse(envelope);
    if (!document || !document->objectValue() || document->objectValue()->size() != 3) return {};
    const auto schema = document->find("schemaVersion");
    if (!schema || schema->numberValue(-1) != 1) return {};
    const auto payload = decode(field(*document, "payload"));
    const auto signature = decode(field(*document, "signature"));
    if (!payload || !signature) return {};
    if (publicKey.empty()) publicKey = LauncherUpdateTrust::PublicKey;
    if (!verifySignature(*payload, *signature, publicKey)) {
        error = L"A assinatura da atualização não é válida."; return {};
    }
    const std::string payloadText(payload->begin(), payload->end());
    const auto value = JsonValue::parse(payloadText);
    if (!value || !value->objectValue() || value->objectValue()->size() != 6) return {};
    LauncherRelease release;
    release.version = field(*value, "version");
    release.codename = field(*value, "codename");
    release.path = field(*value, "path");
    release.sha256 = field(*value, "sha256");
    if (!parseVersion(release.version)) return {};
    const auto alphaNumeric = [](unsigned char c) {return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');};
    if (release.codename.empty() || release.codename.size() > 40 ||
        !std::all_of(release.codename.begin(), release.codename.end(), alphaNumeric)) return {};
    if (release.path != "/launcher/releases/" + release.version + "/EternalLauncher.exe") return {};
    if (release.sha256.size() != 64 || !std::all_of(release.sha256.begin(), release.sha256.end(),
        [](char c) {return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');})) return {};
    const auto size = value->find("size");
    const double bytes = size ? size->numberValue(-1) : -1;
    if (!std::isfinite(bytes) || bytes < 1 || bytes > MaximumExecutableSize || bytes != std::floor(bytes)) return {};
    release.size = static_cast<std::uint64_t>(bytes);
    if (architecture.empty()) {
#ifdef _WIN64
        architecture = "x64";
#else
        architecture = "x86";
#endif
    }
    if (field(*value, "architecture") != architecture) {
        error = L"A atualização não é compatível com este launcher."; return {};
    }
    error.clear();
    return release;
}
