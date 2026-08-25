#ifdef MACLOCK_CONFIGURATION_ARCHIVE_COMBINED_SOURCE
static void collect_files(
    const char *path, std::vector<String> &files)
{
    fs::File directory = LittleFS.open(path, "r");
    if (!directory || !directory.isDirectory())
    {
        directory.close();
        return;
    }
    fs::File entry = directory.openNextFile();
    while (entry)
    {
        const String entry_path = entry.path();
        const bool directory_entry = entry.isDirectory();
        entry.close();
        const char *name =
            strrchr(entry_path.c_str(), '/');
        name = name ? name + 1 : entry_path.c_str();
        if (name[0] != '.')
        {
            if (directory_entry)
                collect_files(entry_path.c_str(), files);
            else
                files.push_back(entry_path);
        }
        entry = directory.openNextFile();
    }
    directory.close();
}

static void remove_tree(const char *path)
{
    fs::File entry = LittleFS.open(path, "r");
    if (!entry)
        return;
    if (!entry.isDirectory())
    {
        entry.close();
        LittleFS.remove(path);
        return;
    }
    entry.close();

    std::vector<String> children;
    fs::File directory = LittleFS.open(path, "r");
    fs::File child = directory.openNextFile();
    while (child)
    {
        children.push_back(child.path());
        child.close();
        child = directory.openNextFile();
    }
    directory.close();
    for (const String &item : children)
        remove_tree(item.c_str());
    LittleFS.rmdir(path);
}

static bool move_staged_files(
    const char *staging_root,
    const char *destination_root)
{
    std::vector<String> files;
    collect_files(staging_root, files);
    const size_t prefix_length = strlen(staging_root);
    for (const String &source : files)
    {
        const char *suffix =
            source.c_str() + prefix_length;
        const String destination =
            String(destination_root) + suffix;
        if (!ensure_parent_directories(
                destination.c_str()))
        {
            return false;
        }
        if (LittleFS.exists(destination.c_str()))
            LittleFS.remove(destination.c_str());
        if (!LittleFS.rename(
                source.c_str(), destination.c_str()))
        {
            return false;
        }
    }
    return true;
}

static void collect_root_floppies(
    std::vector<String> &files)
{
    fs::File root = LittleFS.open("/", "r");
    if (!root || !root.isDirectory())
    {
        root.close();
        return;
    }
    fs::File entry = root.openNextFile();
    while (entry)
    {
        const String path = entry.path();
        const bool regular = !entry.isDirectory();
        entry.close();
        const char *name = strrchr(path.c_str(), '/');
        name = name ? name + 1 : path.c_str();
        if (regular && is_floppy_name(name))
            files.push_back(path);
        entry = root.openNextFile();
    }
    root.close();
}

static bool replace_restored_files(bool replace_loading)
{
    remove_tree("/downloaded");
    if (!LittleFS.mkdir("/downloaded") ||
        !move_staged_files(
            kRestoreDownloaded, "/downloaded"))
    {
        return false;
    }

    std::vector<String> existing_floppies;
    collect_root_floppies(existing_floppies);
    for (const String &path : existing_floppies)
    {
        if (!LittleFS.remove(path.c_str()))
            return false;
    }
    if (!move_staged_files(kRestoreFloppies, ""))
        return false;

    if (LittleFS.exists(kRestoreRom))
    {
        if (LittleFS.exists(kRomPath) &&
            !LittleFS.remove(kRomPath))
        {
            return false;
        }
        if (!LittleFS.rename(kRestoreRom, kRomPath))
            return false;
    }
    if (replace_loading)
    {
        remove_tree("/loading");
        if (!LittleFS.mkdir("/loading") ||
            !move_staged_files(kRestoreLoading, "/loading"))
            return false;
    }
    return true;
}
#endif
