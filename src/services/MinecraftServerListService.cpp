#include "services/MinecraftServerListService.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace
{
    constexpr std::size_t MaximumNbtFileSize = 32U * 1024U * 1024U;
    constexpr std::uint32_t MaximumCollectionSize = 1U * 1024U * 1024U;

    struct NbtTag
    {
        // Payloads simples ficam como bytes originais para conservar tags que
        // o launcher não precisa interpretar ao regravar a lista do jogador.
        std::uint8_t type = 0;
        std::string name;
        std::uint8_t listElementType = 0;
        std::vector<std::uint8_t> payload;
        std::vector<NbtTag> children;
    };

    std::string wideToUtf8(std::wstring_view value)
    {
        if (value.empty()) return {};
        const int size = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring utf8ToWide(std::string_view value)
    {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            nullptr, 0);
        if (size <= 0) return {};
        std::wstring result(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            result.data(), size);
        return result;
    }

    class NbtReader
    {
    public:
        explicit NbtReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

        bool readRoot(NbtTag& root)
        {
            if (!readByte(root.type) || root.type != 10 || !readString(root.name) ||
                !readPayload(root, 0))
            {
                return false;
            }
            return position_ == bytes_.size();
        }

    private:
        bool readByte(std::uint8_t& value)
        {
            if (position_ >= bytes_.size()) return false;
            value = bytes_[position_++];
            return true;
        }

        bool readUnsignedShort(std::uint16_t& value)
        {
            if (bytes_.size() - position_ < 2) return false;
            value = static_cast<std::uint16_t>(bytes_[position_]) << 8 |
                    static_cast<std::uint16_t>(bytes_[position_ + 1]);
            position_ += 2;
            return true;
        }

        bool readUnsignedInt(std::uint32_t& value)
        {
            if (bytes_.size() - position_ < 4) return false;
            value = static_cast<std::uint32_t>(bytes_[position_]) << 24 |
                    static_cast<std::uint32_t>(bytes_[position_ + 1]) << 16 |
                    static_cast<std::uint32_t>(bytes_[position_ + 2]) << 8 |
                    static_cast<std::uint32_t>(bytes_[position_ + 3]);
            position_ += 4;
            return true;
        }

        bool readString(std::string& value)
        {
            std::uint16_t length = 0;
            if (!readUnsignedShort(length) || bytes_.size() - position_ < length) return false;
            value.assign(reinterpret_cast<const char*>(bytes_.data() + position_), length);
            position_ += length;
            return true;
        }

        bool readRaw(std::size_t length, std::vector<std::uint8_t>& result)
        {
            if (length > bytes_.size() - position_) return false;
            result.assign(bytes_.begin() + static_cast<std::ptrdiff_t>(position_),
                          bytes_.begin() + static_cast<std::ptrdiff_t>(position_ + length));
            position_ += length;
            return true;
        }

        bool readLengthPrefixedRaw(
            std::size_t elementSize, std::vector<std::uint8_t>& result)
        {
            const auto start = position_;
            std::uint32_t count = 0;
            if (!readUnsignedInt(count) || count > MaximumCollectionSize ||
                count > (bytes_.size() - position_) / elementSize)
            {
                return false;
            }
            position_ += static_cast<std::size_t>(count) * elementSize;
            result.assign(bytes_.begin() + static_cast<std::ptrdiff_t>(start),
                          bytes_.begin() + static_cast<std::ptrdiff_t>(position_));
            return true;
        }

        bool readPayload(NbtTag& tag, unsigned depth)
        {
            if (depth > 64) return false;
            switch (tag.type)
            {
            case 1: return readRaw(1, tag.payload);
            case 2: return readRaw(2, tag.payload);
            case 3:
            case 5: return readRaw(4, tag.payload);
            case 4:
            case 6: return readRaw(8, tag.payload);
            case 7: return readLengthPrefixedRaw(1, tag.payload);
            case 8:
            {
                const auto start = position_;
                std::string ignored;
                if (!readString(ignored)) return false;
                tag.payload.assign(bytes_.begin() + static_cast<std::ptrdiff_t>(start),
                                   bytes_.begin() + static_cast<std::ptrdiff_t>(position_));
                return true;
            }
            case 9:
            {
                std::uint32_t count = 0;
                if (!readByte(tag.listElementType) || !readUnsignedInt(count) ||
                    count > MaximumCollectionSize ||
                    (count > 0 && (tag.listElementType == 0 || tag.listElementType > 12)))
                {
                    return false;
                }
                tag.children.reserve(count);
                for (std::uint32_t index = 0; index < count; ++index)
                {
                    NbtTag child;
                    child.type = tag.listElementType;
                    if (!readPayload(child, depth + 1)) return false;
                    tag.children.push_back(std::move(child));
                }
                return true;
            }
            case 10:
                while (position_ < bytes_.size())
                {
                    NbtTag child;
                    if (!readByte(child.type)) return false;
                    if (child.type == 0) return true;
                    if (child.type > 12 || !readString(child.name) ||
                        !readPayload(child, depth + 1))
                    {
                        return false;
                    }
                    tag.children.push_back(std::move(child));
                    if (tag.children.size() > MaximumCollectionSize) return false;
                }
                return false;
            case 11: return readLengthPrefixedRaw(4, tag.payload);
            case 12: return readLengthPrefixedRaw(8, tag.payload);
            default: return false;
            }
        }

        std::span<const std::uint8_t> bytes_;
        std::size_t position_ = 0;
    };

    class NbtWriter
    {
    public:
        std::vector<std::uint8_t> writeRoot(const NbtTag& root)
        {
            writeNamedTag(root);
            return std::move(bytes_);
        }

    private:
        void writeByte(std::uint8_t value) { bytes_.push_back(value); }

        void writeUnsignedShort(std::uint16_t value)
        {
            writeByte(static_cast<std::uint8_t>(value >> 8));
            writeByte(static_cast<std::uint8_t>(value));
        }

        void writeUnsignedInt(std::uint32_t value)
        {
            writeByte(static_cast<std::uint8_t>(value >> 24));
            writeByte(static_cast<std::uint8_t>(value >> 16));
            writeByte(static_cast<std::uint8_t>(value >> 8));
            writeByte(static_cast<std::uint8_t>(value));
        }

        void writeString(std::string_view value)
        {
            writeUnsignedShort(static_cast<std::uint16_t>(value.size()));
            bytes_.insert(bytes_.end(), value.begin(), value.end());
        }

        void writeNamedTag(const NbtTag& tag)
        {
            writeByte(tag.type);
            writeString(tag.name);
            writePayload(tag);
        }

        void writePayload(const NbtTag& tag)
        {
            if (tag.type == 9)
            {
                writeByte(tag.listElementType);
                writeUnsignedInt(static_cast<std::uint32_t>(tag.children.size()));
                for (const auto& child : tag.children) writePayload(child);
            }
            else if (tag.type == 10)
            {
                for (const auto& child : tag.children) writeNamedTag(child);
                writeByte(0);
            }
            else
            {
                bytes_.insert(bytes_.end(), tag.payload.begin(), tag.payload.end());
            }
        }

        std::vector<std::uint8_t> bytes_;
    };

    NbtTag* findChild(NbtTag& parent, std::string_view name)
    {
        const auto found = std::find_if(
            parent.children.begin(), parent.children.end(), [&](const NbtTag& child)
            {
                return child.name == name;
            });
        return found == parent.children.end() ? nullptr : &*found;
    }

    const NbtTag* findChild(const NbtTag& parent, std::string_view name)
    {
        const auto found = std::find_if(
            parent.children.begin(), parent.children.end(), [&](const NbtTag& child)
            {
                return child.name == name;
            });
        return found == parent.children.end() ? nullptr : &*found;
    }

    std::string stringValue(const NbtTag* tag)
    {
        if (tag == nullptr || tag->type != 8 || tag->payload.size() < 2) return {};
        const auto size = static_cast<std::size_t>(tag->payload[0]) << 8 | tag->payload[1];
        if (size + 2 != tag->payload.size()) return {};
        return {reinterpret_cast<const char*>(tag->payload.data() + 2), size};
    }

    bool setStringValue(NbtTag& tag, std::string_view value)
    {
        if (value.size() > std::numeric_limits<std::uint16_t>::max()) return false;
        tag.type = 8;
        tag.payload = {
            static_cast<std::uint8_t>(value.size() >> 8),
            static_cast<std::uint8_t>(value.size())};
        tag.payload.insert(tag.payload.end(), value.begin(), value.end());
        return true;
    }

    bool setCompoundString(NbtTag& compound, std::string_view name, std::string_view value)
    {
        auto* tag = findChild(compound, name);
        if (tag == nullptr)
        {
            compound.children.push_back({8, std::string(name)});
            tag = &compound.children.back();
        }
        return setStringValue(*tag, value);
    }

    bool readFile(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes)
    {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) return false;
        const auto size = input.tellg();
        if (size < 0 || static_cast<std::uint64_t>(size) > MaximumNbtFileSize) return false;
        bytes.resize(static_cast<std::size_t>(size));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(input) || bytes.empty();
    }

    bool writeFile(const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        output.flush();
        return static_cast<bool>(output);
    }

    std::filesystem::path executableDirectory()
    {
        std::vector<wchar_t> path(32768);
        const DWORD length = GetModuleFileNameW(
            nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size()) return {};
        return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
    }

    std::wstring quoteArgument(std::wstring_view argument)
    {
        std::wstring quoted = L"\"";
        std::size_t backslashes = 0;
        for (const wchar_t character : argument)
        {
            if (character == L'\\')
            {
                ++backslashes;
            }
            else if (character == L'\"')
            {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted += L'\"';
                backslashes = 0;
            }
            else
            {
                quoted.append(backslashes, L'\\');
                quoted += character;
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, L'\\');
        quoted += L'\"';
        return quoted;
    }

    bool runGzipPowerShell(
        const std::filesystem::path& script,
        std::wstring_view mode,
        const std::filesystem::path& source,
        const std::filesystem::path& destination,
        std::stop_token stopToken)
    {
        std::array<wchar_t, MAX_PATH> systemDirectory{};
        const UINT length = GetSystemDirectoryW(
            systemDirectory.data(), static_cast<UINT>(systemDirectory.size()));
        if (length == 0 || length >= systemDirectory.size()) return false;

        const auto powerShell = std::filesystem::path(systemDirectory.data()) /
                                L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
        if (!std::filesystem::is_regular_file(powerShell) ||
            !std::filesystem::is_regular_file(script)) return false;

        std::wstring command = quoteArgument(powerShell.wstring()) +
            L" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " +
            quoteArgument(script.wstring()) + L" " + quoteArgument(mode) + L" " +
            quoteArgument(source.wstring()) + L" " + quoteArgument(destination.wstring());
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(powerShell.c_str(), command.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
        {
            return false;
        }

        CloseHandle(process.hThread);
        while (WaitForSingleObject(process.hProcess, 100) == WAIT_TIMEOUT)
        {
            if (stopToken.stop_requested())
            {
                TerminateProcess(process.hProcess, ERROR_CANCELLED);
                WaitForSingleObject(process.hProcess, INFINITE);
                break;
            }
        }
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hProcess);
        return !stopToken.stop_requested() && exitCode == 0;
    }

    bool createTemporaryFile(const std::filesystem::path& directory,
                             std::filesystem::path& result)
    {
        std::array<wchar_t, MAX_PATH> path{};
        if (GetTempFileNameW(directory.c_str(), L"els", 0, path.data()) == 0) return false;
        result = path.data();
        return true;
    }

    struct TemporaryFiles
    {
        std::filesystem::path uncompressed;
        std::filesystem::path compressed;
        std::filesystem::path metadata;

        ~TemporaryFiles()
        {
            std::error_code ignored;
            if (!uncompressed.empty()) std::filesystem::remove(uncompressed, ignored);
            if (!compressed.empty()) std::filesystem::remove(compressed, ignored);
            if (!metadata.empty()) std::filesystem::remove(metadata, ignored);
        }
    };

    bool loadPreviousEntry(const std::filesystem::path& path,
                           std::string& previousName,
                           std::string& previousAddress)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input) return false;
        std::getline(input, previousName);
        std::getline(input, previousAddress);
        if (!previousName.empty() && previousName.back() == '\r') previousName.pop_back();
        if (!previousAddress.empty() && previousAddress.back() == '\r') previousAddress.pop_back();
        return !previousName.empty() && !previousAddress.empty();
    }

    bool matchesEntry(const NbtTag& entry, std::string_view name, std::string_view address)
    {
        return stringValue(findChild(entry, "name")) == name &&
               stringValue(findChild(entry, "ip")) == address;
    }

    NbtTag makeServerEntry(std::string_view name, std::string_view address)
    {
        NbtTag entry;
        entry.type = 10;
        setCompoundString(entry, "name", name);
        setCompoundString(entry, "ip", address);
        return entry;
    }

    bool ensureListEntry(NbtTag& root,
                         std::string_view name,
                         std::string_view address,
                         const std::filesystem::path& metadataPath)
    {
        auto* servers = findChild(root, "servers");
        if (servers == nullptr)
        {
            root.children.push_back({9, "servers", 10});
            servers = &root.children.back();
        }
        if (servers->type != 9 || (servers->listElementType != 0 &&
                                   servers->listElementType != 10)) return false;
        servers->listElementType = 10;

        // O arquivo auxiliar identifica a entrada gerenciada quando nome ou
        // endereço forem alterados na configuração entre versões do launcher.
        std::string previousName;
        std::string previousAddress;
        const bool hasPrevious = loadPreviousEntry(
            metadataPath, previousName, previousAddress);
        auto managed = servers->children.end();
        if (hasPrevious)
        {
            managed = std::find_if(
                servers->children.begin(), servers->children.end(),
                [&](const NbtTag& entry)
                {
                    return matchesEntry(entry, previousName, previousAddress);
                });
        }
        if (managed == servers->children.end())
        {
            managed = std::find_if(
                servers->children.begin(), servers->children.end(),
                [&](const NbtTag& entry) { return matchesEntry(entry, name, address); });
        }

        if (managed == servers->children.end())
        {
            servers->children.push_back(makeServerEntry(name, address));
        }
        else if (!setCompoundString(*managed, "name", name) ||
                 !setCompoundString(*managed, "ip", address))
        {
            return false;
        }
        return true;
    }
}

