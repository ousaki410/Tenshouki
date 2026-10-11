/**
 * config.hpp - 配置管理 (基于 TOML)
 */

#pragma once

#include <toml.hpp>
#include <string>
#include <filesystem>
#include <optional>
#include <variant>
#include <unordered_map>
#include <mutex>

namespace tensyoki {

class Config {
public:
    // 获取单例
    static Config& instance() {
        static Config instance;
        return instance;
    }
    
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    // 加载配置文件
    bool load(const std::string& filename = "config.toml");
    
    // 保存配置文件
    bool save(const std::string& filename = "") const;
    
    // 获取值 (支持类型转换)
    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        std::shared_lock lock(mutex_);
        auto it = values_.find(key);
        if (it != values_.end()) {
            try {
                return std::get<T>(it->second);
            } catch (const std::bad_variant_access&) {
                // 类型不匹配，尝试转换
                return convert<T>(it->second);
            }
        }
        return default_value;
    }
    
    // 设置值
    template<typename T>
    void set(const std::string& key, const T& value) {
        std::unique_lock lock(mutex_);
        values_[key] = value;
        dirty_ = true;
    }
    
    // 检查键是否存在
    bool has(const std::string& key) const {
        std::shared_lock lock(mutex_);
        return values_.contains(key);
    }
    
    // 移除键
    void remove(const std::string& key) {
        std::unique_lock lock(mutex_);
        values_.erase(key);
        dirty_ = true;
    }
    
    // 获取所有键
    std::vector<std::string> keys() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        result.reserve(values_.size());
        for (const auto& [k, v] : values_) result.push_back(k);
        return result;
    }
    
    // 窗口设置快捷访问
    int get_window_width() const { return get("window.width", 1280); }
    int get_window_height() const { return get("window.height", 720); }
    std::string get_window_title() const { return get("window.title", "Tensyoki Linux Port"); }
    bool get_fullscreen() const { return get("window.fullscreen", false); }
    bool get_vsync() const { return get("window.vsync", true); }
    int get_max_fps() const { return get("window.max_fps", 0); }
    
    // 数据路径
    std::filesystem::path get_data_root() const { 
        return get("paths.data_root", std::filesystem::path{}); 
    }
    void set_data_root(const std::filesystem::path& path) { 
        set("paths.data_root", path); 
    }
    
    // 语言设置
    std::string get_language() const { return get("game.language", "ja"); }
    
    // 音频设置
    float get_master_volume() const { return get("audio.master_volume", 1.0f); }
    float get_music_volume() const { return get("audio.music_volume", 1.0f); }
    float get_sfx_volume() const { return get("audio.sfx_volume", 1.0f); }
    
    // 渲染设置
    std::string get_renderer_backend() const { return get("renderer.backend", "opengl"); }
    int get_msaa_samples() const { return get("renderer.msaa_samples", 0); }
    bool get_debug_overlay() const { return get("renderer.debug_overlay", false); }
    
    // 导出为 TOML 字符串 (用于调试)
    std::string to_toml_string() const;

private:
    Config() = default;
    
    using Value = std::variant<
        bool, int, int64_t, double, float, 
        std::string, std::filesystem::path
    >;
    
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Value> values_;
    mutable bool dirty_ = false;
    std::filesystem::path config_file_;
    
    // 从 TOML 表递归加载
    void load_table(const toml::table& tbl, const std::string& prefix = "");
    
    // 类型转换
    template<typename T>
    T convert(const Value& v) const {
        if constexpr (std::is_same_v<T, bool>) {
            if (std::holds_alternative<int>(v)) return std::get<int>(v) != 0;
            if (std::holds_alternative<std::string>(v)) {
                auto s = std::get<std::string>(v);
                return s == "true" || s == "1" || s == "yes" || s == "on";
            }
        } else if constexpr (std::is_same_v<T, int>) {
            if (std::holds_alternative<int64_t>(v)) return static_cast<int>(std::get<int64_t>(v));
            if (std::holds_alternative<double>(v)) return static_cast<int>(std::get<double>(v));
            if (std::holds_alternative<float>(v)) return static_cast<int>(std::get<float>(v));
            if (std::holds_alternative<std::string>(v)) return std::stoi(std::get<std::string>(v));
        } else if constexpr (std::is_same_v<T, float>) {
            if (std::holds_alternative<double>(v)) return static_cast<float>(std::get<double>(v));
            if (std::holds_alternative<int>(v)) return static_cast<float>(std::get<int>(v));
            if (std::holds_alternative<int64_t>(v)) return static_cast<float>(std::get<int64_t>(v));
            if (std::holds_alternative<std::string>(v)) return std::stof(std::get<std::string>(v));
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (std::holds_alternative<std::filesystem::path>(v)) 
                return std::get<std::filesystem::path>(v).string();
            if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
            if (std::holds_alternative<double>(v)) return std::to_string(std::get<double>(v));
        } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            if (std::holds_alternative<std::string>(v)) 
                return std::filesystem::path(std::get<std::string>(v));
        }
        return T{};
    }
    
    // 保存到 TOML 表
    void save_table(toml::table& tbl) const;
};

} // namespace tensyoki