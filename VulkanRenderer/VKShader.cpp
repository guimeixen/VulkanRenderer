#include "VKShader.h"

#include "Log.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

VKShader::VKShader()
{
    shaderModule = VK_NULL_HANDLE;
    stageInfo = {};
    compiled = false;
    compiledShaderFileExists = false;
    shaderNeedsCompile = false;
}

bool VKShader::LoadShader(VkDevice device, const std::string& shaderName, VkShaderStageFlagBits shaderStage)
{
    std::string baseShaderPath = "Data/Shaders/" + shaderName;
    std::string compiledShaderPath = "Data/Shaders/spirv/" + shaderName;

    if (shaderStage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        baseShaderPath += ".vert";
        compiledShaderPath += "_vert.spv";
    }
    else if (shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        baseShaderPath += ".frag";
        compiledShaderPath += "_frag.spv";
    }
    else if (shaderStage == VK_SHADER_STAGE_COMPUTE_BIT)
    {
        baseShaderPath += ".comp";
        compiledShaderPath += "_comp.spv";
    }

    compiledShaderFileExists = std::filesystem::exists(compiledShaderPath);

    if (compiledShaderFileExists)
    {
        auto compiledTime = std::filesystem::last_write_time(compiledShaderPath);
        auto baseShaderWriteTime = std::filesystem::last_write_time(baseShaderPath);

        // If the compiled time is older than when the base shader was modified the we need to compile it again
        if (compiledTime < baseShaderWriteTime)
            shaderNeedsCompile = true;
    }

    if (compiledShaderFileExists == false || shaderNeedsCompile)
    {
        // Compile the shader
        std::string command = "glslc.exe ";
        command += baseShaderPath;
        command += " -o " + compiledShaderPath;

        if (std::system(command.c_str()) != 0)
        {
            Log::Print(LogLevel::LEVEL_ERROR, "Failed to compile shader\n");
            return false;
        }
        else
        {
            Log::Print(LogLevel::LEVEL_INFO, "Compiled shader %s\n", baseShaderPath.c_str());
        }
    }

    std::ifstream file(compiledShaderPath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        std::cout << "Failed to open compiled shader file: " << compiledShaderPath << '\n';
        return false;
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> shaderData(fileSize);
    file.seekg(0);
    file.read(shaderData.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderData.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderData.data());

    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        std::cout << "Failed to create vertex shader module\n";
        return false;
    }

    stageInfo = {};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = shaderStage;
    stageInfo.module = shaderModule;
    stageInfo.pName = "main";

    std::cout << "Loaded shader: " << baseShaderPath << '\n';

    return true;
}

void VKShader::Dispose(VkDevice device)
{
    if (shaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device, shaderModule, nullptr);
    }
}

bool VKShader::Compile(const std::string &baseShaderPath, const std::string& compiledShaderPath, VkShaderStageFlagBits stage)
{
   

    return true;
}