bool MinecraftServerListService::ensureServerPresent(
    const std::filesystem::path& gameDirectory,
    std::wstring_view serverName,
    std::wstring_view serverAddress,
    std::stop_token stopToken,
    std::wstring& errorMessage) const
{
    const auto name = wideToUtf8(serverName);
    const auto address = wideToUtf8(serverAddress);
    if (name.empty() || address.empty() || name.size() > 256 || address.size() > 255 ||
        name.find_first_of("\r\n") != std::string::npos ||
        address.find_first_of("\r\n \t") != std::string::npos)
    {
        errorMessage = L"O nome ou endereço do servidor configurado é inválido.";
        return false;
    }

    const auto serversFile = gameDirectory / L"servers.dat";
    const auto metadataFile = gameDirectory.parent_path() / L"managed-game-server.txt";
    const auto scriptFile = executableDirectory() / L"scripts" / L"GzipFile.ps1";
    TemporaryFiles temporary;
    if (!createTemporaryFile(gameDirectory, temporary.uncompressed) ||
        !createTemporaryFile(gameDirectory, temporary.compressed) ||
        !createTemporaryFile(gameDirectory.parent_path(), temporary.metadata))
    {
        errorMessage = L"Não foi possível criar arquivos temporários para a lista de servidores.";
        return false;
    }

    NbtTag root;
    if (std::filesystem::exists(serversFile))
    {
        if (!runGzipPowerShell(scriptFile, L"Decompress", serversFile,
                               temporary.uncompressed, stopToken))
        {
            errorMessage = stopToken.stop_requested()
                ? L"Configuração do servidor cancelada."
                : L"Não foi possível ler a lista Multiplayer existente.";
            return false;
        }
        std::vector<std::uint8_t> bytes;
        if (!readFile(temporary.uncompressed, bytes) || bytes.empty() ||
            bytes.size() > MaximumNbtFileSize || !NbtReader(bytes).readRoot(root))
        {
            errorMessage = L"A lista Multiplayer existente está inválida; ela foi preservada.";
            return false;
        }
    }
    else
    {
        root.type = 10;
    }

    if (root.type != 10 || !ensureListEntry(root, name, address, metadataFile))
    {
        errorMessage = L"Não foi possível atualizar a lista Multiplayer sem alterar as outras entradas.";
        return false;
    }

    auto bytes = NbtWriter().writeRoot(root);
    if (bytes.empty() || bytes.size() > MaximumNbtFileSize ||
        !writeFile(temporary.uncompressed, bytes))
    {
        errorMessage = L"Não foi possível preparar a nova lista Multiplayer.";
        return false;
    }
    if (!runGzipPowerShell(scriptFile, L"Compress", temporary.uncompressed,
                           temporary.compressed, stopToken))
    {
        errorMessage = stopToken.stop_requested()
            ? L"Configuração do servidor cancelada."
            : L"Não foi possível compactar a lista Multiplayer.";
        return false;
    }
    if (!MoveFileExW(temporary.compressed.c_str(), serversFile.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        errorMessage = L"Não foi possível salvar a entrada na lista Multiplayer.";
        return false;
    }
    temporary.compressed.clear();

    std::ofstream metadata(temporary.metadata, std::ios::binary | std::ios::trunc);
    metadata.write(name.data(), static_cast<std::streamsize>(name.size()));
    metadata.put('\n');
    metadata.write(address.data(), static_cast<std::streamsize>(address.size()));
    metadata.put('\n');
    metadata.flush();
    metadata.close();
    if (!metadata || !MoveFileExW(
            temporary.metadata.c_str(), metadataFile.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        errorMessage = L"O servidor foi salvo, mas não foi possível registrar sua configuração gerenciada.";
        return false;
    }
    temporary.metadata.clear();
    return true;
}
