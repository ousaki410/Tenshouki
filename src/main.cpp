/**
 * main.cpp - 天翔记 Linux 移植版入口点
 */

#include "core/config.hpp"
#include "core/filesystem.hpp"
#include "core/logging.hpp"
#include "core/math/math_utils.hpp"
#include "render/renderer.hpp"
#include "resource/bin_parser.hpp"
#include "ui/imgui_backend.hpp"
#include "platform/linux/platform_linux.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <memory>
#include <filesystem>

namespace tensyoki {

class Application {
public:
    Application(int argc, char* argv[]) : argc_(argc), argv_(argv) {}
    
    int run() {
        if (!init()) {
            return 1;
        }
        
        main_loop();
        
        shutdown();
        return 0;
    }

private:
    int argc_;
    char** argv_;
    
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<ImGuiBackend> imgui_;
    std::unique_ptr<Platform> platform_;
    std::filesystem::path data_root_;
    
    bool init() {
        // 初始化日志
        Logging::init("tensyoki.log");
        log_info("=== Tensyoki Linux Port ===");
        log_info("Version: 0.1.0-dev");
        log_info("Build: {}", __DATE__ " " __TIME__);
        
        // 解析命令行参数获取数据路径
        parse_args();
        
        // 初始化 VFS
        if (!VFS::instance().mount(data_root_, "/")) {
            log_error("Failed to mount data root: {}", data_root_.string());
            return false;
        }
        
        // 加载配置
        Config::instance().load("config.toml");
        
        // 初始化平台抽象
        platform_ = Platform::create();
        if (!platform_->init(Config::instance().get_window_width(), 
                            Config::instance().get_window_height(),
                            Config::instance().get_window_title())) {
            log_error("Platform initialization failed");
            return false;
        }
        
        // 初始化渲染器
        renderer_ = std::make_unique<Renderer>();
        if (!renderer_->init(platform_->get_window_handle())) {
            log_error("Renderer initialization failed");
            return false;
        }
        
        // 初始化 ImGui
        imgui_ = std::make_unique<ImGuiBackend>();
        if (!imgui_->init(platform_->get_window_handle(), renderer_->get_device())) {
            log_error("ImGui initialization failed");
            return false;
        }
        
        // 测试资源解析
        test_resource_parsing();
        
        log_info("Initialization complete");
        return true;
    }
    
    void parse_args() {
        // 优先级: 命令行 > 环境变量 > 配置文件 > 自动检测
        data_root_ = Config::instance().get_data_root();
        
        if (argc_ > 1) {
            data_root_ = argv_[1];
        } else if (const char* env = std::getenv("TENSYOKI_DATA_ROOT")) {
            data_root_ = env;
        } else {
            // 自动检测常见路径
            std::vector<std::filesystem::path> candidates = {
                std::filesystem::current_path() / "data",
                std::filesystem::current_path() / "NOBUNAGA'S AMBITION Tenshouki with Power Up Kit HD version",
                "/usr/share/tensyoki/data",
                std::filesystem::path(std::getenv("HOME")) / ".local/share/tensyoki/data",
            };
            
            for (const auto& p : candidates) {
                if (std::filesystem::exists(p / "res" / "res_grp.bin")) {
                    data_root_ = p;
                    break;
                }
            }
        }
        
        if (data_root_.empty() || !std::filesystem::exists(data_root_)) {
            log_error("Game data not found. Please specify data root path.");
            log_error("Usage: tensyoki [data_root_path]");
            log_error("Or set TENSYOKI_DATA_ROOT environment variable");
            data_root_ = ".";  // 允许继续运行以显示错误 UI
        } else {
            log_info("Data root: {}", data_root_.string());
        }
    }
    
    void test_resource_parsing() {
        log_info("Testing resource parsing...");
        
        // 测试解析 shader.bin
        BinParser shader_parser;
        if (shader_parser.parse("res/res_shader.bin")) {
            log_info("res_shader.bin: {} resources", shader_parser.get_all_resources().size());
            shader_parser.dump_info(log_info_stream());
        }
        
        // 测试解析 else.bin
        BinParser else_parser;
        if (else_parser.parse("res/res_else.bin")) {
            log_info("res_else.bin: {} resources", else_parser.get_all_resources().size());
        }
        
        // 测试解析 grp.bin (大文件，可能只解析索引)
        BinParser grp_parser;
        if (grp_parser.parse("res/res_grp.bin")) {
            log_info("res_grp.bin: {} resources", grp_parser.get_all_resources().size());
        }

        // 缓存 grp 解析器供 UI 使用（延迟加载避免启动卡顿）
        grp_parser_ = std::make_unique<BinParser>();
        if (grp_parser_->parse("res/res_grp.bin")) {
            log_info("Cached res_grp.bin: {} resources", grp_parser_->get_all_resources().size());
        }
    }
    
