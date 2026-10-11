/**
 * config.cpp - 配置管理实现
 */

#include "config.hpp"
#include "logging.hpp"
#include "filesystem.hpp"
#include <fstream>
#include <sstream>

namespace tensyoki {

bool Config::load(const std::string& filename) {
    config_file_ = filename;
    
    // 尝试从多个位置加载
    std::filesystem::path config_path;
    std::vector<std::filesystem::path> search_paths = {
        std::filesystem::current_path() / filename,
        VFS::instance().get_user_data_dir() / filename,
        "/etc/tensyoki/" / filename,
    };
    
    for (const auto& p : search_paths) {
        if (std::filesystem::exists(p)) {
            config_path = p;
            break;
        }
    }
    
    if (config_path.empty()) {
        log_info("Config file not found, creating default: {}", filename);
        // 创建默认配置
        set_defaults();
        return save(filename);
    }
    
    try {
        auto tbl = toml::parse_file(config_path.string());
        load_table(tbl);
        config_file_ = config_path;
        log_info("Loaded config from: {}", config_path.string());
        return true;
    } catch (const toml::parse_error& e) {
        log_error("Failed to parse config: {}", e.what());
        return false;
    }
}

void Config::load_table(const toml::table& tbl, const std::string& prefix) {
    for (const auto& [key, val] : tbl) {
        std::string full_key = prefix.empty() ? key : prefix + "." + key;
        
        if (val.is_table()) {
            load_table(*val.as_table(), full_key);
        } else if (val.is_array()) {
            // 数组暂时存为字符串 (JSON 格式)
            std::ostringstream oss;
            oss << val;
            values_[full_key] = oss.str();
        } else if (val.is_string()) {
            values_[full_key] = val.as_string()->get();
        } else if (val.is_integer()) {
            values_[full_key] = val.as_integer()->get();
        } else if (val.is_floating()) {
            values_[full_key] = val.as_floating()->get();
        } else if (val.is_boolean()) {
            values_[full_key] = val.as_boolean()->get();
        }
    }
}

bool Config::save(const std::string& filename) const {
    std::filesystem::path path = filename.empty() ? config_file_ : filename;
    if (path.empty()) {
        path = VFS::instance().get_user_data_dir() / "config.toml";
    }
    
    std::filesystem::create_directories(path.parent_path());
    
    toml::table tbl;
    save_table(tbl);
    
    try {
        std::ofstream ofs(path);
        ofs << tbl;
        log_info("Config saved to: {}", path.string());
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to save config: {}", e.what());
        return false;
    }
}

void Config::save_table(toml::table& tbl) const {
    std::shared_lock lock(mutex_);
    
    // 按前缀分组
    std::map<std::string, toml::table*> groups;
    
    for (const auto& [key, val] : values_) {
        // 解析键路径
        std::vector<std::string> parts;
        std::string part;
        for (char c : key) {
            if (c == '.') {
                parts.push_back(part);
                part.clear();
            } else {
                part += c;
            }
        }
        if (!part.empty()) parts.push_back(part);
        
        // 创建嵌套表
        toml::table* current = &tbl;
        for (size_t i = 0; i < parts.size() - 1; ++i) {
            auto& next = (*current)[parts[i]];
            if (!next.is_table()) {
                next = toml::table{};
            }
            current = next.as_table();
        }
        
        // 设置值
        const std::string& leaf = parts.back();
        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                (*current)[leaf] = v;
            } else if constexpr (std::is_same_v<T, int>) {
                (*current)[leaf] = v;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                (*current)[leaf] = v;
            } else if constexpr (std::is_same_v<T, double>) {
                (*current)[leaf] = v;
            } else if constexpr (std::is_same_v<T, float>) {
                (*current)[leaf] = v;
            } else if constexpr (std::is_same_v<T, std::string>) {
                (*current)[leaf] = v;
            } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
                (*current)[leaf] = v.string();
            }
        }, val);
    }
}

void Config::set_defaults() {
    // 窗口设置
    set("window.width", 1280);
    set("window.height", 720);
    set("window.title", "Tensyoki Linux Port");
    set("window.fullscreen", false);
    set("window.vsync", true);
    set("window.max_fps", 0);
    
    // 路径
    set("paths.data_root", std::filesystem::path{});
    set("paths.save_dir", std::filesystem::path{});
    
    // 游戏
    set("game.language", "ja");
    set("game.difficulty", "normal");
    
    // 音频
    set("audio.master_volume", 1.0f);
    set("audio.music_volume", 1.0f);
    set("audio.sfx_volume", 1.0f);
    set("audio.enabled", true);
    
    // 渲染
    set("renderer.backend", "opengl");
    set("renderer.msaa_samples", 0);
    set("renderer.debug_overlay", false);
    set("renderer.anisotropy", 16);
    
    // 输入
    set("input.mouse_sensitivity", 1.0f);
    set("input.invert_mouse", false);
    set("input.gamepad_deadzone", 0.2f);
    
    dirty_ = true;
}

std::string Config::to_toml_string() const {
    toml::table tbl;
    save_table(tbl);
    std::ostringstream oss;
    oss << tbl;
    return oss.str();
}

} // namespace tensyoki