#version 460 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

struct InstanceProperties {
    vec4 position;
};

layout(std430, binding = 1) buffer CurrValidInstanceData {
    InstanceProperties currValidInstanceProps[];
};

uniform mat4 modelMat;
uniform mat4 viewMat;
uniform mat4 projMat;

out VS_OUT {
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
    flat float textureLayer;
} vs_out;

void main() {
    uint instanceIndex = gl_BaseInstance + gl_InstanceID;
    vec4 instanceData = currValidInstanceProps[instanceIndex].position;
    vec3 translation = instanceData.xyz;
    float layer = instanceData.w;

    vec4 localPos = modelMat * vec4(inPosition, 1.0);
    vec3 worldPosition = localPos.xyz + translation;
    vec3 normal = mat3(modelMat) * inNormal;

    vs_out.worldPos = worldPosition;
    vs_out.normal = normal;
    vs_out.uv = inUV;
    vs_out.textureLayer = layer;

    gl_Position = projMat * viewMat * vec4(worldPosition, 1.0);
}
