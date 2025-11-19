#include "RenderingOrderExp.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/vec4.hpp>

#include <tiny_obj_loader.h>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "../Rendering/ShaderParameterBindingPoint.h"
#include "../Rendering/Shader.h"
#include "../Scene/SpatialSample.h"

namespace INANOA {
namespace {
        constexpr GLuint RAW_INSTANCE_BINDING = 0;
        constexpr GLuint VISIBLE_INSTANCE_BINDING = 1;
        constexpr GLuint DRAW_COMMAND_BINDING = 2;
        constexpr GLuint INSTANCE_STATE_BINDING = 3;

        constexpr int NUM_FOLIAGE_TEXTURES = 3;
        constexpr int IMG_WIDTH = 1024;
        constexpr int IMG_HEIGHT = 1024;
        constexpr int IMG_CHANNEL = 4;

        const glm::vec3 LIGHT_DIRECTION = glm::normalize(glm::vec3(0.3f, 0.7f, 0.5f));

        struct RawInstancePropertiesGPU {
                glm::vec4 position;
                glm::ivec4 indices;
        };
        struct InstancePropertiesGPU {
                glm::vec4 position;
        };
}

struct RenderingOrderExp::Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
};

struct RenderingOrderExp::MeshInfo {
        std::string name;
        uint32_t indexCount = 0u;
        uint32_t firstIndex = 0u;
        uint32_t baseVertex = 0u;
        uint32_t textureLayer = 0u;
        uint32_t rawOffset = 0u;
        uint32_t rawCount = 0u;
};

struct RenderingOrderExp::DrawCommand {
        uint32_t count = 0u;
        uint32_t instanceCount = 0u;
        uint32_t firstIndex = 0u;
        uint32_t baseVertex = 0u;
        uint32_t baseInstance = 0u;
};

namespace {
        std::string directoryFromPath(const std::string& filePath) {
                const size_t pos = filePath.find_last_of("/\\");
                if (pos == std::string::npos) {
                        return std::string();
                }
                return filePath.substr(0, pos + 1);
        }
}

RenderingOrderExp::RenderingOrderExp() {
this->m_cameraForwardSpeed = 0.25f;
this->m_cameraForwardMagnitude = glm::vec3(0.0f);
this->m_frameWidth = 64;
this->m_frameHeight = 64;
this->m_rawInstanceSSBO = 0u;
this->m_visibleInstanceSSBO = 0u;
this->m_drawCommandSSBO = 0u;
this->m_instanceStateSSBO = 0u;
this->m_totalInstanceCount = 0u;
       
}

RenderingOrderExp::~RenderingOrderExp() {
        delete this->m_viewFrustum;
        delete this->m_horizontalGround;
        delete this->m_renderer;
        delete this->m_playerCamera;
        delete this->m_godCamera;

        delete this->m_foliageShader;
        delete this->m_slimeShader;
        delete this->m_computeShader;

        if (this->m_foliageVao != 0u) {
                glDeleteVertexArrays(1, &this->m_foliageVao);
        }
        if (this->m_foliageVbo != 0u) {
                glDeleteBuffers(1, &this->m_foliageVbo);
        }
        if (this->m_foliageIbo != 0u) {
                glDeleteBuffers(1, &this->m_foliageIbo);
        }
        if (this->m_slimeVao != 0u) {
                glDeleteVertexArrays(1, &this->m_slimeVao);
        }
        if (this->m_slimeVbo != 0u) {
                glDeleteBuffers(1, &this->m_slimeVbo);
        }
        if (this->m_slimeIbo != 0u) {
                glDeleteBuffers(1, &this->m_slimeIbo);
        }
        if (this->m_foliageTextureArray != 0u) {
                glDeleteTextures(1, &this->m_foliageTextureArray);
        }
        if (this->m_slimeTexture != 0u) {
                glDeleteTextures(1, &this->m_slimeTexture);
        }
        if (this->m_rawInstanceSSBO != 0u) {
                glDeleteBuffers(1, &this->m_rawInstanceSSBO);
        }
        if (this->m_visibleInstanceSSBO != 0u) {
                glDeleteBuffers(1, &this->m_visibleInstanceSSBO);
        }
        if (this->m_drawCommandSSBO != 0u) {
                glDeleteBuffers(1, &this->m_drawCommandSSBO);
        }
        if (this->m_instanceStateSSBO != 0u) {
                glDeleteBuffers(1, &this->m_instanceStateSSBO);
        }
}

