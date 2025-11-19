#pragma once

#include <vector>
#include <array>

#include <glad/glad.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include "../Rendering/RendererBase.h"
#include "../Scene/RViewFrustum.h"
#include "../Scene/RHorizonGround.h"
#include "../Scene/Trajectory.h"

namespace INANOA {
        class RenderingOrderExp
        {
        public:
                RenderingOrderExp();
                virtual ~RenderingOrderExp();

        public:
                bool init(const int w, const int h);
                void resize(const int w, const int h);
                void update();
                void render();

                void handleKey(const int key, const int action);
                void handleMouseButton(const int button, const int action, const double cursorX, const double cursorY);
                void handleCursor(const double cursorX, const double cursorY);
                void handleScroll(const double yoffset);

        private:
                void initializeSceneResources();
                void initializeFoliage();
                void initializeSlime();
                void initializeTextureArray();
                void initializeInstanceBuffers();
                void initializeComputeShader();
                void uploadDrawCommands();
                void dispatchCullingCompute(const Camera* playerCam, const glm::vec3& slimePos);
                void renderViewport(const Camera* camera, const glm::vec3& slimePos, const int viewportX, const int viewportY, const int viewportWidth, const int viewportHeight);
                void renderFoliage(const Camera* camera);
                void renderSlime(const Camera* camera, const glm::vec3& slimePos);
                void updatePlayerCameraMovement();
                void updateGodCameraTrackball();

                SCENE::RViewFrustum* m_viewFrustum = nullptr;
                SCENE::EXPERIMENTAL::HorizonGround* m_horizontalGround = nullptr;

                Camera* m_playerCamera = nullptr;
                Camera* m_godCamera = nullptr;

                glm::vec3 m_cameraForwardMagnitude;
                float m_cameraForwardSpeed;
                bool m_moveForward = false;
                bool m_moveBackward = false;
                bool m_rotateLeft = false;
                bool m_rotateRight = false;

                bool m_trackballDragging = false;
                glm::vec2 m_lastCursor = glm::vec2(0.0f);
                float m_trackballYaw = 0.0f;
                float m_trackballPitch = 0.0f;
                glm::vec3 m_godCameraTarget = glm::vec3(0.0f);

                int m_frameWidth;
                int m_frameHeight;

                OPENGL::RendererBase* m_renderer = nullptr;

                struct MeshInfo;
                struct DrawCommand;
                struct Vertex;

                std::vector<MeshInfo> m_meshInfos;
                std::vector<DrawCommand> m_drawCommands;

                GLuint m_foliageVao = 0u;
                GLuint m_foliageVbo = 0u;
                GLuint m_foliageIbo = 0u;
                GLuint m_foliageTextureArray = 0u;

                GLuint m_slimeVao = 0u;
                GLuint m_slimeVbo = 0u;
                GLuint m_slimeIbo = 0u;
                GLuint m_slimeTexture = 0u;
                GLsizei m_slimeIndexCount = 0;

                GLuint m_rawInstanceSSBO = 0u;
                GLuint m_visibleInstanceSSBO = 0u;
                GLuint m_drawCommandSSBO = 0u;
                GLuint m_instanceStateSSBO = 0u;

                size_t m_totalInstanceCount = 0u;


                GLuint m_rawInstanceSSBO = 0u;
                GLuint m_visibleInstanceSSBO = 0u;
                GLuint m_drawCommandSSBO = 0u;
                GLuint m_instanceStateSSBO = 0u;

                size_t m_totalInstanceCount = 0u;

                OPENGL::ShaderProgram* m_foliageShader = nullptr;
                OPENGL::ShaderProgram* m_slimeShader = nullptr;
                OPENGL::ShaderProgram* m_computeShader = nullptr;

                GLint m_foliageModelLoc = -1;
                GLint m_foliageViewLoc = -1;
                GLint m_foliageProjLoc = -1;
                GLint m_foliageCameraPosLoc = -1;
                GLint m_foliageLightDirLoc = -1;

                GLint m_slimeModelLoc = -1;
                GLint m_slimeViewLoc = -1;
                GLint m_slimeProjLoc = -1;
                GLint m_slimeCameraPosLoc = -1;
                GLint m_slimeLightDirLoc = -1;

                GLint m_computeViewLoc = -1;
                GLint m_computeProjLoc = -1;
                GLint m_computeTotalInstanceLoc = -1;
                GLint m_computeNumMeshesLoc = -1;
                GLint m_computeEraseRadiusLoc = -1;
                GLint m_computeSlimePosLoc = -1;

                float m_eraseRadius = 3.0f;

                SCENE::EXPERIMENTAL::Trajectory m_slimeTrajectory;
        };

}
