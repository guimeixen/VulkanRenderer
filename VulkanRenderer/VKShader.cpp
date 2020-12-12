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
}

void VKShader::LoadShader(VkDevice device, const std::string &vertexPath, const std::string &fragmentPath)
{
    std::ifstream vertexFile(vertexPath, std::ios::ate | std::ios::binary);
    std::ifstream fragmentFile(fragmentPath, std::ios::ate | std::ios::binary);

    if (!vertexFile.is_open())
    {
        std::cout << "Failed to open vertex file: " << vertexPath << '\n';
        return;
    }
    if (!fragmentFile.is_open())
    {
        std::cout << "Failed to open fragment file: " << fragmentPath << '\n';
        return;
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
    }

    if (vkCreateShaderModule(device, &fragmentCreateInfo, nullptr, &fragmentModule) != VK_SUCCESS)
    {
        std::cout << "Failed to create fragment shader module\n";
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
}

void VKShader::Dispose(VkDevice device)
{
    vkDestroyShaderModule(device, vertexModule, nullptr);
    vkDestroyShaderModule(device, fragmentModule, nullptr);
}