    void main_loop() {
        bool running = true;
        uint64_t frame_count = 0;
        auto last_time = std::chrono::high_resolution_clock::now();
        
        while (running) {
            // 处理事件
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                imgui_->process_event(&event);
                
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.key == SDLK_ESCAPE) {
                        running = false;
                    } else if (event.key.key == SDLK_F11) {
                        platform_->toggle_fullscreen();
                    }
                } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                    renderer_->on_resize(event.window.data1, event.window.data2);
                }
            }
            
            // 计算帧时间
            auto current_time = std::chrono::high_resolution_clock::now();
            float delta_time = std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;
            
            // 开始新帧
            imgui_->new_frame(delta_time);
            renderer_->begin_frame();
            
            // 渲染 UI
            render_ui();
            
            // 结束帧
            renderer_->end_frame();
            imgui_->render();
            platform_->swap_buffers();
            
            frame_count++;
            
            // FPS 统计
            if (frame_count % 60 == 0) {
                auto now = std::chrono::high_resolution_clock::now();
                float elapsed = std::chrono::duration<float>(now - last_time).count();
                if (elapsed > 0) {
                    log_debug("FPS: {:.1f}", 60.0f / elapsed);
                }
            }
        }
    }
    
    void render_ui() {
        // 主菜单演示
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Data Root...")) {
                    // TODO: 文件选择对话框
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Quit", "Esc")) {
                    platform_->request_quit();
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Debug")) {
                ImGui::MenuItem("Resource Browser", nullptr, &show_resource_browser_);
                ImGui::MenuItem("Shader Viewer", nullptr, &show_shader_viewer_);
                ImGui::MenuItem("Data Table Viewer", nullptr, &show_dat_viewer_);
                ImGui::EndMenu();
            }
            
            ImGui::EndMainMenuBar();
        }
        
        // 资源浏览器
        if (show_resource_browser_) {
            render_resource_browser();
        }
        
        // 状态栏
        ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::SetWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 25));
        ImGui::SetWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 25));
        ImGui::Text("Tensyoki Linux Port | Data: %s | FPS: %.1f", 
                    data_root_.string().c_str(), ImGui::GetIO().Framerate);
        ImGui::End();
    }
    
    void render_resource_browser() {
        ImGui::Begin("Resource Browser", &show_resource_browser_, ImGuiWindowFlags_MenuBar);
        
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Extract Selected")) {
                    if (selected_entry_ && selected_parser_) {
                        auto result = selected_parser_->extract(*selected_entry_);
                        if (result) {
                            log_info("Extracted '{}' ({} bytes)", selected_entry_->name, result->size());
                        } else {
                            log_error("Extract failed: {}", tensyoki::to_string(result.error()));
                        }
                    }
                }
                if (ImGui::MenuItem("Extract All")) {
                    if (selected_parser_) {
                        selected_parser_->extract_all("extracted/");
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Show Hex Dump", nullptr, &show_hex_dump_);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        
        ImGui::BeginChild("LeftPane", ImVec2(350, 0), true);
        
        // Container selector
        static std::string current_file = "res/res_grp.bin";
        if (ImGui::BeginCombo("Container", current_file.c_str())) {
            const char* files[] = {
                "res/res_shader.bin",
                "res/res_else.bin", 
                "res/res_grp.bin",
                "res/res_snd.bin",
                "res/res_exp.bin",
                "res/res_exp_pc.bin"
            };
            for (const char* f : files) {
                bool selected = (current_file == f);
                if (ImGui::Selectable(f, selected)) {
                    current_file = f;
                    if (!grp_parser_ || current_file == "res/res_grp.bin") {
                        if (!grp_parser_) grp_parser_ = std::make_unique<BinParser>();
                        if (grp_parser_->parse(current_file)) {
                            log_info("Parsed {}: {} resources", current_file, grp_parser_->get_all_resources().size());
                        }
                        selected_parser_ = grp_parser_.get();
                    }
                    selected_entry_ = nullptr;
                }
            }
            ImGui::EndCombo();
        }
        
        ImGui::Separator();
        
        if (selected_parser_) {
            ImGui::Text("Entries: %zu", selected_parser_->entries().size());
            ImGui::SameLine();
            ImGui::Text("(%s)", current_file.c_str());
        }
        ImGui::Separator();
        
        static ImGuiTextFilter filter;
        filter.Draw("Filter##res_filter", -100.0f);
        
        // Tree view of entries
        if (selected_parser_) {
            for (const auto& entry : selected_parser_->entries()) {
                if (!filter.PassFilter(entry.name.c_str())) continue;
                
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (selected_entry_ == &entry) flags |= ImGuiTreeNodeFlags_Selected;
                
                bool opened = ImGui::TreeNodeEx(entry.name.c_str(), flags);
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    selected_entry_ = &entry;
                    selected_parser_ = selected_parser_;
                }
                if (opened) ImGui::TreePop();
            }
        }
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        // Right pane: Details / Hex Dump
        ImGui::BeginChild("RightPane", ImVec2(0, 0), true);
        
        if (selected_entry_ && selected_parser_) {
            render_entry_details(*selected_entry_, *selected_parser_);
        } else {
            ImGui::Text("Select a resource from the list");
            ImGui::TextDisabled("Use the filter to narrow down entries");
        }
        ImGui::EndChild();
        
        ImGui::End();
    }
    
    void render_entry_details(const BinFileEntry& entry, const BinParser& parser) {
        ImGui::Text("Name: %s", entry.name.c_str());
        ImGui::Text("Hash: 0x%08X", entry.name_hash);
        ImGui::Text("Offset: 0x%08X (%u)", entry.offset, entry.offset);
        ImGui::Text("Size: 0x%08X (%u bytes)", entry.size, entry.size);
        ImGui::Text("Type: %u", entry.type);
        ImGui::Text("Flags: 0x%04X", entry.flags);
        ImGui::Text("Extension: %s", entry.guess_extension().c_str());
        
        ImGui::Separator();
        
        // Action buttons
        if (ImGui::Button("Extract")) {
            auto result = parser.extract(entry);
            if (result) {
                log_info("Extracted '{}' ({} bytes)", entry.name, result->size());
            } else {
                log_error("Extract failed: {}", tensyoki::to_string(result.error()));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("View Hex Dump")) {
            show_hex_dump_ = true;
        }
        
        ImGui::Separator();
        
        // Hex Dump
        if (show_hex_dump_) {
            ImGui::Text("Hex Dump (first 256 bytes):");
            ImGui::SameLine();
            ImGui::Checkbox("Show All", &show_full_hex_);
            
            auto view_result = parser.view(entry);
            if (view_result) {
                const auto& data = *view_result;
                size_t dump_size = show_full_hex_ ? data.size() : std::min<size_t>(256, data.size());
                
                ImGui::BeginChild("HexDump", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);
                
                static ImGuiTextFilter hex_filter;
                hex_filter.Draw("Filter##hex", 200);
                
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // monospace
                
                const int bytes_per_row = 16;
                for (size_t row = 0; row < (dump_size + bytes_per_row - 1) / bytes_per_row; ++row) {
                    size_t offset = row * bytes_per_row;
                    if (offset >= data.size()) break;
                    
                    ImGui::Text("%08zX: ", entry.offset + offset);
                    ImGui::SameLine();
                    
                    // Hex bytes
                    for (int col = 0; col < bytes_per_row; ++col) {
                        size_t idx = offset + col;
                        if (idx < dump_size) {
                            ImGui::Text("%02X ", data[idx]);
                        } else {
                            ImGui::Text("   ");
                        }
                        ImGui::SameLine(0, col == 7 ? 20 : 4);
                    }
                    
                    // ASCII representation
                    ImGui::SameLine(16 * 4 + 20);
                    ImGui::Text("|");
                    ImGui::SameLine();
                    for (int col = 0; col < bytes_per_row; ++col) {
                        size_t idx = offset + col;
                        if (idx < dump_size) {
                            uint8_t c = data[idx];
                            ImGui::Text("%c", (c >= 32 && c <= 126) ? char(c) : '.');
                        }
                        ImGui::SameLine(0, 0);
                    }
                }
                
                ImGui::PopFont();
                ImGui::EndChild();
            } else {
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Failed to view data: %s", 
                                   tensyoki::to_string(view_result.error()));
            }
        }
        
        ImGui::EndChild();
    }
    
    bool show_hex_dump_ = true;
    bool show_full_hex_ = false;
    
    void shutdown() {
        log_info("Shutting down...");
        
        imgui_.reset();
        renderer_.reset();
        platform_.reset();
        
        Config::instance().save();
        VFS::instance().unmount_all();
        
        Logging::shutdown();
    }
    
    bool show_resource_browser_ = false;
    bool show_shader_viewer_ = false;
    bool show_dat_viewer_ = false;

    std::unique_ptr<BinParser> grp_parser_;
    const BinFileEntry* selected_entry_ = nullptr;
    const BinParser* selected_parser_ = nullptr;
};

} // namespace tensyoki

int main(int argc, char* argv[]) {
    try {
        tensyoki::Application app(argc, argv);
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error" << std::endl;
        return 1;
    }
}