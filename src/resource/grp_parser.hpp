/**
 * grp_parser.hpp - 光荣 .bin 容器内部图像资源解析器
 * 
 * 目标：解析 res_grp.bin 中的子文件，剥离头部/调色板，输出标准 RGBA8888 像素数组
 * 供 SDL3/OpenGL 直接创建纹理
 */

#pragma once

#include "bin_parser.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <span>
#include <optional>
#include <expected>
#include <memory>

namespace tensyoki {

// 前置声明
class GrpParser;

/// 解析后的图像数据
struct ImageData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;           // 内部格式标识
    std::vector<uint8_t> pixels;   // RGBA8888 数据 (width * height * 4)
    std::vector<uint8_t> palette;  // 调色板数据 (如果是索引色)
    bool has_alpha = false;
    bool is_compressed = false;
    
    // 获取行跨度
    size_t stride() const noexcept { return width * 4; }
    // 获取总像素字节数
    size_t pixel_size() const noexcept { return pixels.size(); }
};

/// grp 内部图像格式类型
enum class GrpImageFormat : uint32_t {
    Unknown = 0,
    RGBA8888 = 1,      // 32位直接色
    RGB565 = 2,        // 16位直接色
    Indexed4 = 3,      // 4位索引色 (16色调色板)
    Indexed8 = 4,      // 8位索引色 (256色调色板)
    DXT1 = 5,          // DXT1 压缩
    DXT3 = 6,          // DXT3 压缩
    DXT5 = 7,          // DXT5 压缩
    Custom = 0xFFFF    // 自定义/未知格式
};

/// grp 子文件头部结构 (基于光荣常见格式推测)
#pragma pack(push, 1)
struct GrpFileHeader {
    char magic[4];              // 如 "GRP ", "IMG ", "TEX " 等
    uint32_t version;           // 版本
    uint32_t width;             // 宽度
    uint32_t height;            // 高度
    uint32_t format;            // GrpImageFormat
    uint32_t flags;             // 标志位 (压缩、多级细节等)
    uint32_t mip_count;         // Mipmap 数量
    uint32_t palette_offset;    // 调色板偏移 (相对文件头)
    uint32_t palette_size;      // 调色板大小
    uint32_t pixel_offset;      // 像素数据偏移 (相对文件头)
    uint32_t pixel_size;        // 像素数据大小
    uint32_t reserved[4];
};
#pragma pack(pop)

/// 解析错误
enum class GrpParseError {
    InvalidHeader,
    UnsupportedFormat,
    CorruptedPalette,
    CorruptedPixels,
    DecompressionFailed,
    InvalidDimensions,
    BufferTooSmall
};

constexpr std::string_view to_string(GrpParseError err) noexcept {
    using E = GrpParseError;
    switch (err) {
        case E::InvalidHeader:       return "Invalid GRP header";
        case E::UnsupportedFormat:   return "Unsupported image format";
        case E::CorruptedPalette:    return "Corrupted palette data";
        case E::CorruptedPixels:     return "Corrupted pixel data";
        case E::DecompressionFailed: return "Decompression failed";
        case E::InvalidDimensions:   return "Invalid image dimensions";
        case E::BufferTooSmall:      return "Buffer too small";
    }
    return "Unknown GRP error";
}

/// 结果类型 (使用 bin_parser 的 Result 类型)
#include "bin_parser.hpp"
template<typename T>
using GrpResult = Result<T, GrpParseError>;

/// GRP 图像解析器
class GrpParser {
public:
    GrpParser() = default;
    ~GrpParser() = default;
    
    GrpParser(const GrpParser&) = delete;
    GrpParser& operator=(const GrpParser&) = delete;
    GrpParser(GrpParser&&) = default;
    GrpParser& operator=(GrpParser&&) = default;
    
    /// 从 bin_parser 提取的 span 解析单个图像
    GrpResult<ImageData> parse(std::span<const uint8_t> data) const;
    
    /// 批量解析：从 BinArchive 解析所有疑似图像条目
    std::vector<std::pair<BinFileEntry, ImageData>> parse_all(const BinArchive& archive) const;
    
    /// 检测数据是否为有效图像格式
    static bool is_valid_image(std::span<const uint8_t> data);
    
    /// 获取支持的格式列表
    static std::vector<GrpImageFormat> supported_formats();

private:
    // 内部解析方法
    GrpResult<ImageData> parse_rgba8888(std::span<const uint8_t> data, 
                                         const GrpFileHeader& header) const;
    GrpResult<ImageData> parse_rgb565(std::span<const uint8_t> data, 
                                       const GrpFileHeader& header) const;
    GrpResult<ImageData> parse_indexed4(std::span<const uint8_t> data, 
                                         const GrpFileHeader& header) const;
    GrpResult<ImageData> parse_indexed8(std::span<const uint8_t> data, 
                                         const GrpFileHeader& header) const;
    GrpResult<ImageData> parse_dxt(std::span<const uint8_t> data, 
                                    const GrpFileHeader& header,
                                    GrpImageFormat format) const;
    
    // 调色板解析
    GrpResult<std::vector<uint8_t>> parse_palette(std::span<const uint8_t> data,
                                                   const GrpFileHeader& header) const;
    
    // 像素解压/转换
    void apply_palette(const std::vector<uint8_t>& palette,
                        std::span<const uint8_t> src_indices,
                        std::vector<uint8_t>& dst_rgba,
                        uint32_t width, uint32_t height,
                        bool is_4bpp) const;
    
    // 工具
    static GrpImageFormat detect_format(std::span<const uint8_t> data);
    static uint32_t read_u32_le(std::span<const uint8_t> data, size_t offset);
    static uint16_t read_u16_le(std::span<const uint8_t> data, size_t offset);
};

/// 便利函数：从 BinArchive 条目直接解析图像
GrpResult<ImageData> parse_image_from_entry(const BinArchive& archive, 
                                             const BinFileEntry& entry);

/// 便利函数：尝试从任意数据推测并解析图像
GrpResult<ImageData> try_parse_image(std::span<const uint8_t> data);

} // namespace tensyoki