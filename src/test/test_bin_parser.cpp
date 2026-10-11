/**
 * test_bin_parser.cpp - 单元测试
 */

#include "gtest/gtest.h"
#include "resource/bin_parser.hpp"
#include "core/filesystem.hpp"
#include "core/logging.hpp"
#include <filesystem>
#include <fstream>

namespace tensyoki {
namespace test {

class BinParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logging::init("test.log", spdlog::level::off, spdlog::level::debug);
        
        // 创建临时测试文件
        test_dir_ = std::filesystem::temp_directory_path() / "tensyoki_test";
        std::filesystem::create_directories(test_dir_);
        
        // 创建测试用 .bin 文件
        create_test_bin();
    }
    
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
        Logging::shutdown();
    }
    
    void create_test_bin() {
        test_bin_path_ = test_dir_ / "test.bin";
        std::ofstream ofs(test_bin_path_, std::ios::binary);
        
        // 写入 LINK 头部
        uint32_t magic = 0x4B4E494C;  // "LINK"
        uint32_t version = 1;
        uint32_t flags = 0;
        uint32_t reserved = 0;
        uint32_t index_offset = 32;  // 头部后
        uint32_t index_count = 2;
        uint32_t data_offset = 32 + 2 * 24;  // 头部 + 2个索引
        uint32_t data_size = 100;
        
        ofs.write(reinterpret_cast<char*>(&magic), 4);
        ofs.write(reinterpret_cast<char*>(&version), 4);
        ofs.write(reinterpret_cast<char*>(&flags), 4);
        ofs.write(reinterpret_cast<char*>(&reserved), 4);
        ofs.write(reinterpret_cast<char*>(&index_offset), 4);
        ofs.write(reinterpret_cast<char*>(&index_count), 4);
        ofs.write(reinterpret_cast<char*>(&data_offset), 4);
        ofs.write(reinterpret_cast<char*>(&data_size), 4);
        
        // 写入索引表 (2个条目)
        // 条目1: name_hash=0x12345678, offset=0, size=50, flags=0, type=1(Texture)
        uint32_t entry1[] = {0x12345678, 0, 50, 0, 1, 0, 0};
        ofs.write(reinterpret_cast<char*>(entry1), 24);
        
        // 条目2: name_hash=0x87654321, offset=50, size=50, flags=0, type=3(Shader)
        uint32_t entry2[] = {0x87654321, 50, 50, 0, 3, 0, 0};
        ofs.write(reinterpret_cast<char*>(entry2), 24);
        
        // 写入数据区 (100 字节)
        std::vector<char> data(100, 'A');
        for (int i = 0; i < 50; ++i) data[i] = 'T';  // Texture data
        for (int i = 50; i < 100; ++i) data[i] = 'S'; // Shader data
        ofs.write(data.data(), 100);
    }
    
    std::filesystem::path test_dir_;
    std::filesystem::path test_bin_path_;
};

TEST_F(BinParserTest, ParseValidBin) {
    BinParser parser;
    ASSERT_TRUE(parser.parse(test_bin_path_));
    
    auto resources = parser.get_all_resources();
    ASSERT_EQ(resources.size(), 2);
    
    EXPECT_EQ(resources[0].name_hash, 0x12345678);
    EXPECT_EQ(resources[0].offset, 32 + 48 + 0);  // data_offset + 0
    EXPECT_EQ(resources[0].size, 50);
    EXPECT_EQ(resources[0].type, BinParser::ResourceType::Texture);
    
    EXPECT_EQ(resources[1].name_hash, 0x87654321);
    EXPECT_EQ(resources[1].offset, 32 + 48 + 50);
    EXPECT_EQ(resources[1].size, 50);
    EXPECT_EQ(resources[1].type, BinParser::ResourceType::Shader);
}

TEST_F(BinParserTest, ExtractResource) {
    BinParser parser;
    ASSERT_TRUE(parser.parse(test_bin_path_));
    
    auto resources = parser.get_all_resources();
    ASSERT_EQ(resources.size(), 2);
    
    std::vector<uint8_t> data;
    ASSERT_TRUE(parser.extract_resource(resources[0], data));
    ASSERT_EQ(data.size(), 50);
    for (uint8_t b : data) EXPECT_EQ(b, 'T');
    
    ASSERT_TRUE(parser.extract_resource(resources[1], data));
    ASSERT_EQ(data.size(), 50);
    for (uint8_t b : data) EXPECT_EQ(b, 'S');
}

TEST_F(BinParserTest, ExtractAll) {
    BinParser parser;
    ASSERT_TRUE(parser.parse(test_bin_path_));
    
    auto out_dir = test_dir_ / "extracted";
    ASSERT_TRUE(parser.extract_all(out_dir));
    
    ASSERT_TRUE(std::filesystem::exists(out_dir / "resource_12345678"));
    ASSERT_TRUE(std::filesystem::exists(out_dir / "resource_87654321"));
}

TEST_F(BinParserTest, FindResource) {
    BinParser parser;
    ASSERT_TRUE(parser.parse(test_bin_path_));
    
    // 默认名称是哈希值
    const auto* res = parser.find_resource("resource_12345678");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->name_hash, 0x12345678);
    
    // 不存在的资源
    EXPECT_EQ(parser.find_resource("nonexistent"), nullptr);
}

TEST_F(BinParserTest, FilterByType) {
    BinParser parser;
    ASSERT_TRUE(parser.parse(test_bin_path_));
    
    auto textures = parser.get_resources_by_type(BinParser::ResourceType::Texture);
    ASSERT_EQ(textures.size(), 1);
    EXPECT_EQ(textures[0].name_hash, 0x12345678);
    
    auto shaders = parser.get_resources_by_type(BinParser::ResourceType::Shader);
    ASSERT_EQ(shaders.size(), 1);
    EXPECT_EQ(shaders[0].name_hash, 0x87654321);
}

TEST_F(BinParserTest, InvalidMagic) {
    // 创建无效魔数文件
    auto bad_path = test_dir_ / "bad.bin";
    std::ofstream ofs(bad_path, std::ios::binary);
    uint32_t bad_magic = 0xDEADBEEF;
    ofs.write(reinterpret_cast<char*>(&bad_magic), 4);
    
    BinParser parser;
    EXPECT_FALSE(parser.parse(bad_path));
}

TEST_F(BinParserTest, EmptyFile) {
    auto empty_path = test_dir_ / "empty.bin";
    std::ofstream ofs(empty_path, std::ios::binary);
    
    BinParser parser;
    EXPECT_FALSE(parser.parse(empty_path));
}

} // namespace test
} // namespace tensyoki

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}