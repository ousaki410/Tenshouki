/**
 * bin_parser.hpp - 光荣游戏通用 .bin 容器解析器
 * 
 * 文件格式:
 * - Magic: "LINK" (4 bytes)
 * - Index Table: 子文件数量 + 每个条目(偏移量, 大小, 文件名哈希等)
 * - Data Blocks: 实际文件数据
 */

#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>
#include <span>
#include <optional>
#include <string_view>
#include <memory>
#include <utility>
#include <stdexcept>
#include <fstream>
#include <variant>

namespace tensyoki {

// 前置声明
class BinArchive;

/// .bin 容器中的单个文件条目
struct BinFileEntry {
    std::string name;           // 文件名(如果可用)
    uint32_t name_hash = 0;     // 文件名哈希
    uint32_t offset = 0;        // 相对于数据区起始的偏移量
    uint32_t size = 0;          // 文件大小(字节)
    uint32_t flags = 0;         // 标志位(压缩、加密等)
    uint32_t type = 0;          // 资源类型
    
    /// 获取文件扩展名推测
    std::string guess_extension() const;
};

/// 解析错误类型
enum class BinParseError {
    FileNotFound,
    FileTooSmall,
    InvalidMagic,
    InvalidVersion,
    CorruptedIndex,
    CorruptedData,
    IOError,
    UnsupportedFormat
};

/// 错误信息转字符串
constexpr std::string_view to_string(BinParseError err) noexcept {
    using E = BinParseError;
    switch (err) {
        case E::FileNotFound:    return "File not found";
        case E::FileTooSmall:    return "File too small for valid header";
        case E::InvalidMagic:    return "Invalid magic number (expected 'LINK')";
        case E::InvalidVersion:  return "Unsupported format version";
        case E::CorruptedIndex:  return "Corrupted index table";
        case E::CorruptedData:   return "Corrupted data block";
        case E::IOError:         return "I/O error";
        case E::UnsupportedFormat: return "Unsupported format variant";
    }
    return "Unknown error";
}

/// C++20 兼容的 Result 类型 (类似 std::expected)
/// 使用 std::variant 处理 void 类型 (用 std::monostate 代替 void)
template<typename T, typename E>
class Result {
public:
    // 禁止默认构造
    Result() = delete;
    
    // 成功构造
    template<typename U = T>
    constexpr Result(U&& value) requires (!std::is_void_v<U>) 
        : storage_(std::in_place_index<0>, std::forward<U>(value)) {}
    
    // 错误构造
    constexpr Result(E&& error) : storage_(std::in_place_index<1>, std::move(error)) {}
    constexpr Result(const E& error) : storage_(std::in_place_index<1>, error) {}
    
    // void 类型的特化构造
    template<typename U = T>
    constexpr Result(std::nullptr_t) requires (std::is_void_v<U>) 
        : storage_(std::in_place_index<0>, std::monostate{}) {}
    
    // 移动构造/赋值
    constexpr Result(Result&& other) noexcept = default;
    constexpr Result& operator=(Result&& other) noexcept = default;
    
    // 禁止拷贝
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    
    // 状态查询
    constexpr explicit operator bool() const noexcept { return storage_.index() == 0; }
    constexpr bool has_value() const noexcept { return storage_.index() == 0; }
    constexpr bool has_error() const noexcept { return storage_.index() == 1; }
    
    // 值访问
    template<typename U = T>
    constexpr U& value() & requires (!std::is_void_v<U>) { 
        if (!has_value()) throw std::runtime_error("Result has no value"); 
        return std::get<0>(storage_); 
    }
    template<typename U = T>
    constexpr const U& value() const& requires (!std::is_void_v<U>) { 
        if (!has_value()) throw std::runtime_error("Result has no value"); 
        return std::get<0>(storage_); 
    }
    template<typename U = T>
    constexpr U&& value() && requires (!std::is_void_v<U>) { 
        if (!has_value()) throw std::runtime_error("Result has no value"); 
        return std::move(std::get<0>(storage_)); 
    }
    
    // void 类型的 value()
    template<typename U = T>
    constexpr void value() requires (std::is_void_v<U>) { 
        if (!has_value()) throw std::runtime_error("Result has no value"); 
    }
    template<typename U = T>
    constexpr void value() const requires (std::is_void_v<U>) { 
        if (!has_value()) throw std::runtime_error("Result has no value"); 
    }
    
    // 错误访问
    constexpr E& error() & { 
        if (has_value()) throw std::runtime_error("Result has no error"); 
        return std::get<1>(storage_); 
    }
    constexpr const E& error() const& { 
        if (has_value()) throw std::runtime_error("Result has no error"); 
        return std::get<1>(storage_); 
    }
    
    // 运算符重载 (仅非 void)
    template<typename U = T>
    constexpr U* operator->() & requires (!std::is_void_v<U>) { return has_value() ? &std::get<0>(storage_) : nullptr; }
    template<typename U = T>
    constexpr const U* operator->() const requires (!std::is_void_v<U>) { return has_value() ? &std::get<0>(storage_) : nullptr; }
    
    template<typename U = T>
    constexpr U& operator*() & requires (!std::is_void_v<U>) { return value(); }
    template<typename U = T>
    constexpr const U& operator*() const& requires (!std::is_void_v<U>) { return value(); }
    
