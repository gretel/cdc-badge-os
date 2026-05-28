#include "plugin_manager/PluginStorage.h"
#include "cdc_log.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_partition.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

namespace cdc::plugin_manager {

static const char* TAG = "PLG_STO";
static const char* PARTITION_LABEL = "plugins";
static const char* MOUNT_POINT = "/plugins";

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static bool s_mounted = false;

bool PluginStorage::mount()
{
    if (s_mounted) return true;

    const esp_vfs_fat_mount_config_t cfg = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(
        MOUNT_POINT, PARTITION_LABEL, &cfg, &s_wl_handle);
    if (err != ESP_OK) {
        LOG_E(TAG, "mount failed: 0x%x", err);
        return false;
    }

    LOG_I(TAG, "mounted %s on %s", PARTITION_LABEL, MOUNT_POINT);
    s_mounted = true;
    return true;
}

void PluginStorage::unmount()
{
    if (!s_mounted) return;
    esp_vfs_fat_spiflash_unmount_rw_wl(MOUNT_POINT, s_wl_handle);
    s_wl_handle = WL_INVALID_HANDLE;
    s_mounted = false;
}

const char* PluginStorage::basePath()
{
    return MOUNT_POINT;
}

static bool ends_with(const char* s, size_t s_len, const char* suffix, size_t suf_len)
{
    return s_len > suf_len && std::strcmp(s + s_len - suf_len, suffix) == 0;
}

std::vector<std::string> PluginStorage::listPluginIds()
{
    std::vector<std::string> ids;
    if (!s_mounted) return ids;

    DIR* dir = opendir(MOUNT_POINT);
    if (!dir) {
        LOG_W(TAG, "opendir failed");
        return ids;
    }

    while (struct dirent* ent = readdir(dir)) {
        const char* name = ent->d_name;
        size_t len = std::strlen(name);
        std::string id;
        if (ends_with(name, len, ".aot", 4)) {
            id.assign(name, len - 4);
        } else if (ends_with(name, len, ".wasm", 5)) {
            id.assign(name, len - 5);
        } else {
            continue;
        }
        if (std::find(ids.begin(), ids.end(), id) != ids.end()) continue;

        std::string meta = metaPath(id);
        struct stat st;
        if (stat(meta.c_str(), &st) == 0 && (st.st_mode & S_IFREG)) {
            ids.push_back(std::move(id));
        }
    }

    closedir(dir);
    return ids;
}

std::string PluginStorage::binaryPath(const std::string& id)
{
    std::string aot = aotPath(id);
    struct stat st;
    if (stat(aot.c_str(), &st) == 0 && (st.st_mode & S_IFREG)) {
        return aot;
    }
    return wasmPath(id);
}

std::string PluginStorage::wasmPath(const std::string& id)
{
    return std::string(MOUNT_POINT) + "/" + id + ".wasm";
}

std::string PluginStorage::aotPath(const std::string& id)
{
    return std::string(MOUNT_POINT) + "/" + id + ".aot";
}

std::string PluginStorage::metaPath(const std::string& id)
{
    return std::string(MOUNT_POINT) + "/" + id + ".meta";
}

std::string PluginStorage::langPath(const std::string& id)
{
    return std::string(MOUNT_POINT) + "/" + id + ".lang";
}

bool PluginStorage::stats(uint64_t& free_bytes, uint64_t& total_bytes)
{
    if (!s_mounted) return false;
    return esp_vfs_fat_info(MOUNT_POINT, &total_bytes, &free_bytes) == ESP_OK;
}

}  // namespace cdc::plugin_manager
