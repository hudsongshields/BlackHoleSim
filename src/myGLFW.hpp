#pragma once
#include "config.hpp"
#include "stb_image.h"

// CameraData - owns all camera and mouse state
// ---------------------------------------------------------------------------
struct CameraData {
    glm::vec3 camPos;
    glm::vec3 worldUp;
    glm::vec3 camForward;
    glm::vec3 camRight;
    glm::vec3 camUp;
    float     yaw;
    float     pitch;
    float     deltaTime;
    double    lastX;
    double    lastY;
    bool      firstMouse;

    CameraData(
        glm::vec3 pos     = { 0.0f, 0.0f, 3.0f },
        glm::vec3 up      = { 0.0f, 1.0f, 0.0f },
        float     yawIn   = 90.f,
        float     pitchIn = 0.f
    )
        : camPos(pos), worldUp(up)
        , yaw(yawIn), pitch(pitchIn)
        , deltaTime(0.016f)
        , lastX(0.0), lastY(0.0), firstMouse(true)
    { updateCameraVectors(); }

    void init(int width, int height) {
        lastX = width  / 2.0;
        lastY = height / 2.0;
        firstMouse = true;
    }

    void updateCameraVectors() {
        float y = glm::radians(yaw);
        float p = glm::radians(pitch);
        glm::vec3 f {
            std::cos(y) * std::cos(p),
            std::sin(p),
            std::sin(y) * std::cos(p)
        };
        camForward = glm::normalize(f);
        camRight   = glm::normalize(glm::cross(camForward, worldUp));
        camUp      = glm::normalize(glm::cross(camRight,   camForward));
    }
};



// myGLFW - manages window, GL context, shaders, fullscreen triangle, texture
// ---------------------------------------------------------------------------
class myGLFW {
private:
    std::shared_ptr<GLFWwindow> _window;
    CameraData* _camData = nullptr;
    GLuint _sceneProgram = 0;
    GLuint _extractProgram = 0;
    GLuint _blurProgram = 0;
    GLuint _finalProgram = 0;
    GLuint _vao = 0, _vbo = 0;
    GLuint _sceneTex = 0;
    GLuint _compositeTex = 0;
    GLuint _skyboxTex = 0;
    GLuint _compositeFBO = 0;
    GLuint _pingpongFBO[2] = {0, 0};
    GLuint _pingpongTex[2] = {0, 0};
    int    _width, _height;

    bool  _bloomEnabled = false;
    float _bloomThreshold = 1.0f;
    float _bloomStrength = 0.85f;
    float _exposure = 1.0f;
    int   _blurPasses = 8;

    GLint _extractThresholdLoc = -1;
    GLint _blurHorizontalLoc = -1;
    GLint _finalBloomStrengthLoc = -1;
    GLint _finalExposureLoc = -1;

    static GLuint makeModule(const std::string& path, GLenum type) {
        std::ifstream file(path);
        std::stringstream buf;
        buf << file.rdbuf();
        std::string src = buf.str();
        if (src.empty())
            std::cerr << "Shader not found: " << path << "\n";
        const char* cstr = src.c_str();
        GLuint mod = glCreateShader(type);
        glShaderSource(mod, 1, &cstr, nullptr);
        glCompileShader(mod);
        GLint ok; glGetShaderiv(mod, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetShaderInfoLog(mod, 512, nullptr, log);
            std::cerr << "Shader compile error:\n" << log << "\n";
        }
        return mod;
    }

    static GLuint makeProgram(const std::string& vertPath, const std::string& fragPath) {
        GLuint vertex = makeModule(vertPath, GL_VERTEX_SHADER);
        GLuint fragment = makeModule(fragPath, GL_FRAGMENT_SHADER);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);