bool RenderingOrderExp::init(const int w, const int h) {
        OPENGL::RendererBase* renderer = new OPENGL::RendererBase();
        const std::string vsFile = "shaders\\vertexShader_ogl_450.glsl";
        const std::string fsFile = "shaders\\fragmentShader_ogl_450.glsl";
        if (renderer->init(vsFile, fsFile, w, h) == false) {
                return false;
        }
        this->m_renderer = renderer;

        this->m_godCamera = new Camera(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 5.0f, 60.0f, 0.1f, 512.0f);
        this->m_godCamera->resize(w, h);
        this->m_godCamera->setViewOrg(glm::vec3(0.0f, 55.0f, 50.0f));
        this->m_godCamera->setLookCenter(glm::vec3(0.0f, 32.0f, -12.0f));
        this->m_godCamera->setDistance(70.0f);
        this->m_godCamera->update();

        this->m_playerCamera = new Camera(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 9.5f, -5.0f), glm::vec3(0.0f, 1.0f, 0.0f), 10.0f, 45.0f, 1.0f, 150.0f);
        this->m_playerCamera->resize(w, h);
        this->m_playerCamera->update();

        this->m_renderer->setCamera(
                this->m_godCamera->projMatrix(),
                this->m_godCamera->viewMatrix(),
                this->m_godCamera->viewOrig()
        );

        this->m_viewFrustum = new SCENE::RViewFrustum(1, this->m_playerCamera);
        this->m_viewFrustum->resize(this->m_playerCamera);

        this->m_horizontalGround = new SCENE::EXPERIMENTAL::HorizonGround(2, this->m_playerCamera);
        this->m_horizontalGround->resize(this->m_playerCamera);

        this->m_godCameraTarget = this->m_godCamera->lookCenter();
        const glm::vec3 godDir = glm::normalize(this->m_godCamera->lookCenter() - this->m_godCamera->viewOrig());
        this->m_trackballYaw = std::atan2(godDir.x, godDir.z);
        this->m_trackballPitch = std::asin(glm::clamp(godDir.y, -1.0f, 1.0f));

        this->initializeSceneResources();
        this->resize(w, h);

        this->m_slimeTrajectory.enable(true);

        return true;
}

void RenderingOrderExp::initializeSceneResources() {
        this->initializeFoliage();
        this->initializeTextureArray();
        this->initializeInstanceBuffers();
        this->initializeComputeShader();
        this->initializeSlime();
        this->updateGodCameraTrackball();
}

