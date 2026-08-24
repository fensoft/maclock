#ifdef MACLOCK_UPDATE_INSTALL_FRAGMENT
bool validate_manifest(
    JsonDocument &manifest, const char *expected_version,
    String &error)
{
    const String version = manifest["version"] | "";
    if (!version.equalsIgnoreCase(expected_version))
    {
        error = "Release and update manifest versions differ";
        return false;
    }
    if (strcmp(
            manifest["board"] | "", MACLOCK_BOARD_ID) != 0 ||
        (manifest["partitionSchema"] | 0) !=
            MACLOCK_PARTITION_SCHEMA ||
        (manifest["assetSchema"] | 0) !=
            MACLOCK_ASSET_SCHEMA)
    {
        error = "This release is not compatible with this Maclock";
        return false;
    }
    JsonObjectConst firmware = manifest["firmware"];
    JsonObjectConst assets = manifest["assets"];
    if (strcmp(firmware["name"] | "", kFirmwareName) != 0 ||
        strcmp(assets["name"] | "", kAssetsName) != 0 ||
        (firmware["size"] | 0U) == 0 ||
        (assets["size"] | 0U) == 0 ||
        strlen(firmware["sha256"] | "") != 64 ||
        strlen(assets["sha256"] | "") != 64 ||
        !assets["files"].is<JsonArrayConst>())
    {
        error = "The update manifest is incomplete";
        return false;
    }
    std::vector<String> manifest_paths;
    for (JsonObjectConst file :
         assets["files"].as<JsonArrayConst>())
    {
        const char *path = file["path"] | "";
        if (!valid_asset_path(path) ||
            strlen(file["sha256"] | "") != 64 ||
            (file["method"] | 99) > 8 ||
            ((file["method"] | 99) != 0 &&
             (file["method"] | 99) != 8))
        {
            error = "The update manifest contains an unsafe asset";
            return false;
        }
        for (const String &other : manifest_paths)
        {
            if (other == path)
            {
                error =
                    "The update manifest contains duplicate assets";
                return false;
            }
        }
        manifest_paths.emplace_back(path);
    }
    return true;
}

#endif