        GLint ok = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetProgramInfoLog(program, 512, nullptr, log);
            std::cerr << "Program link error:\n" << log << "\n";
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            glDeleteProgram(program);
            return 0;
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return program;
    }

    void drawNoBloom() {
        glUseProgram(_sceneProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _sceneTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _skyboxTex);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    void renderCompositeScenePass() {
        glBindFramebuffer(GL_FRAMEBUFFER, _compositeFBO);
        glUseProgram(_sceneProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _sceneTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _skyboxTex);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    void renderBrightExtractPass() {
        glBindFramebuffer(GL_FRAMEBUFFER, _pingpongFBO[0]);
        glUseProgram(_extractProgram);
        glUniform1f(_extractThresholdLoc, _bloomThreshold);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _compositeTex);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    GLuint renderBlurPasses() {
        bool horizontal = true;
        GLuint inputTex = _pingpongTex[0];

        for (int i = 0; i < _blurPasses; ++i) {
            const int targetIndex = horizontal ? 1 : 0;
            glBindFramebuffer(GL_FRAMEBUFFER, _pingpongFBO[targetIndex]);
            glUseProgram(_blurProgram);
            glUniform1i(_blurHorizontalLoc, horizontal ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, inputTex);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            inputTex = _pingpongTex[targetIndex];
            horizontal = !horizontal;
        }

        return inputTex;
    }

    void renderFinalBloomPass(GLuint bloomTex) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(_finalProgram);
        glUniform1f(_finalBloomStrengthLoc, _bloomStrength);
        glUniform1f(_finalExposureLoc, _exposure);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _compositeTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomTex);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

public:
    myGLFW(int width, int height) : _width(width), _height(height) {
        if (!glfwInit()) { std::cerr << "GLFW init failed\n"; std::exit(-1); }
        _window = std::shared_ptr<GLFWwindow>(
            glfwCreateWindow(width, height, "Raymarch Sphere", nullptr, nullptr),
            glfwDestroyWindow
        );
        if (!_window) { std::cerr << "Window creation failed\n"; glfwTerminate(); std::exit(-1); }
        glfwMakeContextCurrent(_window.get());
        glfwSwapInterval(0);
        glfwSetInputMode(_window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "OpenGL load failed\n"; glfwTerminate(); std::exit(-1);
        }
    }

    ~myGLFW() {
        if (_sceneProgram)   glDeleteProgram(_sceneProgram);
        if (_extractProgram) glDeleteProgram(_extractProgram);
        if (_blurProgram)    glDeleteProgram(_blurProgram);
        if (_finalProgram)   glDeleteProgram(_finalProgram);
        if (_vao)      glDeleteVertexArrays(1, &_vao);
        if (_vbo)      glDeleteBuffers(1, &_vbo);
        if (_sceneTex) glDeleteTextures(1, &_sceneTex);
        if (_compositeTex) glDeleteTextures(1, &_compositeTex);
        if (_pingpongTex[0] || _pingpongTex[1]) glDeleteTextures(2, _pingpongTex);
        if (_compositeFBO) glDeleteFramebuffers(1, &_compositeFBO);
        if (_pingpongFBO[0] || _pingpongFBO[1]) glDeleteFramebuffers(2, _pingpongFBO);
        _window.reset();
        glfwTerminate();
    }

    void setCameraData(CameraData& cam) {
        _camData = &cam;
        _camData->init(_width, _height);
        glfwSetWindowUserPointer(_window.get(), _camData);
        glfwSetKeyCallback(_window.get(),       keyCallback);
        glfwSetCursorPosCallback(_window.get(), mouseCallback);
    }

    void loadShaders(const std::string& vertPath, const std::string& fragPath) {
        GLuint vertex = makeModule(vertPath, GL_VERTEX_SHADER);
        GLuint fragment = makeModule(fragPath, GL_FRAGMENT_SHADER);
        _sceneProgram = glCreateProgram();
        glAttachShader(_sceneProgram, vertex); glAttachShader(_sceneProgram, fragment);
        glLinkProgram(_sceneProgram);
        GLint ok; glGetProgramiv(_sceneProgram, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetProgramInfoLog(_sceneProgram, 512, nullptr, log);
            std::cerr << "Shader link error:\n" << log << "\n";
        }
        glDeleteShader(vertex); glDeleteShader(fragment);

        glUseProgram(_sceneProgram);
        glUniform1i(glGetUniformLocation(_sceneProgram, "u_scene"), 0);
        glUniform1i(glGetUniformLocation(_sceneProgram, "u_skybox"), 1);
    }

    void setupFullscreenTriangle() {
        float vertices[] = { -1.0f, -1.0f,  3.0f, -1.0f,  -1.0f,  3.0f };
        glGenVertexArrays(1, &_vao); glGenBuffers(1, &_vbo);
        glBindVertexArray(_vao);
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glBindVertexArray(0);
    }

    unsigned int setupSceneTexture(const std::string& uniformName = "u_scene") {
        glGenTextures(1, &_sceneTex);
        glBindTexture(GL_TEXTURE_2D, _sceneTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _width, _height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (_sceneProgram) {
            glUseProgram(_sceneProgram);
            glUniform1i(glGetUniformLocation(_sceneProgram, uniformName.c_str()), 0);
        }
        return _sceneTex;
    }

    bool setupBloomPipeline(
        const std::string& vertPath,
        const std::string& extractFragPath,
        const std::string& blurFragPath,
        const std::string& finalFragPath
    ) {
        _extractProgram = makeProgram(vertPath, extractFragPath);
        _blurProgram = makeProgram(vertPath, blurFragPath);
        _finalProgram = makeProgram(vertPath, finalFragPath);

        if (!_extractProgram || !_blurProgram || !_finalProgram) {
            std::cerr << "Bloom shader setup failed. Bloom disabled.\n";
            return false;
        }

        glUseProgram(_extractProgram);
        glUniform1i(glGetUniformLocation(_extractProgram, "u_input"), 0);
        _extractThresholdLoc = glGetUniformLocation(_extractProgram, "u_threshold");

        glUseProgram(_blurProgram);
        glUniform1i(glGetUniformLocation(_blurProgram, "u_input"), 0);
        glUniform2f(glGetUniformLocation(_blurProgram, "u_texelSize"), 1.0f / float(_width), 1.0f / float(_height));
        _blurHorizontalLoc = glGetUniformLocation(_blurProgram, "u_horizontal");

        glUseProgram(_finalProgram);
        glUniform1i(glGetUniformLocation(_finalProgram, "u_sceneColor"), 0);
        glUniform1i(glGetUniformLocation(_finalProgram, "u_bloomBlur"), 1);
        _finalBloomStrengthLoc = glGetUniformLocation(_finalProgram, "u_bloomStrength");
        _finalExposureLoc = glGetUniformLocation(_finalProgram, "u_exposure");

        glGenFramebuffers(1, &_compositeFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, _compositeFBO);

        glGenTextures(1, &_compositeTex);
        glBindTexture(GL_TEXTURE_2D, _compositeTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, _width, _height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _compositeTex, 0);

        glGenFramebuffers(2, _pingpongFBO);
        glGenTextures(2, _pingpongTex);
        for (int i = 0; i < 2; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, _pingpongFBO[i]);
            glBindTexture(GL_TEXTURE_2D, _pingpongTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, _width, _height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _pingpongTex[i], 0);
        }

        bool complete =
            glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (!complete) {
            std::cerr << "Bloom framebuffer setup failed. Bloom disabled.\n";
            return false;
        }

        _bloomEnabled = true;
        return true;
    }

    void setBloomSettings(float threshold, float strength, float exposure, int blurPasses) {
        _bloomThreshold = threshold;
        _bloomStrength = strength;
        _exposure = exposure;
        _blurPasses = std::max(1, blurPasses);
    }

    unsigned int loadCubemap(std::vector<std::string> faces)
    {
        glGenTextures(1, &_skyboxTex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _skyboxTex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        int width, height, nrChannels;
        for (unsigned int i = 0; i < faces.size(); i++)
        {
            unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
            if (data)
            {
                GLenum format = GL_RGB;
                if (nrChannels == 3) {
                    format = GL_RGB;
                } else if (nrChannels == 4) {
                    format = GL_RGBA;
                } else {
                    std::cout << "Unsupported cubemap channel count " << nrChannels
                              << " at path: " << faces[i] << std::endl;
                    stbi_image_free(data);
                    continue;
                }

                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                            0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data
                );
                stbi_image_free(data);
            }
            else
            {
                std::cout << "Cubemap tex failed to load at path: " << faces[i] << std::endl;
                stbi_image_free(data);
            }
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        if (_sceneProgram) {
            glUseProgram(_sceneProgram);
            glUniform1i(glGetUniformLocation(_sceneProgram, "u_skybox"), 1);
        }
        return _skyboxTex;
    }

    unsigned int getSkybox() const { return _skyboxTex; }

    void uploadFramebuffer(const std::vector<vec4> *fb) {
        glBindTexture(GL_TEXTURE_2D, _sceneTex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, GL_RGBA, GL_FLOAT, fb->data());
    }

    void uploadFromPBO(GLuint pbo) {
        glBindTexture(GL_TEXTURE_2D, _sceneTex);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, GL_RGBA, GL_FLOAT, nullptr);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    void draw() {
        glBindVertexArray(_vao);

        if (!_bloomEnabled || !_extractProgram || !_blurProgram || !_finalProgram) {
            drawNoBloom();
            return;
        }

        glViewport(0, 0, _width, _height);
        renderCompositeScenePass();
        renderBrightExtractPass();
        GLuint blurredBloomTex = renderBlurPasses();
        renderFinalBloomPass(blurredBloomTex);
    }

    bool shouldClose() const { return glfwWindowShouldClose(_window.get()); }
    void pollEvents() { glfwPollEvents(); }
    void swapBuffers() { glfwSwapBuffers(_window.get()); }
    void clear(float r = 0.f, float g = 0.f, float b = 0.2f, float a = 1.f) {
        glClearColor(r, g, b, a); glClear(GL_COLOR_BUFFER_BIT);
    }


    GLFWwindow* window() const { return _window.get(); }

    static void keyCallback(GLFWwindow* win, int key, int, int action, int) {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
        CameraData& cam = *(CameraData*)glfwGetWindowUserPointer(win);
        float speed = 5.0f * cam.deltaTime;
        if (glfwGetKey(win, GLFW_KEY_W)          == GLFW_PRESS) cam.camPos += cam.camForward * speed;
        if (glfwGetKey(win, GLFW_KEY_S)          == GLFW_PRESS) cam.camPos -= cam.camForward * speed;
        if (glfwGetKey(win, GLFW_KEY_A)          == GLFW_PRESS) cam.camPos -= cam.camRight   * speed;
        if (glfwGetKey(win, GLFW_KEY_D)          == GLFW_PRESS) cam.camPos += cam.camRight   * speed;
        if (glfwGetKey(win, GLFW_KEY_SPACE)      == GLFW_PRESS) cam.camPos -= cam.worldUp    * speed;
        if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) cam.camPos += cam.worldUp    * speed;
    }

    static void mouseCallback(GLFWwindow* win, double xpos, double ypos) {
        CameraData& cam = *(CameraData*)glfwGetWindowUserPointer(win);
        if (cam.firstMouse) { cam.lastX = xpos; cam.lastY = ypos; cam.firstMouse = false; return; }
        float xoff = float(xpos - cam.lastX) * 0.1f;
        float yoff = float(ypos - cam.lastY) * 0.1f;
        cam.lastX = xpos; cam.lastY = ypos;
        cam.yaw  += xoff;
        cam.pitch = glm::clamp(cam.pitch + yoff, -89.f, 89.f);
        cam.updateCameraVectors();
    }
};