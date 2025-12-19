#include "config.h"
#include "LightRay.h"
#include "Schwarzchild.h"
#include <iostream>


unsigned int make_shader(const std::string& vertex_filepath, const std::string& fragment_filepath);
unsigned int make_module(const std::string& filepath, unsigned int module_type);
constexpr float GM = 1.0f;


float Sphere_SDF(vec3 p, vec3 center, float radius) {
    return glm::length(p - center) - radius;
}
void updateCameraVectors(glm::vec3& camForward,glm::vec3& camRight, glm::vec3& camUp, float yawDeg, float pitchDeg, const glm::vec3& worldUp){
    float yaw   = glm::radians(yawDeg);
    float pitch = glm::radians(pitchDeg);

    glm::vec3 front;
    front.x = std::cos(yaw) * std::cos(pitch);
    front.y = std::sin(pitch);
    front.z = std::sin(yaw) * std::cos(pitch);

    camForward = glm::normalize(front);
    camRight   = glm::normalize(glm::cross(camForward, worldUp));
    camUp      = glm::normalize(glm::cross(camRight, camForward));
}
vec3 computeRayDirWorld(float u, float v, const vec3& camPos, const vec3& camForward, const vec3& camRight, const vec3& camUp, float fov, float width, float height) {
    float aspectRatio = width / height;
    vec2 ndc = vec2(
        (2.0f * u) - 1.0f,
        1.0f - (2.0f * v)
    );

    vec3 rayDirCamera = glm::normalize(vec3(ndc.x * aspectRatio, ndc.y, -1.0f));

    // Convert to world space
    vec3 rayDirWorld = glm::normalize(
        rayDirCamera.x * camRight +
        rayDirCamera.y * camUp +
        rayDirCamera.z * camForward
    );

    return rayDirWorld;
}
vec3 traceRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& sphereCenter, float sphereRadius, float diskRadius) {

    const float tMax = 50.0f;

    float hitRadius;
    float dt = 0.1f;
    float height = 0.015f * diskRadius;
    vec3 r = rayOrigin;

    Schwarzschild ray(r, rayDir, GM, height);
    for (int i {0}; i < 128; ++i) {
        LightRay::CollisionType collision = ray.checkCollision(dt, sphereRadius, diskRadius);

        if (ray.t > tMax) break;
        switch (collision) {
            case LightRay::BLACKHOLE:
                return vec3(0.0f, 0.0f, 0.0f);

            case LightRay::DISK: {
                float rNormalized = glm::clamp((ray.getR() - sphereRadius) / (diskRadius - sphereRadius), 0.0f, 1.0f);
                vec3 innerColor = vec3(1.0, 0.35, 0.1);  // reddish inner
                vec3 outerColor = vec3(1.0, 0.75, 0.25); // more orange outer
                vec3 baseColor  = mix(innerColor, outerColor, rNormalized);


                // Narrow bright ring near the inner edge (photon ring-ish)
                float ring1 = exp(-pow((rNormalized - 0.15) / 0.03, 2.0));
                float ring2 = exp(-pow((rNormalized - 0.30) / 0.06, 2.0));
                float ring3 = exp(-pow((rNormalized - 0.51) / 0.10, 2.0));
                float ring4 = exp(-pow((rNormalized - 0.80) / 0.16, 2.0));
                float radialGlow = ring1 + 0.8 * ring2 + 0.5 * ring3 + 0.3 * ring4;

                baseColor *= radialGlow;
                return baseColor;
            }
            case LightRay::NONE:
                break;
        }
    }
    
    vec3 backgroundTop = vec3(0.0f, 0.0f, 0.0f);
    vec3 backgroundBot = vec3(0.1f, 0.1f, 0.25f);
    float distance = glm::clamp(ray.y_pos(), 0.0f, 1.0f);
    vec3 background = glm::mix(backgroundBot, backgroundTop, distance);
    return background;
}
void marchColumns(
    int xBegin,
    int xEnd,
    int width,
    int height,
    const vec3& camPos,
    const vec3& camForward,
    const vec3& camRight,
    const vec3& camUp,
    float fov,
    const vec3& sphereCenter,
    float sphereRadius,
    float diskRadius,
    std::vector<vec3>& framebuffer // shared
) {
    for (int i = xBegin; i < xEnd; ++i) {
        for (int j = 0; j < height; ++j) {
            float u = (i + 0.5f) / static_cast<float>(width);
            float v = (j + 0.5f) / static_cast<float>(height);
            vec3 rayDir = computeRayDirWorld(u, v, camPos, camForward, camRight, camUp, fov, width, height);
                    
            vec3 color = traceRay(camPos, rayDir, sphereCenter, sphereRadius, diskRadius);
            framebuffer[j * width + i] = color;
        }
    }
}

