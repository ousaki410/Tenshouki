/**
 * imgui_backend.cpp - Dear ImGui 后端实现
 */

#include "imgui_backend.hpp"
#include "logging.hpp"
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>
#include <SDL3/SDL.h>
#include <vector>

namespace tensyoki {

struct ImGuiBackend::Impl {
    ImGuiContext* ctx = nullptr;
    bool initialized = false;
    bool use_vulkan = false;
    SDL_Window* window = nullptr;
};

ImGuiBackend::ImGuiBackend() : impl_(std::make_unique<Impl>()) {}
ImGuiBackend::~ImGuiBackend() { shutdown(); }

bool ImGuiBackend::init(const InitParams& params) {
    if (impl_->initialized) {
        log_warn("ImGuiBackend already initialized");
        return true;
    }
    
    impl_->window = static_cast<SDL_Window*>(params.window_handle);
    if (!impl_->window) {
        log_error("Invalid window handle");
        return false;
    }
    
    // 创建 ImGui 上下文
    impl_->ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(impl_->ctx);
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    if (params.enable_docking) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }
    if (params.enable_viewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }
    
    io.IniFilename = params.ini_filename.c_str();
    io.FontGlobalScale = params.font_scale;
    
    // 初始化 SDL3 后端
    if (!ImGui_ImplSDL3_InitForOpenGL(impl_->window, 
                                        SDL_GL_GetCurrentContext())) {
        log_error("ImGui_ImplSDL3_InitForOpenGL failed");
        return false;
    }
    
    // 初始化 OpenGL3 后端
    const char* glsl_version = "#version 450";
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        log_error("ImGui_ImplOpenGL3_Init failed");
        return false;
    }
    
    // 设置默认样式
    set_style_dark();
    
    // 加载默认字体
    load_font_default(18.0f * params.font_scale);
    
    impl_->initialized = true;
    log_info("ImGuiBackend initialized");
    return true;
}

void ImGuiBackend::shutdown() {
    if (!impl_->initialized) return;
    
    ImGui::SetCurrentContext(impl_->ctx);
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(impl_->ctx);
    
    impl_->ctx = nullptr;
    impl_->initialized = false;
    log_info("ImGuiBackend shutdown");
}

void ImGuiBackend::new_frame(float delta_time) {
    if (!impl_->initialized) return;
    
    ImGui::SetCurrentContext(impl_->ctx);
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    
    // 设置帧时间
    ImGui::GetIO().DeltaTime = delta_time;
}

void ImGuiBackend::render() {
    if (!impl_->initialized) return;
    
    ImGui::SetCurrentContext(impl_->ctx);
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // 处理多视口
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiBackend::process_event(const SDL_Event* event) {
    if (!impl_->initialized) return;
    ImGui_ImplSDL3_ProcessEvent(event);
}

ImFont* ImGuiBackend::load_font(const std::string& path, float size, 
                                 const ImWchar* ranges) {
    if (!impl_->initialized) return nullptr;
    
    ImGui::SetCurrentContext(impl_->ctx);
    ImGuiIO& io = ImGui::GetIO();
    
    ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size, nullptr, ranges);
    if (!font) {
        log_error("Failed to load font: {}", path);
        return nullptr;
    }
    
    log_info("Loaded font: {} ({}pt)", path, size);
    return font;
}

ImFont* ImGuiBackend::load_font_default(float size) {
    if (!impl_->initialized) return nullptr;
    
    ImGui::SetCurrentContext(impl_->ctx);
    ImGuiIO& io = ImGui::GetIO();
    
    // 添加默认字体 + 中日韩范围
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    
    // 基础字体
    ImFont* font = io.Fonts->AddFontDefault(&config);
    
    // 合并中文字体 (需要用户提供字体文件)
    // 这里只是预留接口
    
    io.FontDefault = font;
    return font;
}

void ImGuiBackend::set_default_font(ImFont* font) {
    if (!impl_->initialized) return;
    ImGui::SetCurrentContext(impl_->ctx);
    ImGui::GetIO().FontDefault = font;
}

const ImWchar* ImGuiBackend::get_chinese_ranges() {
    // 简体中文常用字范围
    static const ImWchar ranges[] = {
        0x0020, 0x00FF,  // Basic Latin + Latin Supplement
        0x2000, 0x206F,  // General Punctuation
        0x3000, 0x30FF,  // CJK Symbols and Punctuation
        0x4E00, 0x9FFF,  // CJK Unified Ideographs
        0,
    };
    return ranges;
}

const ImWchar* ImGuiBackend::get_japanese_ranges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF,
        0x2000, 0x206F,
        0x3000, 0x30FF,  // Hiragana/Katakana
        0x4E00, 0x9FFF,  // Kanji
        0xFF00, 0xFFEF,  // Halfwidth/Fullwidth
        0,
    };
    return ranges;
}

const ImWchar* ImGuiBackend::get_korean_ranges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF,
        0x2000, 0x206F,
        0x3130, 0x318F,  // Hangul Compatibility Jamo
        0xAC00, 0xD7AF,  // Hangul Syllables
        0,
    };
    return ranges;
}

