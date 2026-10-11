/**
 * filesystem.hpp - 虚拟文件系统 (VFS)
 * 支持挂载多个目录、.bin 容器透明读取、大小写不敏感路径
 */

#pragma once

#include <filesystem>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <fstream>
#include <cstdint>

namespace tensyoki {

class FileHandle {
public:
    virtual ~FileHandle() = default;
    virtual ssize_t read(void* buffer, size_t size) = 0;
    virtual ssize_t write(const void* buffer, size_t size) = 0;
    virtual bool seek(int64_t offset, int whence = SEEK_SET) = 0;
    virtual int64_t tell() = 0;
    virtual int64_t size() = 0;
    virtual bool is_valid() const = 0;
    virtual void close() = 0;
};

class VFS {
public:
    // 获取单例
    static VFS& instance() {
        static VFS instance;
        return instance;
    }
    
    VFS(const VFS&) = delete;
    VFS& operator=(const VFS&) = delete;
    
    // 挂载目录到虚拟路径
    // 例如: mount("/path/to/game/data", "/") 将游戏数据目录挂载为根目录
    bool mount(const std::filesystem::path& physical_path, const std::string& virtual_path = "/");
    
    // 挂载 .bin 容器
    bool mount_bin(const std::filesystem::path& bin_path, const std::string& virtual_path = "/");
    
    // 卸载
    void unmount(const std::string& virtual_path);
    void unmount_all();
    
    // 打开文件读取
    std::unique_ptr<FileHandle> open_read(const std::string& virtual_path);
    
    // 打开文件写入 (仅限用户目录)
    std::unique_ptr<FileHandle> open_write(const std::string& virtual_path, bool append = false);
    
    // 检查文件是否存在
    bool exists(const std::string& virtual_path) const;
    
    // 获取文件大小
    int64_t file_size(const std::string& virtual_path) const;
    
    // 列出目录
    std::vector<std::string> list_directory(const std::string& virtual_path) const;
    
    // 解析虚拟路径到物理路径 (用于调试)
    std::optional<std::filesystem::path> resolve(const std::string& virtual_path) const;
    
    // 设置大小写敏感性 (模拟 Windows 不敏感)
    void set_case_sensitive(bool sensitive) { case_sensitive_ = sensitive; }
    
    // 获取用户数据目录 (存档、配置、日志)
    std::filesystem::path get_user_data_dir() const;
    
    // 获取缓存目录
    std::filesystem::path get_cache_dir() const;

private:
    VFS() = default;
    
    struct MountPoint {
        std::string virtual_path;
        std::filesystem::path physical_path;
        bool is_bin_container = false;
        int priority = 0;  // 优先级，高优先级先搜索
    };
    
    mutable std::shared_mutex mounts_mutex_;
    std::vector<MountPoint> mounts_;
    bool case_sensitive_ = false;  // 默认不敏感，模拟 Windows
    
    // 规范化路径 (统一分隔符、大小写)
    std::string normalize_path(const std::string& path) const;
    
    // 在挂载点中查找文件
    std::optional<std::filesystem::path> find_file(const std::string& virtual_path) const;
};

// 便利函数
inline std::unique_ptr<FileHandle> vfs_open_read(const std::string& path) {
    return VFS::instance().open_read(path);
}

inline std::unique_ptr<FileHandle> vfs_open_write(const std::string& path, bool append = false) {
    return VFS::instance().open_write(path, append);
}

inline bool vfs_exists(const std::string& path) {
    return VFS::instance().exists(path);
}

} // namespace tensyoki