void RenderingOrderExp::initializeFoliage() {
        const std::array<std::pair<std::string, uint32_t>, NUM_FOLIAGE_TEXTURES> meshInfos = { {
                {"assets/models/foliages/grassB.obj", 0u},
                {"assets/models/foliages/bush01_lod2.obj", 1u},
                {"assets/models/foliages/bush05_lod2.obj", 2u}
        } };

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        vertices.reserve(100000);
        indices.reserve(150000);
        this->m_meshInfos.clear();

        for (size_t meshIdx = 0; meshIdx < meshInfos.size(); ++meshIdx) {
                const std::string& objFile = meshInfos[meshIdx].first;
                tinyobj::attrib_t attrib;
                std::vector<tinyobj::shape_t> shapes;
                std::vector<tinyobj::material_t> materials;
                std::string warn;
                std::string err;
                const std::string baseDir = directoryFromPath(objFile);
                bool loaded = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objFile.c_str(), baseDir.c_str(), true);
                if (!warn.empty()) {
                        std::cout << "[TinyObjLoader] " << warn << std::endl;
                }
                if (!loaded) {
                        std::cerr << "Failed to load OBJ: " << objFile << " " << err << std::endl;
                        continue;
                }

                std::vector<Vertex> meshVertices;
                std::vector<uint32_t> meshIndices;
                meshVertices.reserve(shapes.size() * 64);
                meshIndices.reserve(shapes.size() * 64);
                uint32_t localVertexCounter = 0u;

                for (const auto& shape : shapes) {
                        for (const auto& idx : shape.mesh.indices) {
                                Vertex vertex{};
                                if (idx.vertex_index >= 0) {
                                        vertex.position = glm::vec3(
                                                attrib.vertices[3 * idx.vertex_index + 0],
                                                attrib.vertices[3 * idx.vertex_index + 1],
                                                attrib.vertices[3 * idx.vertex_index + 2]
                                        );
                                }
                                if (idx.normal_index >= 0) {
                                        vertex.normal = glm::vec3(
                                                attrib.normals[3 * idx.normal_index + 0],
                                                attrib.normals[3 * idx.normal_index + 1],
                                                attrib.normals[3 * idx.normal_index + 2]
                                        );
                                }
                                else {
                                        vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                                }
                                if (idx.texcoord_index >= 0) {
                                        vertex.uv = glm::vec2(
                                                attrib.texcoords[2 * idx.texcoord_index + 0],
                                                attrib.texcoords[2 * idx.texcoord_index + 1]
                                        );
                                }
                                else {
                                        vertex.uv = glm::vec2(0.0f);
                                }
                                meshVertices.push_back(vertex);
                                meshIndices.push_back(localVertexCounter);
                                localVertexCounter = localVertexCounter + 1u;
                        }
                }

                const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
                const uint32_t firstIndex = static_cast<uint32_t>(indices.size());
                vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());
                indices.insert(indices.end(), meshIndices.begin(), meshIndices.end());

                MeshInfo info{};
                info.name = objFile;
                info.textureLayer = meshInfos[meshIdx].second;
                info.baseVertex = baseVertex;
                info.firstIndex = firstIndex;
                info.indexCount = static_cast<uint32_t>(meshIndices.size());
                this->m_meshInfos.push_back(info);
        }

        glGenVertexArrays(1, &this->m_foliageVao);
        glBindVertexArray(this->m_foliageVao);

        glGenBuffers(1, &this->m_foliageVbo);
        glBindBuffer(GL_ARRAY_BUFFER, this->m_foliageVbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &this->m_foliageIbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->m_foliageIbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(glm::vec3)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(glm::vec3) * 2));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        this->m_foliageShader = OPENGL::ShaderProgram::createShaderProgram("shaders/foliage_instancing.vert", "shaders/foliage_instancing.frag");
        if (this->m_foliageShader == nullptr) {
                        std::cerr << "Failed to create foliage shader program" << std::endl;
                        return;
        }
        glUseProgram(this->m_foliageShader->programId());
        this->m_foliageModelLoc = glGetUniformLocation(this->m_foliageShader->programId(), "modelMat");
        this->m_foliageViewLoc = glGetUniformLocation(this->m_foliageShader->programId(), "viewMat");
        this->m_foliageProjLoc = glGetUniformLocation(this->m_foliageShader->programId(), "projMat");
        this->m_foliageCameraPosLoc = glGetUniformLocation(this->m_foliageShader->programId(), "cameraPos");
        this->m_foliageLightDirLoc = glGetUniformLocation(this->m_foliageShader->programId(), "lightDir");
        const GLint foliageSamplerLoc = glGetUniformLocation(this->m_foliageShader->programId(), "albedoTextureArray");
        glUniform1i(foliageSamplerLoc, 0);
        const glm::mat4 identityMat(1.0f);
        glUniformMatrix4fv(this->m_foliageModelLoc, 1, GL_FALSE, glm::value_ptr(identityMat));
        glUniform3fv(this->m_foliageLightDirLoc, 1, glm::value_ptr(LIGHT_DIRECTION));
        glUseProgram(0);
}

