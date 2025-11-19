#version 430 core

layout(location = 0) out vec4 fragColor;

in VS_OUT {
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
} fs_in;

uniform sampler2D albedoTexture;
uniform vec3 cameraPos;
uniform vec3 lightDir;

const vec3 Ka = vec3(0.1);
const vec3 Kd = vec3(0.8);
const vec3 Ks = vec3(0.1);
const float shininess = 1.0;
const float exposure = 3.0;

void main() {
    vec4 texel = texture(albedoTexture, fs_in.uv);
    if (texel.a < 0.5) {
        discard;
    }
    vec3 albedo = texel.rgb;
    vec3 N = normalize(fs_in.normal);
    vec3 L = normalize(lightDir);
    vec3 V = normalize(cameraPos - fs_in.worldPos);
    float diff = max(dot(N, L), 0.0);
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(R, V), 0.0), shininess);

    vec3 ambient = Ka * albedo;
    vec3 diffuse = Kd * albedo * diff;
    vec3 specular = Ks * albedo * spec;
    vec3 color = ambient + diffuse + specular;

    vec3 mapped = vec3(1.0) - exp(-color * exposure);
    mapped = pow(mapped, vec3(1.0 / 2.2));
    fragColor = vec4(mapped, 1.0);
}
