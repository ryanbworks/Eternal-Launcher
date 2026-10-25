#pragma once

#include <filesystem>

// Boundary reserved for a future incremental update policy.
// The current installation already validates downloads but doesn't remove obsolete files.
class IUpdateService
{
public:
    virtual ~IUpdateService() = default;

    // TODO: detect managed files that were removed from the manifest.
    virtual bool needsUpdate(const std::filesystem::path& installationPath) = 0;

    // TODO: apply removals with backup/rollback before cleaning up obsolete files.
    virtual void updateFiles(const std::filesystem::path& installationPath) = 0;
};
