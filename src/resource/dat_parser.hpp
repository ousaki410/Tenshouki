/**
 * dat_parser.hpp - 光荣游戏通用 .DAT 结构体数组数据解析器
 * 
 * 适用于武将、城池、道具等固定大小结构体数组的 DAT 文件。
 */

#pragma once

#include "bin_parser.hpp"
#include "logging.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <span>
#include <optional>
#include <algorithm>
#include <type_traits>

namespace tensyoki {

/// DAT 解析错误类型 (复用或扩展 BinParseError)
enum class DatParseError {
    FileNotFound,
    FileTooSmall,
    InvalidAlignment,
    OutOfRange,
    IOError,
    CorruptedData
};

constexpr std::string_view to_string(DatParseError err) noexcept {
    using E = DatParseError;
    switch (err) {
        case E::FileNotFound:     return "DAT file not found";
        case E::FileTooSmall:     return "DAT file too small for record structure";
        case E::InvalidAlignment: return "DAT file size does not align with record size";
        case E::OutOfRange:       return "Record index out of range";
        case E::IOError:          return "DAT I/O error";
        case E::CorruptedData:    return "Corrupted DAT data";
    }
    return "Unknown DAT error";
}

template<typename T>
using DatResult = Result<T, DatParseError>;

/// 示例武将记录结构体 (64 字节对齐)
#pragma pack(push, 1)
struct OfficerRecord {
    uint16_t id;              // 武将内部 ID
    uint16_t portrait_id;     // 头像 ID
    uint8_t  status;          // 状态 (在野、仕官、死亡等)
    uint8_t  loyalty;         // 忠诚度
    uint8_t  leadership;      // 统率
    uint8_t  warfare;         // 武力
    uint8_t  wisdom;          // 智力
    uint8_t  politics;        // 政治
    uint8_t  ambition;        // 野心
    uint8_t  charm;           // 魅力
    char     name[20];        // 姓名 (固定长度，可能含 Shift-JIS 编码)
    char     kana[20];        // 假名/字号
    uint8_t  reserved[6];     // 保留字段以凑齐 64 字节
};
#pragma pack(pop)

static_assert(sizeof(OfficerRecord) == 64, "OfficerRecord size must be exactly 64 bytes");

/// 通用 DAT 结构体数组容器解析器
template<typename RecordType>
class DatArchive {
    static_assert(std::is_trivially_copyable_v<RecordType>, "RecordType must be trivially copyable");
    static_assert(std::is_standard_layout_v<RecordType>, "RecordType must be standard layout");

public:
    DatArchive() = default;
    ~DatArchive() = default;

    DatArchive(const DatArchive&) = delete;
    DatArchive& operator=(const DatArchive&) = delete;
    DatArchive(DatArchive&&) = default;
    DatArchive& operator=(DatArchive&&) = default;

    /// 从内存 Span 加载解析
    DatResult<void> load_from_memory(std::span<const uint8_t> data) {
        clear();

        if (data.empty()) {
            LOG_ERROR("DAT data span is empty");
            return DatResult<void>::failure(DatParseError::FileTooSmall);
        }

        constexpr size_t record_size = sizeof(RecordType);
        if (data.size() < record_size) {
            LOG_ERROR("DAT data too small: {} bytes (need at least {})", data.size(), record_size);
            return DatResult<void>::failure(DatParseError::FileTooSmall);
        }

        if (data.size() % record_size != 0) {
            LOG_WARN("DAT file size ({} bytes) is not a multiple of record size ({} bytes). Extra trailing bytes will be ignored.",
                     data.size(), record_size);
        }

        size_t count = data.size() / record_size;
        raw_data_.assign(data.begin(), data.end());
        
        // 建立记录视图
        records_ = std::span<const RecordType>(
            reinterpret_cast<const RecordType*>(raw_data_.data()), 
            count
        );

        LOG_INFO("Successfully loaded {} records of size {} bytes (total {} bytes)",
                 count, record_size, raw_data_.size());
        return DatResult<void>::success();
    }

    /// 从 BinArchive 的特定条目加载
    DatResult<void> load_from_archive(const BinArchive& archive, const BinFileEntry& entry) {
        auto view_result = archive.view(entry);
        if (!view_result) {
            LOG_ERROR("Failed to view entry '{}' from archive for DAT loading", entry.name);
            return DatResult<void>::failure(DatParseError::CorruptedData);
        }
        return load_from_memory(*view_result);
    }

    /// 获取全部记录的只读视图
    std::span<const RecordType> records() const noexcept { return records_; }

    /// 获取记录数量
    size_t count() const noexcept { return records_.size(); }

    /// 是否已加载
    bool is_loaded() const noexcept { return !records_.empty(); }

    /// 根据索引获取记录
    DatResult<const RecordType*> get(size_t index) const noexcept {
        if (index >= records_.size()) {
            LOG_ERROR("DAT record index out of range: {} >= {}", index, records_.size());
            return DatResult<const RecordType*>::failure(DatParseError::OutOfRange);
        }
        return DatResult<const RecordType*>::success(&records_[index]);
    }

    /// 根据 ID 检索记录 (假设 RecordType 具有 id 字段)
    template<typename T = RecordType>
    auto find_by_id(uint16_t id) const -> std::optional<RecordType> requires requires(T t) { t.id; } {
        auto it = std::find_if(records_.begin(), records_.end(), [id](const RecordType& r) {
            return r.id == id;
        });
        if (it != records_.end()) {
            return *it;
        }
        return std::nullopt;
    }

    /// 清空数据
    void clear() noexcept {
        raw_data_.clear();
        records_ = {};
    }

private:
    std::vector<uint8_t> raw_data_;
    std::span<const RecordType> records_;
};

} // namespace tensyoki