    // 错误构造辅助
    static constexpr Result success() requires (std::is_void_v<T>) { return Result(nullptr); }
    template<typename U = T>
    static constexpr Result success(U&& val) requires (!std::is_void_v<U>) { return Result(std::forward<U>(val)); }
    static constexpr Result failure(E&& err) { return Result(std::move(err)); }
    static constexpr Result failure(const E& err) { return Result(err); }

private:
    // variant index 0 = value (T or monostate for void), index 1 = error
    std::variant<std::conditional_t<std::is_void_v<T>, std::monostate, T>, E> storage_;
};

/// 结果类型别名
template<typename T>
using BinResult = Result<T, BinParseError>;

/// .bin 容器主类
class BinArchive {
public:
    BinArchive() = default;
    ~BinArchive() = default;
    
    // 禁止拷贝(大数据)，允许移动
    BinArchive(const BinArchive&) = delete;
    BinArchive& operator=(const BinArchive&) = delete;
    BinArchive(BinArchive&&) = default;
    BinArchive& operator=(BinArchive&&) = default;
    
    /// 从文件加载并解析
    BinResult<void> load_from_file(const std::filesystem::path& path);
    
    /// 从内存缓冲区加载
    BinResult<void> load_from_memory(std::span<const uint8_t> data);
    
    /// 获取所有文件条目
    const std::vector<BinFileEntry>& entries() const noexcept { return entries_; }
    
    /// 根据名称查找条目
    const BinFileEntry* find_entry(std::string_view name) const noexcept;
    
    /// 根据哈希查找条目
    const BinFileEntry* find_entry_by_hash(uint32_t hash) const noexcept;
    
    /// 提取文件数据到 vector
    BinResult<std::vector<uint8_t>> extract(const BinFileEntry& entry) const;
    
    /// 提取文件数据到预分配缓冲区
    BinResult<void> extract_to(const BinFileEntry& entry, std::vector<uint8_t>& out_buffer) const;
    
    /// 获取文件数据的只读视图(零拷贝，需保证 archive 存活)
    BinResult<std::span<const uint8_t>> view(const BinFileEntry& entry) const;
    
    /// 导出所有文件到目录
    BinResult<void> extract_all(const std::filesystem::path& output_dir) const;
    
    /// 获取容器信息摘要
    struct Info {
        std::string source_path;
        uint32_t version = 0;
        uint32_t file_count = 0;
        uint64_t total_size = 0;
        uint64_t index_table_offset = 0;
        uint64_t data_offset = 0;
    };
    Info info() const noexcept;
    
    /// 清理状态
    void clear() noexcept;

    // 公开头部结构供外部检查
    #pragma pack(push, 1)
    struct Header {
        char magic[4];           // "LINK"
        uint32_t version;        // 版本号
        uint32_t file_count;     // 文件数量
        uint32_t index_offset;   // 索引表偏移(相对文件头)
        uint32_t index_size;     // 索引表大小
        uint32_t data_offset;    // 数据区偏移(相对文件头)
        uint32_t flags;          // 容器标志
        uint32_t reserved[4];    // 保留字段
    };
    
    struct IndexEntry {
        uint32_t name_hash;      // 文件名哈希
        uint32_t offset;         // 相对数据区偏移
        uint32_t size;           // 文件大小
        uint32_t flags;          // 标志位
        uint32_t type;           // 资源类型
        uint32_t reserved;       // 保留
    };
    #pragma pack(pop)
    
    static constexpr uint32_t MAGIC_LINK = 0x4B4E494C; // "LINK" little-endian
    static constexpr uint32_t SUPPORTED_VERSION_MIN = 1;
    static constexpr uint32_t SUPPORTED_VERSION_MAX = 3;
    
    // 内部解析方法
    BinResult<void> parse_header(std::span<const uint8_t> data);
    BinResult<void> parse_index_table(std::span<const uint8_t> data);
    BinResult<void> validate_data_blocks() const;
    
    // 工具方法
    static uint32_t read_u32_le(std::span<const uint8_t> data, size_t offset);
    static void write_u32_le(std::vector<uint8_t>& data, size_t offset, uint32_t value);

private:
    std::filesystem::path source_path_;
    std::vector<uint8_t> file_data_;           // 完整文件内容(内存映射或读入)
    std::vector<BinFileEntry> entries_;        // 解析出的文件列表
    Header header_{};
    bool loaded_ = false;
};

/// 便利函数：快速检查是否为有效 .bin 文件
inline BinResult<bool> is_valid_bin_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return BinResult<bool>::failure(BinParseError::FileNotFound);
    }
    if (std::filesystem::file_size(path) < sizeof(BinArchive::Header)) {
        return BinResult<bool>::failure(BinParseError::FileTooSmall);
    }
    
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return BinResult<bool>::failure(BinParseError::IOError);
    }
    char magic[4];
    file.read(magic, 4);
    if (std::string_view(magic, 4) != "LINK") {
        return BinResult<bool>::failure(BinParseError::InvalidMagic);
    }
    return BinResult<bool>::success(true);
}

} // namespace tensyoki