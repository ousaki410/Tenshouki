/**
 * shader_compiler.cpp - DXC Shader 编译器实现 (最小可行实现)
 */

#include "shader_compiler.hpp"
#include "logging.hpp"
#include <filesystem>
#include <fstream>
#include <vector>

namespace tensyoki {

// Check if DXC is available
#if defined(HAS_DXC) && HAS_DXC
#include <dxcapi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

ShaderCompiler::ShaderCompiler() {
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create DXC utils: 0x{:08X}", hr);
    }
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create DXC compiler: 0x{:08X}", hr);
    }
    hr = utils_->CreateDefaultIncludeHandler(&include_handler_);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create include handler: 0x{:08X}", hr);
    }
}

ShaderCompiler::~ShaderCompiler() = default;

ShaderCompiler::ShaderCompiler(ShaderCompiler&& other) noexcept 
    : utils_(std::move(other.utils_))
    , compiler_(std::move(other.compiler_))
    , include_handler_(std::move(other.include_handler_)) {}

ShaderCompiler& ShaderCompiler::operator=(ShaderCompiler&& other) noexcept {
    if (this != &other) {
        utils_ = std::move(other.utils_);
        compiler_ = std::move(other.compiler_);
        include_handler_ = std::move(other.include_handler_);
    }
    return *this;
}

std::vector<uint32_t> ShaderCompiler::compile_hlsl(const std::string& source, 
                                                   const std::string& entry_point,
                                                   ShaderStage stage,
                                                   const std::string& profile) {
    std::vector<uint32_t> result;
    
    if (!compiler_ || !utils_) {
        LOG_ERROR("DXC not initialized");
        return result;
    }
    
    // Create source buffer
    ComPtr<IDxcBlobEncoding> source_blob;
    HRESULT hr = utils_->CreateBlob(source.c_str(), source.size(), CP_UTF8, &source_blob);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create source blob: 0x{:08X}", hr);
        return result;
    }
    
    // Prepare arguments
    std::vector<LPCWSTR> args;
    args.push_back(L"-E");
    args.push_back(std::wstring(entry_point.begin(), entry_point.end()).c_str());
    args.push_back(L"-T");
    args.push_back(std::wstring(profile.begin(), profile.end()).c_str());
    args.push_back(L"-Zi");  // Debug info
    args.push_back(L"-Qstrip_reflect");  // Strip reflection
    
    // Compile
    ComPtr<IDxcResult> compile_result;
    hr = compiler_->Compile(source_blob.Get(), args.data(), static_cast<uint32_t>(args.size()), 
                           include_handler_.Get(), IID_PPV_ARGS(&compile_result));
    if (FAILED(hr)) {
        LOG_ERROR("Shader compilation failed: 0x{:08X}", hr);
        return result;
    }
    
    // Check for errors
    ComPtr<IDxcBlobUtf8> errors;
    hr = compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (SUCCEEDED(hr) && errors && errors->GetStringLength() > 0) {
        LOG_ERROR("Shader compilation errors:\n{}", errors->GetStringPointer());
        return result;
    }
    
    // Get shader blob
    ComPtr<IDxcBlob> shader_blob;
    hr = compile_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_blob), nullptr);
    if (FAILED(hr) || !shader_blob) {
        LOG_ERROR("Failed to get shader blob");
        return result;
    }
    
    // Convert to SPIR-V
    result.resize(shader_blob->GetBufferSize() / sizeof(uint32_t));
    std::memcpy(result.data(), shader_blob->GetBufferPointer(), shader_blob->GetBufferSize());
    
    LOG_INFO("Compiled shader '{}' ({}) - {} bytes", entry_point, profile, shader_blob->GetBufferSize());
    return result;
}

std::vector<uint32_t> ShaderCompiler::compile_hlsl_file(const std::filesystem::path& path,
                                                        const std::string& entry_point,
                                                        ShaderStage stage,
                                                        const std::string& profile) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        LOG_ERROR("Failed to open shader file: {}", path.string());
        return {};
    }
    
    size_t size = file.tellg();
    file.seekg(0);
    std::string source(size, '\0');
    file.read(&source[0], size);
    file.close();
    
    return compile_hlsl(source, entry_point, stage, profile);
}