// Camera state
float yaw   = 90.0f;
float pitch = 0.0f;

// Mouse state
double lastX, lastY;
bool firstMouse = true;

int main() {
    // --- Initialize GLFW and create window ---
    GLFWwindow* window;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    const int width = 480;
    const int height = 270;
    window = glfwCreateWindow(width, height, "Raymarch Sphere", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    lastX = width / 2.0;
    lastY = height / 2.0;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        return -1;
    }

    // --- Setup OpenGL state ---
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Fullscreen triangle vertices
    float vertices[] = {
        -1.0f, -1.0f,
        3.0f, -1.0f,
        -1.0f,  3.0f
    };

    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // matches layout(location = 0) in vertex shader
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    unsigned int shaderProgram = make_shader(
        "../../src/shaders/vertex.glsl",
        "../../src/shaders/fragment.glsl"
    );

    glUseProgram(shaderProgram);


    // Static camera scene parameters
    glm::vec3 camPos    = glm::vec3(0.0f, 7.0f, 7.0f);
    glm::vec3 camTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 worldUp   = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 camForward = glm::normalize(camTarget - camPos);
    glm::vec3 camRight   = glm::normalize(glm::cross(camForward, worldUp));
    glm::vec3 camUp      = glm::normalize(glm::cross(camRight, camForward));

    float fov = glm::radians(45.0f);


    // Black hole and accretion disk parameters
    glm::vec3 sphereCenter          =   glm::vec3(0.0f, 0.0f, 0.0f);
    constexpr float sphereRadius    =   2.0f * GM; // r_schwarschild = 2GM
    constexpr float diskRadius      =   sphereRadius * 2.5f;


    // Create texture to hold the rendered scene
    GLuint sceneTex;
    glGenTextures(1, &sceneTex); // gejenerate texture ID
    glBindTexture(GL_TEXTURE_2D, sceneTex); // bind as 2D texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F,
                width, height, 0,
                GL_RGB, GL_FLOAT, nullptr); // allocate empty texture

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // no interpolation
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // no interpolation
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // prevent wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // prevent wrapping

    glUseProgram(shaderProgram);
    GLint sceneLoc = glGetUniformLocation(shaderProgram, "u_scene");
    glUniform1i(sceneLoc, 0);

    std::vector<vec3> framebuffer(width * height);

    // --- Main loop ---
    int    fpsFrames   = 0;
    float  lastFPSTime = glfwGetTime();
    float lastTime = (float)glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        fpsFrames++;
        glfwPollEvents();

        glClearColor(0.0f, 0.0f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        float currentTime = (float)glfwGetTime();
        float time = currentTime - lastTime;
        lastTime = currentTime;

        // tracks fps (5 seconds for better accuracy)
        float delta = currentTime - lastFPSTime;
        if (delta >= 5.0) {
            std::cout << "FPS: " << fpsFrames / delta << std::endl;
            fpsFrames = 0;
            lastFPSTime = currentTime;
        }

        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = float(xpos - lastX);
        float yoffset = float(lastY - ypos); // reversed: screen Y down, pitch up
        lastX = xpos;
        lastY = ypos;

        float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw   -= xoffset;
        pitch += yoffset;

        // clamp pitch so you don't flip upside-down
        pitch = glm::clamp(pitch, -89.0f, 89.0f);

        // Recompute camera basis from yaw/pitch
        updateCameraVectors(camForward, camRight, camUp,
                            yaw, pitch, worldUp);


        // --- simple camera movement ---
        float speed = 5.0f;
        float camSpeed = speed * time;

        // move in world axes
        // move in circular path around origin
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camPos -= camForward * camSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camPos += camForward * camSpeed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camPos -= camRight * camSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camPos += camRight * camSpeed;

        // up/down along world Y:
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camPos -= worldUp * camSpeed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            camPos += worldUp * camSpeed;

        // calculating number of concurrent threads
        unsigned int numThreads;
        numThreads = std::thread::hardware_concurrency();

        int columnsPerThread = width / numThreads;
        int remainingColumns = width % numThreads;
        std::vector<std::thread> thread_array;

        for (std::size_t t = 0; t < numThreads; ++t) {
        
            int xBegin = t * columnsPerThread + std::min(static_cast<int>(t), remainingColumns); // starting column for this thread (adds one if within remainder)
            int xEnd   = xBegin + columnsPerThread + (t < remainingColumns ? 1 : 0); // adds one more column if within remainder
            thread_array.emplace_back(
                marchColumns,
                xBegin, xEnd,
                width, height,
                camPos, camForward, camRight, camUp,
                fov,
                sphereCenter, sphereRadius, diskRadius,
                std::ref(framebuffer)
            );
        }

        for (auto& thread : thread_array) {
            thread.join();
        }
        thread_array.clear();

        // --- Upload framebuffer to texture ---
        glBindTexture(GL_TEXTURE_2D, sceneTex);
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        0, 0, width, height,
                        GL_RGB, GL_FLOAT,
                        framebuffer.data());

        // --- Draw fullscreen triangle that samples u_scene ---
        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneTex);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        }
    // Clean resource allocation
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &sceneTex);
    glDeleteProgram(shaderProgram);

    // Close window
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


