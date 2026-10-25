#pragma once

#include <string>
#include <stop_token>
#include "types/InstallationProgress.h"

struct FileServerManifestResponse
{
    bool success = false;
    unsigned long httpStatus = 0;
    std::wstring content;
    std::wstring errorMessage;
    long long elapsedMilliseconds = 0;
    InstallationFailure failure = InstallationFailure::None;
};

// Generic contract for fetching the manifest from the HTTP file server.
// NGINX is never referenced here, since it is only the current host.
class IFileServerService
{
public:
    virtual ~IFileServerService() = default;

    // Fetches the manifest and returns enough data for the interface to show
    // success, unavailability or unexpected HTTP responses.
    virtual FileServerManifestResponse fetchManifest(std::stop_token stop = {}) = 0;
};
