#include "VKShader.h"

#include <fstream>
#include <iostream>
#include <vector>

VKShader::VKShader()
{
    vertexModule = VK_NULL_HANDLE;
    fragmentModule = VK_NULL_HANDLE;
    vertexStageInfo = {};
    fragmentStageInfo = {};
    computeStageInfo = {};
}

bool VKShader::LoadShader(VkDevice device, const std::string &vertexPath, const std::string &fragmentPath)
{
    std::ifstream vertexFile(vertexPath, std::ios::ate | std::ios::binary);
    std::ifstream fragmentFile(fragmentPath, std::ios::ate | std::ios::binary);

    if (!vertexFile.is_open())
    {
        std::cout << "Failed to open vertex file: " << vertexPath << '\n';
        return false;
    }
    if (!fragmentFile.is_open())
    {
        std::cout << "Failed to open fragment file: " << fragmentPath << '\n';
        return false;
    }

    size_t fileSize = (size_t)vertexFile.tellg();
    std::vector<char> vertexData(fileSize);
    vertexFile.seekg(0);
    vertexFile.read(vertexData.data(), fileSize);
    vertexFile.close();

    fileSize = (size_t)fragmentFile.tellg();
    std::vector<char> fragmentData(fileSize);
    fragmentFile.seekg(0);
    fragmentFile.read(fragmentData.data(), fileSize);
    fragmentFile.close();

    VkShaderModuleCreateInfo vertexCreateInfo = {};
    vertexCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertexCreateInfo.codeSize = vertexData.size();
    vertexCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertexData.data());

    VkShaderModuleCreateInfo fragmentCreateInfo = {};
    fragmentCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragmentCreateInfo.codeSize = fragmentData.size();
    fragmentCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragmentData.data());

    if (vkCreateShaderModule(device, &vertexCreateInfo, nullptr, &vertexModule) != VK_SUCCESS)
    {
        std::cout << "Failed to create vertex shader module\n";
        return false;
    }

    if (vkCreateShaderModule(device, &fragmentCreateInfo, nullptr, &fragmentModule) != VK_SUCCESS)
    {
        std::cout << "Failed to create fragment shader module\n";
        return false;
    }

    vertexStageInfo = {};
    vertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStageInfo.module = vertexModule;
    vertexStageInfo.pName = "main";

    fragmentStageInfo = {};
    fragmentStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStageInfo.module = fragmentModule;
    fragmentStageInfo.pName = "main";

    std::cout << "Loaded shaders:\nvertex: " << vertexPath << "\nfragment: " << fragmentPath << '\n';

    return true;
}

bool VKShader::LoadShader(VkDevice device, const std::string& computePath)
{
    std::ifstream computeFile(computePath, std::ios::ate | std::ios::binary);

    if (!computeFile.is_open())
    {
        std::cout << "Failed to open compute shader file: " << computePath << '\n';
        return false;
    }
    size_t fileSize = (size_t)computeFile.tellg();
    std::vector<char> computeData(fileSize);
    computeFile.seekg(0);
    computeFile.read(computeData.data(), fileSize);
    computeFile.close();

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = computeData.size();
    info.pCode = reinterpret_cast<const uint32_t*>(computeData.data());

    if (vkCreateShaderModule(device, &info, nullptr, &computeModule) != VK_SUCCESS)
    {
        std::cout << "Failed to create compute shader module\n";
        return false;
    }

    computeStageInfo = {};
    computeStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeStageInfo.module = computeModule;
    computeStageInfo.pName = "main";

    std::cout << "Loaded shaders:\ncompute: " << computePath << '\n';

    return true;
}

void VKShader::Dispose(VkDevice device)
{
    vkDestroyShaderModule(device, vertexModule, nullptr);
    vkDestroyShaderModule(device, fragmentModule, nullptr);
}