unsigned int make_shader(const std::string& vertex_filepath, const std::string& fragment_filepath) {

    std::vector<unsigned int> modules;
    modules.push_back(make_module(vertex_filepath, GL_VERTEX_SHADER));
    modules.push_back(make_module(fragment_filepath, GL_FRAGMENT_SHADER));

    unsigned int shaderProgram = glCreateProgram();
    for (unsigned int shaderModule : modules) {
        glAttachShader(shaderProgram, shaderModule);
    }
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char errorLog[512];
        glGetProgramInfoLog(shaderProgram, 1024, NULL, errorLog);
        std::cout << "Shader Program Linking Failed:\n" << errorLog << std::endl;
    }

    for (unsigned int shaderModule : modules) {
        glDeleteShader(shaderModule);
    }

    return shaderProgram;
}

unsigned int make_module(const std::string& filepath, unsigned int module_type) {

    std::ifstream file;
    std::stringstream bufferedLines;
    std::string line;

    file.open(filepath);
    while (std::getline(file, line)) {
        bufferedLines << line << std::endl;
    }
    std::string shaderSource = bufferedLines.str();

    if (shaderSource.empty()) {
        std::cerr << "Shader file is empty or could not be read: " << filepath << "\n";
    }

    const char* shaderSrc = shaderSource.c_str();
    bufferedLines.str("");
    file.close();

    unsigned int shaderModule = glCreateShader(module_type);
    glShaderSource(shaderModule, 1, &shaderSrc, NULL);
    glCompileShader(shaderModule);

    int success;
    glGetShaderiv(shaderModule, GL_COMPILE_STATUS, &success);
    if (!success) {
        char errorLog[512];
        glGetShaderInfoLog(shaderModule, 1024, NULL, errorLog);
        std::cout << "Shader Module Compilation Failed:\n" << errorLog << std::endl;
    }
    return shaderModule;
}