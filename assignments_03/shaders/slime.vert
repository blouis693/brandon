#version 430 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

uniform mat4 modelMat;
uniform mat4 viewMat;
uniform mat4 projMat;

out VS_OUT {
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
} vs_out;

void main() {
    vec4 worldPosition = modelMat * vec4(inPosition, 1.0);
    vs_out.worldPos = worldPosition.xyz;
    vs_out.normal = mat3(modelMat) * inNormal;
    vs_out.uv = inUV;
    gl_Position = projMat * viewMat * worldPosition;
}
