#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 uv;

layout(set = 1, binding = 1) uniform samplerCube tex;

void main()
{
    outColor = texture(tex, uv);
}