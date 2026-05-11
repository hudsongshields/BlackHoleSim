#pragma once
#include "config.h"

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
    GLuint _shader   = 0;
    GLuint _vao = 0, _vbo = 0;
    GLuint _sceneTex = 0;
    int    _width, _height;

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

public:
    myGLFW(int width, int height) : _width(width), _height(height) {
        if (!glfwInit()) { std::cerr << "GLFW init failed\n"; std::exit(-1); }
        _window = std::shared_ptr<GLFWwindow>(
            glfwCreateWindow(width, height, "Raymarch Sphere", nullptr, nullptr),
            glfwDestroyWindow
        );
        if (!_window) { std::cerr << "Window creation failed\n"; glfwTerminate(); std::exit(-1); }
        glfwMakeContextCurrent(_window.get());
        glfwSetInputMode(_window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "OpenGL load failed\n"; glfwTerminate(); std::exit(-1);
        }
    }

    ~myGLFW() {
        if (_shader)   glDeleteProgram(_shader);
        if (_vao)      glDeleteVertexArrays(1, &_vao);
        if (_vbo)      glDeleteBuffers(1, &_vbo);
        if (_sceneTex) glDeleteTextures(1, &_sceneTex);
        _window.reset();
        glfwTerminate();
    }

    // Bind camera and register input callbacks
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
        _shader = glCreateProgram();
        glAttachShader(_shader, vertex); glAttachShader(_shader, fragment);
        glLinkProgram(_shader);
        GLint ok; glGetProgramiv(_shader, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetProgramInfoLog(_shader, 512, nullptr, log);
            std::cerr << "Shader link error:\n" << log << "\n";
        }
        glDeleteShader(vertex); glDeleteShader(fragment);
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

    void setupSceneTexture(const std::string& uniformName = "u_scene") {
        glGenTextures(1, &_sceneTex);
        glBindTexture(GL_TEXTURE_2D, _sceneTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, _width, _height, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUseProgram(_shader);
        glUniform1i(glGetUniformLocation(_shader, uniformName.c_str()), 0);
    }

    void uploadFramebuffer(const std::vector<vec3>& fb) {
        glBindTexture(GL_TEXTURE_2D, _sceneTex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, GL_RGB, GL_FLOAT, fb.data());
    }

    void draw() {
        glUseProgram(_shader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _sceneTex);
        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
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