ShaderReflection ShaderCompiler::reflect(const std::vector<uint32_t>& spirv) {
    ShaderReflection result;
    
    if (!utils_) return result;
    
    ComPtr<IDxcBlob> shader_blob;
    HRESULT hr = utils_->CreateBlob(spirv.data(), spirv.size() * sizeof(uint32_t), 0, &shader_blob);
    if (FAILED(hr)) return result;
    
    ComPtr<IDxcContainerReflection> reflection;
    hr = utils_->CreateReflection(shader_blob.Get(), IID_PPV_ARGS(&reflection));
    if (FAILED(hr)) return result;
    
    uint32_t shader_count = 0;
    reflection->GetShaderCount(&shader_count);
    
    for (uint32_t i = 0; i < shader_count; ++i) {
        ComPtr<IDxcShaderReflection> shader_reflection;
        hr = reflection->GetShaderReflection(i, &shader_reflection);
        if (FAILED(hr)) continue;
        
        DxcShaderDesc desc;
        shader_reflection->GetDesc(&desc);
        
        // Input parameters
        for (uint32_t j = 0; j < desc.InputParameterCount; ++j) {
            DxcSignatureParameterDesc param_desc;
            shader_reflection->GetInputParameterDesc(j, &param_desc);
            
            ShaderReflection::Parameter param;
            param.name = param_desc.SemanticName ? param_desc.SemanticName : "";
            param.register_index = param_desc.Register;
            param.component_count = 4; // Simplified
            result.input_parameters.push_back(param);
        }
        
        // Output parameters
        for (uint32_t j = 0; j < desc.OutputParameterCount; ++j) {
            DxcSignatureParameterDesc param_desc;
            shader_reflection->GetOutputParameterDesc(j, &param_desc);
            
            ShaderReflection::Parameter param;
            param.name = param_desc.SemanticName ? param_desc.SemanticName : "";
            param.register_index = param_desc.Register;
            param.component_count = 4;
            result.output_parameters.push_back(param);
        }
        
        // Resources (CBV, SRV, UAV, Sampler)
        for (uint32_t j = 0; j < desc.BoundResourceCount; ++j) {
            DxcResourceBindingDesc res_desc;
            shader_reflection->GetResourceBindingDesc(j, &res_desc);
            
            ShaderReflection::ResourceBinding binding;
            binding.name = res_desc.Name ? res_desc.Name : "";
            binding.register_index = res_desc.BindPoint;
            binding.space = res_desc.BindSpace;
            binding.count = res_desc.BindCount;
            
            switch (res_desc.Type) {
                case DXC_RESOURCE_TYPE_CBUFFER: binding.type = ShaderReflection::ResourceBinding::Type::ConstantBuffer; break;
                case DXC_RESOURCE_TYPE_TEXTURE: binding.type = ShaderReflection::ResourceBinding::Type::Texture; break;
                case DXC_RESOURCE_TYPE_SAMPLER: binding.type = ShaderReflection::ResourceBinding::Type::Sampler; break;
                case DXC_RESOURCE_TYPE_UAV_RWTYPED:
                case DXC_RESOURCE_TYPE_UAV_RWSTRUCTURED:
                case DXC_RESOURCE_TYPE_UAV_RWBYTEADDRESS:
                case DXC_RESOURCE_TYPE_UAV_APPEND_STRUCTURED:
                case DXC_RESOURCE_TYPE_UAV_CONSUME_STRUCTURED:
                case DXC_RESOURCE_TYPE_UAV_RWSTRUCTURED_WITH_COUNTER: binding.type = ShaderReflection::ResourceBinding::Type::StorageBuffer; break;
                default: binding.type = ShaderReflection::ResourceBinding::Type::Unknown; break;
            }
            
            result.resources.push_back(binding);
        }
    }
    
    return result;
}

bool ShaderCompiler::save_spirv(const std::vector<uint32_t>& spirv, const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        LOG_ERROR("Failed to open output file: {}", path.string());
        return false;
    }
    file.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(uint32_t));
    return file.good();
}

std::string ShaderCompiler::get_profile(ShaderStage stage, bool spirv) {
    if (spirv) {
        switch (stage) {
            case ShaderStage::Vertex: return "vs_6_0";
            case ShaderStage::Pixel: return "ps_6_0";
            case ShaderStage::Compute: return "cs_6_0";
            case ShaderStage::Geometry: return "gs_6_0";
            case ShaderStage::Hull: return "hs_6_0";
            case ShaderStage::Domain: return "ds_6_0";
        }
    } else {
        switch (stage) {
            case ShaderStage::Vertex: return "vs_5_0";
            case ShaderStage::Pixel: return "ps_5_0";
            case ShaderStage::Compute: return "cs_5_0";
            case ShaderStage::Geometry: return "gs_5_0";
            case ShaderStage::Hull: return "hs_5_0";
            case ShaderStage::Domain: return "ds_5_0";
        }
    }
    return "vs_6_0";
}

#else

// Fallback implementation without DXC
ShaderCompiler::ShaderCompiler() {
    LOG_WARN("DXC not available, shader compilation disabled");
}

ShaderCompiler::~ShaderCompiler() = default;

ShaderCompiler::ShaderCompiler(ShaderCompiler&&) noexcept = default;
ShaderCompiler& ShaderCompiler::operator=(ShaderCompiler&&) noexcept = default;

std::vector<uint32_t> ShaderCompiler::compile_hlsl(const std::string&, 
                                                   const std::string&,
                                                   ShaderStage,
                                                   const std::string&) {
    LOG_ERROR("DXC not available, cannot compile HLSL");
    return {};
}

std::vector<uint32_t> ShaderCompiler::compile_hlsl_file(const std::filesystem::path&,
                                                        const std::string&,
                                                        ShaderStage,
                                                        const std::string&) {
    LOG_ERROR("DXC not available, cannot compile HLSL file");
    return {};
}

ShaderReflection ShaderCompiler::reflect(const std::vector<uint32_t>&) {
    LOG_WARN("DXC not available, cannot reflect SPIR-V");
    return {};
}

bool ShaderCompiler::save_spirv(const std::vector<uint32_t>&, const std::filesystem::path&) {
    LOG_ERROR("DXC not available, cannot save SPIR-V");
    return false;
}

std::string ShaderCompiler::get_profile(ShaderStage, bool) {
    return "vs_6_0";
}

#endif

} // namespace tensyoki