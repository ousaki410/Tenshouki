/**
 * renderer.hpp - 渲染器抽象层
 * 支持 OpenGL 4.5 和 Vulkan 双后端
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace tensyoki {

class Renderer {
public:
    enum class Backend {
        OpenGL,
        Vulkan,
        Auto
    };
    
    struct InitParams {
        Backend backend = Backend::Auto;
        void* window_handle = nullptr;  // SDL_Window*
        int width = 1280;
        int height = 720;
        bool vsync = true;
        int msaa_samples = 0;
        bool debug_overlay = false;
        bool srgb_framebuffer = true;
    };
    
    struct Viewport {
        int x = 0, y = 0;
        int width = 0, height = 0;
        float min_depth = 0.0f, max_depth = 1.0f;
    };
    
    struct DrawCall {
        uint32_t vertex_count = 0;
        uint32_t instance_count = 1;
        uint32_t first_vertex = 0;
        uint32_t first_instance = 0;
    };
    
    struct TextureCreateInfo {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 1;
        uint32_t mip_levels = 1;
        uint32_t array_layers = 1;
        enum class Format {
            R8G8B8A8_UNORM,
            R8G8B8A8_SRGB,
            R32G32B32A32_SFLOAT,
            R16G16B16A16_SFLOAT,
            D32_SFLOAT,
            D24_UNORM_S8_UINT,
            BC1_RGBA_UNORM,
            BC3_UNORM,
            BC7_UNORM
        } format = Format::R8G8B8A8_UNORM;
        enum class Usage {
            ShaderRead,
            RenderTarget,
            DepthStencil,
            TransferSrc,
            TransferDst
        } usage = Usage::ShaderRead;
        const void* initial_data = nullptr;
        size_t data_size = 0;
    };
    
    struct BufferCreateInfo {
        size_t size = 0;
        enum class Usage {
            VertexBuffer,
            IndexBuffer,
            UniformBuffer,
            StorageBuffer,
            IndirectBuffer,
            Staging
        } usage = Usage::VertexBuffer;
        const void* initial_data = nullptr;
    };
    
    struct PipelineCreateInfo {
        std::string vertex_shader;    // SPIR-V 路径或源码
        std::string fragment_shader;
        struct VertexLayout {
            struct Attribute {
                std::string name;
                enum class Format { Float, Float2, Float3, Float4, Int, Int2, Int3, Int4 } format;
                uint32_t offset = 0;
            };
            std::vector<Attribute> attributes;
            uint32_t stride = 0;
        } vertex_layout;
        
        enum class PrimitiveTopology { TriangleList, TriangleStrip, LineList, PointList } topology = PrimitiveTopology::TriangleList;
        enum class PolygonMode { Fill, Line, Point } polygon_mode = PolygonMode::Fill;
        enum class CullMode { None, Front, Back, FrontAndBack } cull_mode = CullMode::Back;
        bool depth_test = true;
        bool depth_write = true;
        enum class CompareOp { Never, Less, Equal, LessOrEqual, Greater, NotEqual, GreaterOrEqual, Always } depth_compare = CompareOp::Less;
        
        struct BlendState {
            bool enable = false;
            enum class Factor { Zero, One, SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor, SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha } src_color = Factor::SrcAlpha, dst_color = Factor::OneMinusSrcAlpha, src_alpha = Factor::One, dst_alpha = Factor::Zero;
            enum class Op { Add, Subtract, ReverseSubtract, Min, Max } op_color = Op::Add, op_alpha = Op::Add;
        } blend;
    };
    
    Renderer();
    ~Renderer();
    
    // 初始化
    bool init(const InitParams& params);
    
    // 关闭
    void shutdown();
    
    // 调整大小
    void on_resize(int width, int height);
    
    // 帧开始/结束
    void begin_frame();
    void end_frame();
    
    // 视口/裁剪
    void set_viewport(const Viewport& vp);
    void set_scissor(int x, int y, int width, int height);
    
    // 清屏
    void clear(float r, float g, float b, float a, float depth = 1.0f, uint32_t stencil = 0);
    
    // 资源创建
    class Texture;
    class Buffer;
    class Pipeline;
    class Shader;
    
    std::shared_ptr<Texture> create_texture(const TextureCreateInfo& info);
    std::shared_ptr<Buffer> create_buffer(const BufferCreateInfo& info);
    std::shared_ptr<Pipeline> create_pipeline(const PipelineCreateInfo& info);
    std::shared_ptr<Shader> create_shader(ShaderStage stage, const std::vector<uint32_t>& spirv);
    
    // 绑定资源
    void bind_pipeline(std::shared_ptr<Pipeline> pipeline);
    void bind_vertex_buffer(uint32_t slot, std::shared_ptr<Buffer> buffer, uint32_t offset = 0);
    void bind_index_buffer(std::shared_ptr<Buffer> buffer, uint32_t offset = 0);
    void bind_texture(uint32_t slot, std::shared_ptr<Texture> texture);
    void bind_uniform_buffer(uint32_t slot, std::shared_ptr<Buffer> buffer, uint32_t offset = 0, uint32_t size = 0);
    
    // 绘制
    void draw(const DrawCall& call);
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0, uint32_t first_instance = 0);
    void draw_indirect(std::shared_ptr<Buffer> buffer, uint32_t offset = 0, uint32_t draw_count = 1, uint32_t stride = 0);
    
    // 计算着色器
    void dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z);
    
    // 获取后端信息
    Backend get_backend() const;
    void* get_device() const;  // VkDevice 或 无 (OpenGL)
    std::string get_backend_name() const;
    
    // 调试
    void push_debug_group(const std::string& name);
    void pop_debug_group();
    void insert_debug_marker(const std::string& marker);
    
    // 截图
    bool capture_screenshot(const std::filesystem::path& path);
    
    // 统计信息
    struct Stats {
        uint64_t draw_calls = 0;
        uint64_t triangles = 0;
        uint64_t vertices = 0;
        uint64_t texture_binds = 0;
        uint64_t pipeline_binds = 0;
        float gpu_time_ms = 0.0f;
    };
    Stats get_stats() const;
    void reset_stats();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// 2D 精灵渲染器 (高层封装)
class SpriteRenderer {
public:
    struct Sprite {
        glm::vec2 position = {0, 0};
        glm::vec2 size = {1, 1};
        glm::vec4 color = {1, 1, 1, 1};
        float rotation = 0;
        glm::vec2 origin = {0.5f, 0.5f};  // 旋转中心
        std::shared_ptr<Renderer::Texture> texture;
        glm::vec4 uv_rect = {0, 0, 1, 1};  // UV 裁剪
        int layer = 0;  // 渲染层级
    };
    
    SpriteRenderer(Renderer& renderer);
    ~SpriteRenderer();
    
    void begin(const glm::mat4& projection);
    void draw(const Sprite& sprite);
    void draw_batch(const std::vector<Sprite>& sprites);
    void end();
    void flush();
    
    // 设置默认纹理 (白色像素)
    void set_default_texture(std::shared_ptr<Renderer::Texture> tex);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// 文本渲染器 (SDF/MSDF 字体)
class TextRenderer {
public:
    struct TextParams {
        std::string text;
        glm::vec2 position = {0, 0};
        float size = 24.0f;
        glm::vec4 color = {1, 1, 1, 1};
        float wrap_width = 0;  // 0 = 不换行
        enum class Align { Left, Center, Right } align = Align::Left;
        float line_height = 1.2f;
        float letter_spacing = 0;
    };
    
    TextRenderer(Renderer& renderer);
    ~TextRenderer();
    
    bool load_font(const std::filesystem::path& font_path, float size = 24.0f);
    void draw(const TextParams& params);
    glm::vec2 measure_text(const std::string& text, float size) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tensyoki