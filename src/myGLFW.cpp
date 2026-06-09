#include "myGLFW.hpp"

#include "../dependencies/glm/gtc/matrix_transform.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H

#include <iomanip>

float myGLFW::textWidthPx(const std::string& text, float scale) const {
    float widthPx = 0.0f;
    for (unsigned char c : text) {
        if (c >= _glyphs.size()) {
            continue;
        }
        widthPx += float(_glyphs[c].advance >> 6) * scale;
    }
    return widthPx;
}

void myGLFW::drawText(const std::string& text, float x, float y, float scale, const glm::vec3& color) {
    if (!_fpsEnabled || !_fpsProgram) {
        return;
    }

    const glm::mat4 projection = glm::ortho(0.0f, float(_width), 0.0f, float(_height));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(_fpsProgram);
    glUniformMatrix4fv(_fpsProjectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(_fpsColorLoc, color.x, color.y, color.z);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(_fpsVAO);

    float penX = x;
    for (unsigned char c : text) {
        if (c >= _glyphs.size()) {
            continue;
        }

        const Glyph& glyph = _glyphs[c];
        if (!glyph.textureID) {
            continue;
        }

        const float xpos = penX + float(glyph.bearing.x) * scale;
        const float ypos = y - float(glyph.size.y - glyph.bearing.y) * scale;
        const float w = float(glyph.size.x) * scale;
        const float h = float(glyph.size.y) * scale;

        const float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, glyph.textureID);
        glBindBuffer(GL_ARRAY_BUFFER, _fpsVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        penX += float(glyph.advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
}

bool myGLFW::setupFPSCounter(const std::string& fontPath, int pixelSize) {
    _fpsProgram = makeProgram(
        std::string(SHADER_DIR) + "/fps_text_vertex.glsl",
        std::string(SHADER_DIR) + "/fps_text_fragment.glsl"
    );

    if (!_fpsProgram) {
        std::cerr << "FPS shader setup failed.\n";
        return false;
    }

    glUseProgram(_fpsProgram);
    glUniform1i(glGetUniformLocation(_fpsProgram, "u_text"), 0);
    _fpsProjectionLoc = glGetUniformLocation(_fpsProgram, "u_projection");
    _fpsColorLoc = glGetUniformLocation(_fpsProgram, "u_textColor");

    glGenVertexArrays(1, &_fpsVAO);
    glGenBuffers(1, &_fpsVBO);
    glBindVertexArray(_fpsVAO);
    glBindBuffer(GL_ARRAY_BUFFER, _fpsVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library\n";
        return false;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font at " << fontPath << "\n";
        FT_Done_FreeType(ft);
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(std::max(8, pixelSize)));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < _glyphs.size(); ++c) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            continue;
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        _glyphs[c] = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    _fpsEnabled = true;
    return true;
}

void myGLFW::drawFPSCounter(float fpsValue) {
    if (!_fpsEnabled) {
        return;
    }

    std::ostringstream label;
    label << "FPS " << std::fixed << std::setprecision(1) << fpsValue;

    const float scale = 0.8f;
    const float marginX = 20.0f;
    const float marginY = 34.0f;
    const float widthPx = textWidthPx(label.str(), scale);

    const float x = float(_width) - marginX - widthPx;
    const float baselineY = float(_height) - marginY;
    drawText(label.str(), x + 2.0f, baselineY - 2.0f, scale, glm::vec3(0.05f, 0.06f, 0.08f));
    drawText(label.str(), x, baselineY, scale, glm::vec3(0.95f, 0.97f, 1.0f));
}