void ImGuiBackend::set_style_dark() {
    if (!impl_->initialized) return;
    ImGui::SetCurrentContext(impl_->ctx);
    ImGui::StyleColorsDark();
}

void ImGuiBackend::set_style_light() {
    if (!impl_->initialized) return;
    ImGui::SetCurrentContext(impl_->ctx);
    ImGui::StyleColorsLight();
}

void ImGuiBackend::set_style_classic() {
    if (!impl_->initialized) return;
    ImGui::SetCurrentContext(impl_->ctx);
    ImGui::StyleColorsClassic();
}

void ImGuiBackend::scale_style(float factor) {
    if (!impl_->initialized) return;
    ImGui::SetCurrentContext(impl_->ctx);
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(factor);
}

void ImGuiBackend::on_resize(int width, int height) {
    // ImGui 自动处理视口大小
}

// ImGui 辅助函数实现
namespace imgui_utils {

void open_centered_popup(const char* name, ImVec2 size) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (size.x > 0 || size.y > 0) {
        ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    }
    ImGui::OpenPopup(name);
}

void help_marker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool input_text_readonly(const char* label, const char* value) {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
    bool ret = ImGui::InputText(label, const_cast<char*>(value), strlen(value) + 1, 
                                ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();
    return ret;
}

bool color_edit4(const char* label, float col[4], ImGuiColorEditFlags flags) {
    return ImGui::ColorEdit4(label, col, flags | ImGuiColorEditFlags_AlphaBar | 
                             ImGuiColorEditFlags_AlphaPreviewHalf);
}

bool drag_float2(const char* label, float v[2], float speed, 
                 float min, float max, const char* format) {
    return ImGui::DragFloat2(label, v, speed, min, max, format);
}

bool drag_float3(const char* label, float v[3], float speed,
                 float min, float max, const char* format) {
    return ImGui::DragFloat3(label, v, speed, min, max, format);
}

bool drag_float4(const char* label, float v[4], float speed,
                 float min, float max, const char* format) {
    return ImGui::DragFloat4(label, v, speed, min, max, format);
}

bool slider_float(const char* label, float* v, float min, float max,
                  const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderFloat(label, v, min, max, format, flags);
}

bool slider_int(const char* label, int* v, int min, int max,
                const char* format, ImGuiSliderFlags flags) {
    return ImGui::SliderInt(label, v, min, max, format, flags);
}

bool combo(const char* label, int* current, const char* const* items, int count) {
    return ImGui::Combo(label, current, items, count);
}

void table_setup_column(const char* label, ImGuiTableColumnFlags flags, float init_width) {
    ImGui::TableSetupColumn(label, flags, init_width);
}

bool table_next_row(ImGuiTableRowFlags flags) {
    return ImGui::TableNextRow(flags);
}

void table_next_column() {
    ImGui::TableNextColumn();
}

void table_set_column_index(int column) {
    ImGui::TableSetColumnIndex(column);
}

void file_drop_target(const char* label, std::function<void(const std::string&)> callback) {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            callback(path);
        }
        ImGui::EndDragDropTarget();
    }
}

void progress_bar(float fraction, const ImVec2& size, const char* overlay) {
    ImGui::ProgressBar(fraction, size, overlay);
}

bool color_button_with_picker(const char* label, float col[4], ImGuiColorEditFlags flags) {
    bool changed = false;
    if (ImGui::ColorButton(label, ImVec4(col[0], col[1], col[2], col[3]),
                           ImGuiColorEditFlags_NoTooltip | flags)) {
        ImGui::OpenPopup("color_picker");
    }
    if (ImGui::BeginPopup("color_picker")) {
        changed = ImGui::ColorPicker4("##picker", col, flags | ImGuiColorEditFlags_NoSidePreview);
        ImGui::EndPopup();
    }
    return changed;
}

bool begin_vertical_tab_bar(const char* str_id) {
    return ImGui::BeginTabBar(str_id, ImGuiTabBarFlags_NoTabListScrollingButtons | 
                              ImGuiTabBarFlags_FittingPolicyScroll);
}

void end_vertical_tab_bar() {
    ImGui::EndTabBar();
}

bool vertical_tab_item(const char* label, bool* selected) {
    return ImGui::TabItem(label, selected);
}

bool search_box(const char* label, char* buf, size_t buf_size) {
    return ImGui::InputTextWithHint(label, "Search...", buf, buf_size);
}

void separator_text(const char* label) {
    ImGui::SeparatorText(label);
}

void spacing(int count) {
    for (int i = 0; i < count; ++i) ImGui::Spacing();
}

void indent(float width) {
    if (width > 0) ImGui::Indent(width);
    else ImGui::Indent();
}

void unindent(float width) {
    if (width > 0) ImGui::Unindent(width);
    else ImGui::Unindent();
}

} // namespace imgui_utils

} // namespace tensyoki