void RenderingOrderExp::initializeTextureArray() {
        stbi_set_flip_vertically_on_load(true);
        const std::array<std::string, NUM_FOLIAGE_TEXTURES> textures = { {
                "assets/textures/grassB_albedo.png",
                "assets/textures/bush01.png",
                "assets/textures/bush05.png"
        } };

        const size_t layerSize = static_cast<size_t>(IMG_WIDTH) * IMG_HEIGHT * IMG_CHANNEL;
        std::vector<unsigned char> merged(layerSize * NUM_FOLIAGE_TEXTURES, 0);

        for (size_t i = 0; i < textures.size(); ++i) {
                int width = 0;
                int height = 0;
                int channel = 0;
                unsigned char* data = stbi_load(textures[i].c_str(), &width, &height, &channel, STBI_rgb_alpha);
                if (data == nullptr) {
                        std::cerr << "Failed to load texture: " << textures[i] << std::endl;
                        continue;
                }

                const int copyWidth = std::min(width, IMG_WIDTH);
                const int copyHeight = std::min(height, IMG_HEIGHT);
                for (int y = 0; y < copyHeight; ++y) {
                        unsigned char* dst = merged.data() + i * layerSize + static_cast<size_t>(y) * IMG_WIDTH * IMG_CHANNEL;
                        const unsigned char* src = data + static_cast<size_t>(y) * width * IMG_CHANNEL;
                        std::memcpy(dst, src, static_cast<size_t>(copyWidth) * IMG_CHANNEL);
                }
                stbi_image_free(data);
        }

        glGenTextures(1, &this->m_foliageTextureArray);
        glBindTexture(GL_TEXTURE_2D_ARRAY, this->m_foliageTextureArray);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 11, GL_RGBA8, IMG_WIDTH, IMG_HEIGHT, NUM_FOLIAGE_TEXTURES);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, IMG_WIDTH, IMG_HEIGHT, NUM_FOLIAGE_TEXTURES, GL_RGBA, GL_UNSIGNED_BYTE, merged.data());
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void RenderingOrderExp::initializeInstanceBuffers() {
        using namespace SCENE::EXPERIMENTAL;
        const std::array<std::string, NUM_FOLIAGE_TEXTURES> sampleFiles = { {
                "assets/models/spatialSamples/poissonPoints_155304s.ss2",
                "assets/models/spatialSamples/poissonPoints_1010s.ss2",
                "assets/models/spatialSamples/poissonPoints_2797s.ss2"
        } };

        std::vector<RawInstancePropertiesGPU> rawInstances;
        rawInstances.reserve(200000);

        uint32_t baseInstance = 0u;
        this->m_drawCommands.clear();
        this->m_drawCommands.resize(this->m_meshInfos.size());

        for (size_t meshIdx = 0; meshIdx < this->m_meshInfos.size() && meshIdx < sampleFiles.size(); ++meshIdx) {
                SpatialSample* sample = SpatialSample::importBinaryFile(sampleFiles[meshIdx]);
                uint32_t meshCount = 0u;
                if (sample != nullptr) {
                        meshCount = static_cast<uint32_t>(sample->numSample());
                        rawInstances.reserve(rawInstances.size() + meshCount);
                        for (uint32_t i = 0u; i < meshCount; ++i) {
                                const float* pos = sample->position(i);
                                RawInstancePropertiesGPU raw{};
                                raw.position = glm::vec4(pos[0], pos[1], pos[2], static_cast<float>(this->m_meshInfos[meshIdx].textureLayer));
                                raw.indices = glm::ivec4(static_cast<int>(meshIdx), static_cast<int>(this->m_meshInfos[meshIdx].textureLayer), 0, 0);
                                rawInstances.push_back(raw);
                        }
                        delete sample;
                }
                MeshInfo& info = this->m_meshInfos[meshIdx];
                info.rawOffset = baseInstance;
                info.rawCount = meshCount;

                DrawCommand cmd{};
                cmd.count = info.indexCount;
                cmd.instanceCount = 0u;
                cmd.firstIndex = info.firstIndex;
                cmd.baseVertex = info.baseVertex;
                cmd.baseInstance = baseInstance;
                this->m_drawCommands[meshIdx] = cmd;
                baseInstance += meshCount;
        }

        this->m_totalInstanceCount = baseInstance;

        if (this->m_totalInstanceCount == 0u) {
                return;
        }

        glGenBuffers(1, &this->m_rawInstanceSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->m_rawInstanceSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, rawInstances.size() * sizeof(RawInstancePropertiesGPU), rawInstances.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &this->m_visibleInstanceSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->m_visibleInstanceSSBO);
        std::vector<InstancePropertiesGPU> emptyInstances(this->m_totalInstanceCount);
        glBufferData(GL_SHADER_STORAGE_BUFFER, emptyInstances.size() * sizeof(InstancePropertiesGPU), emptyInstances.data(), GL_DYNAMIC_DRAW);

        glGenBuffers(1, &this->m_instanceStateSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->m_instanceStateSSBO);
        std::vector<uint32_t> initialState(this->m_totalInstanceCount, 0u);
        glBufferData(GL_SHADER_STORAGE_BUFFER, initialState.size() * sizeof(uint32_t), initialState.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        this->uploadDrawCommands();
}

void RenderingOrderExp::uploadDrawCommands() {
        if (this->m_drawCommands.empty()) {
                return;
        }
        if (this->m_drawCommandSSBO == 0u) {
                glGenBuffers(1, &this->m_drawCommandSSBO);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->m_drawCommandSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, this->m_drawCommands.size() * sizeof(DrawCommand), this->m_drawCommands.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void RenderingOrderExp::initializeComputeShader() {
        this->m_computeShader = OPENGL::ShaderProgram::createShaderProgramForComputeShader("shaders/foliage_cull.comp");
        if (this->m_computeShader == nullptr) {
                std::cerr << "Failed to create compute shader" << std::endl;
                return;
        }
        glUseProgram(this->m_computeShader->programId());
        this->m_computeViewLoc = glGetUniformLocation(this->m_computeShader->programId(), "playerView");
        this->m_computeProjLoc = glGetUniformLocation(this->m_computeShader->programId(), "playerProj");
        this->m_computeTotalInstanceLoc = glGetUniformLocation(this->m_computeShader->programId(), "totalInstanceCount");
        this->m_computeNumMeshesLoc = glGetUniformLocation(this->m_computeShader->programId(), "numMeshes");
        this->m_computeEraseRadiusLoc = glGetUniformLocation(this->m_computeShader->programId(), "eraseRadius");
        this->m_computeSlimePosLoc = glGetUniformLocation(this->m_computeShader->programId(), "slimePosition");
        glUseProgram(0);
}

void RenderingOrderExp::initializeSlime() {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn;
        std::string err;
        const std::string slimePath = "assets/models/foliages/slime.obj";
        const std::string baseDir = directoryFromPath(slimePath);
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, slimePath.c_str(), baseDir.c_str(), true)) {
                std::cerr << "Failed to load slime obj: " << err << std::endl;
        }
        if (!warn.empty()) {
                std::cout << "[TinyObjLoader] " << warn << std::endl;
        }

        std::vector<Vertex> slimeVertices;
        std::vector<uint32_t> slimeIndices;
        uint32_t localVertexCounter = 0u;
        for (const auto& shape : shapes) {
                for (const auto& idx : shape.mesh.indices) {
                        Vertex vertex{};
                        if (idx.vertex_index >= 0) {
                                vertex.position = glm::vec3(
                                        attrib.vertices[3 * idx.vertex_index + 0],
                                        attrib.vertices[3 * idx.vertex_index + 1],
                                        attrib.vertices[3 * idx.vertex_index + 2]
                                );
                        }
                        if (idx.normal_index >= 0) {
                                vertex.normal = glm::vec3(
                                        attrib.normals[3 * idx.normal_index + 0],
                                        attrib.normals[3 * idx.normal_index + 1],
                                        attrib.normals[3 * idx.normal_index + 2]
                                );
                        }
                        else {
                                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                        }
                        if (idx.texcoord_index >= 0) {
                                vertex.uv = glm::vec2(
                                        attrib.texcoords[2 * idx.texcoord_index + 0],
                                        attrib.texcoords[2 * idx.texcoord_index + 1]
                                );
                        }
                        slimeVertices.push_back(vertex);
                        slimeIndices.push_back(localVertexCounter++);
                }
        }

        glGenVertexArrays(1, &this->m_slimeVao);
        glBindVertexArray(this->m_slimeVao);
        glGenBuffers(1, &this->m_slimeVbo);
        glBindBuffer(GL_ARRAY_BUFFER, this->m_slimeVbo);
        glBufferData(GL_ARRAY_BUFFER, slimeVertices.size() * sizeof(Vertex), slimeVertices.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &this->m_slimeIbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->m_slimeIbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, slimeIndices.size() * sizeof(uint32_t), slimeIndices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(glm::vec3)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(glm::vec3) * 2));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        this->m_slimeIndexCount = static_cast<GLsizei>(slimeIndices.size());

        this->m_slimeShader = OPENGL::ShaderProgram::createShaderProgram("shaders/slime.vert", "shaders/slime.frag");
        if (this->m_slimeShader == nullptr) {
                std::cerr << "Failed to create slime shader" << std::endl;
                return;
        }
        glUseProgram(this->m_slimeShader->programId());
        this->m_slimeModelLoc = glGetUniformLocation(this->m_slimeShader->programId(), "modelMat");
        this->m_slimeViewLoc = glGetUniformLocation(this->m_slimeShader->programId(), "viewMat");
        this->m_slimeProjLoc = glGetUniformLocation(this->m_slimeShader->programId(), "projMat");
        this->m_slimeCameraPosLoc = glGetUniformLocation(this->m_slimeShader->programId(), "cameraPos");
        this->m_slimeLightDirLoc = glGetUniformLocation(this->m_slimeShader->programId(), "lightDir");
        const GLint slimeSamplerLoc = glGetUniformLocation(this->m_slimeShader->programId(), "albedoTexture");
        glUniform1i(slimeSamplerLoc, 0);
        glUniform3fv(this->m_slimeLightDirLoc, 1, glm::value_ptr(LIGHT_DIRECTION));
        glUseProgram(0);

        int width = 0;
        int height = 0;
        int channel = 0;
        unsigned char* data = stbi_load("assets/textures/slime_albedo.jpg", &width, &height, &channel, STBI_rgb_alpha);
        if (data == nullptr) {
                std::cerr << "Failed to load slime texture" << std::endl;
                return;
        }
        glGenTextures(1, &this->m_slimeTexture);
        glBindTexture(GL_TEXTURE_2D, this->m_slimeTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(data);
}

void RenderingOrderExp::resize(const int w, const int h) {
        const int bottomHeight = std::max(1, h / 2);
        const int topHeight = std::max(1, h - bottomHeight);

        this->m_playerCamera->resize(w, bottomHeight);
        this->m_godCamera->resize(w, topHeight);
        this->m_renderer->resize(w, h);
        this->m_frameWidth = w;
        this->m_frameHeight = h;

        this->m_viewFrustum->resize(this->m_playerCamera);
        this->m_horizontalGround->resize(this->m_playerCamera);
}

void RenderingOrderExp::update() {
        this->updatePlayerCameraMovement();
        this->m_playerCamera->forward(this->m_cameraForwardMagnitude, true);
        this->m_playerCamera->update();

        this->updateGodCameraTrackball();

        this->m_viewFrustum->update(this->m_playerCamera);
        this->m_horizontalGround->update(this->m_playerCamera);

        this->m_slimeTrajectory.update();
}

void RenderingOrderExp::render() {
        this->m_renderer->clearRenderTarget();
        const int bottomHeight = std::max(1, this->m_frameHeight / 2);
        const int topHeight = std::max(1, this->m_frameHeight - bottomHeight);

        const glm::vec3 slimePos = this->m_slimeTrajectory.position();
        this->dispatchCullingCompute(this->m_playerCamera, slimePos);

        this->renderViewport(this->m_godCamera, slimePos, 0, bottomHeight, this->m_frameWidth, topHeight);
        glClear(GL_DEPTH_BUFFER_BIT);
        this->renderViewport(this->m_playerCamera, slimePos, 0, 0, this->m_frameWidth, bottomHeight);
}

void RenderingOrderExp::dispatchCullingCompute(const Camera* playerCam, const glm::vec3& slimePos) {
        if (this->m_computeShader == nullptr || this->m_totalInstanceCount == 0u) {
                return;
        }
        for (auto& cmd : this->m_drawCommands) {
                cmd.instanceCount = 0u;
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->m_drawCommandSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, this->m_drawCommands.size() * sizeof(DrawCommand), this->m_drawCommands.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glUseProgram(this->m_computeShader->programId());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, RAW_INSTANCE_BINDING, this->m_rawInstanceSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, VISIBLE_INSTANCE_BINDING, this->m_visibleInstanceSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_COMMAND_BINDING, this->m_drawCommandSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, INSTANCE_STATE_BINDING, this->m_instanceStateSSBO);
        glUniformMatrix4fv(this->m_computeViewLoc, 1, GL_FALSE, glm::value_ptr(playerCam->viewMatrix()));
        glUniformMatrix4fv(this->m_computeProjLoc, 1, GL_FALSE, glm::value_ptr(playerCam->projMatrix()));
        glUniform1ui(this->m_computeTotalInstanceLoc, static_cast<GLuint>(this->m_totalInstanceCount));
        glUniform1i(this->m_computeNumMeshesLoc, static_cast<int>(this->m_meshInfos.size()));
        glUniform1f(this->m_computeEraseRadiusLoc, this->m_eraseRadius);
        glUniform3fv(this->m_computeSlimePosLoc, 1, glm::value_ptr(slimePos));

        const GLuint groupSize = 256u;
        const GLuint numGroups = static_cast<GLuint>((this->m_totalInstanceCount + groupSize - 1) / groupSize);
        glDispatchCompute(numGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

void RenderingOrderExp::renderViewport(const Camera* camera, const glm::vec3& slimePos, const int viewportX, const int viewportY, const int viewportWidth, const int viewportHeight) {
        this->m_renderer->setCamera(camera->projMatrix(), camera->viewMatrix(), camera->viewOrig());
        this->m_renderer->setViewport(viewportX, viewportY, viewportWidth, viewportHeight);
        this->m_renderer->setShadingModel(OPENGL::ShadingModelType::PROCEDURAL_GRID);
        this->m_horizontalGround->render();

        if (camera == this->m_godCamera) {
                this->m_renderer->setShadingModel(OPENGL::ShadingModelType::UNLIT);
                this->m_viewFrustum->render();
        }

        this->renderSlime(camera, slimePos);
        this->renderFoliage(camera);
}

void RenderingOrderExp::renderFoliage(const Camera* camera) {
        if (this->m_foliageShader == nullptr || this->m_totalInstanceCount == 0u) {
                return;
        }
        glUseProgram(this->m_foliageShader->programId());
        glUniformMatrix4fv(this->m_foliageViewLoc, 1, GL_FALSE, glm::value_ptr(camera->viewMatrix()));
        glUniformMatrix4fv(this->m_foliageProjLoc, 1, GL_FALSE, glm::value_ptr(camera->projMatrix()));
        glUniform3fv(this->m_foliageCameraPosLoc, 1, glm::value_ptr(camera->viewOrig()));

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, VISIBLE_INSTANCE_BINDING, this->m_visibleInstanceSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_COMMAND_BINDING, this->m_drawCommandSSBO);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, this->m_foliageTextureArray);

        glBindVertexArray(this->m_foliageVao);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, this->m_drawCommandSSBO);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(this->m_meshInfos.size()), 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void RenderingOrderExp::renderSlime(const Camera* camera, const glm::vec3& slimePos) {
        if (this->m_slimeShader == nullptr || this->m_slimeIndexCount == 0) {
                return;
        }
        glUseProgram(this->m_slimeShader->programId());
        glm::mat4 model = glm::translate(glm::mat4(1.0f), slimePos);
        glUniformMatrix4fv(this->m_slimeModelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(this->m_slimeViewLoc, 1, GL_FALSE, glm::value_ptr(camera->viewMatrix()));
        glUniformMatrix4fv(this->m_slimeProjLoc, 1, GL_FALSE, glm::value_ptr(camera->projMatrix()));
        glUniform3fv(this->m_slimeCameraPosLoc, 1, glm::value_ptr(camera->viewOrig()));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, this->m_slimeTexture);
        glBindVertexArray(this->m_slimeVao);
        glDrawElements(GL_TRIANGLES, this->m_slimeIndexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderingOrderExp::updatePlayerCameraMovement() {
        glm::vec3 move(0.0f);
        if (this->m_moveForward) {
                move.z -= this->m_cameraForwardSpeed;
        }
        if (this->m_moveBackward) {
                move.z += this->m_cameraForwardSpeed;
        }
        this->m_cameraForwardMagnitude = move;

        const float rotateSpeed = 0.01f;
        if (this->m_rotateLeft) {
                this->m_playerCamera->rotateLookCenterAccordingToViewOrg(rotateSpeed);
        }
        if (this->m_rotateRight) {
                this->m_playerCamera->rotateLookCenterAccordingToViewOrg(-rotateSpeed);
        }
}

void RenderingOrderExp::updateGodCameraTrackball() {
        const float radius = this->m_godCamera->distance();
        const float cosPitch = std::cos(this->m_trackballPitch);
        glm::vec3 dir(
                std::sin(this->m_trackballYaw) * cosPitch,
                std::sin(this->m_trackballPitch),
                std::cos(this->m_trackballYaw) * cosPitch
        );
        const glm::vec3 eye = this->m_godCameraTarget - dir * radius;
        this->m_godCamera->setLookCenter(this->m_godCameraTarget);
        this->m_godCamera->setViewOrg(eye);
        this->m_godCamera->update();
}

void RenderingOrderExp::handleKey(const int key, const int action) {
        const bool pressed = action != GLFW_RELEASE;
        switch (key) {
        case GLFW_KEY_W:
                this->m_moveForward = pressed;
                break;
        case GLFW_KEY_S:
                this->m_moveBackward = pressed;
                break;
        case GLFW_KEY_A:
                this->m_rotateLeft = pressed;
                break;
        case GLFW_KEY_D:
                this->m_rotateRight = pressed;
                break;
        default:
                break;
        }
}

void RenderingOrderExp::handleMouseButton(const int button, const int action, const double cursorX, const double cursorY) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
                if (action == GLFW_PRESS) {
                        this->m_trackballDragging = true;
                        this->m_lastCursor = glm::vec2(static_cast<float>(cursorX), static_cast<float>(cursorY));
                }
                else if (action == GLFW_RELEASE) {
                        this->m_trackballDragging = false;
                }
        }
}

void RenderingOrderExp::handleCursor(const double cursorX, const double cursorY) {
        if (this->m_trackballDragging == false) {
                return;
        }
        const glm::vec2 current(static_cast<float>(cursorX), static_cast<float>(cursorY));
        const glm::vec2 delta = current - this->m_lastCursor;
        this->m_lastCursor = current;
        const float sensitivity = 0.005f;
        this->m_trackballYaw -= delta.x * sensitivity;
        this->m_trackballPitch -= delta.y * sensitivity;
        this->m_trackballPitch = glm::clamp(this->m_trackballPitch, -1.3f, 1.3f);
        this->updateGodCameraTrackball();
}

void RenderingOrderExp::handleScroll(const double yoffset) {
        this->m_godCamera->distanceOffset(static_cast<float>(-yoffset));
        this->updateGodCameraTrackball();
}

}
