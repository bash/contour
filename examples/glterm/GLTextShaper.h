#pragma once

#include "FontManager.h"
#include "Shader.h"

#include <glm/glm.hpp>
#include <GL/glew.h>

#include <array>
#include <unordered_map>
#include <vector>

class GLTextShaper {
  public:
    GLTextShaper(Font& _regularFont, glm::mat4 const& _projection);
    ~GLTextShaper();

    void setProjection(glm::mat4 const& _projectionMatrix);

    void render(
        glm::ivec2 _pos,
        std::vector<char32_t> const& _chars,
        glm::vec4 const& _color,
        FontStyle _style);

  private:
    struct Glyph {
        GLuint textureID;
        glm::ivec2 size;      // glyph size
        glm::ivec2 bearing;   // offset from baseline to left/top of glyph
        unsigned height;
        unsigned descender;
        unsigned advance;     // offset to advance to next glyph in line.

        ~Glyph();
    };

    Glyph& getGlyphByIndex(unsigned long _index, FontStyle _style);

    static std::string const& fragmentShaderCode();
    static std::string const& vertexShaderCode();

  private:
    std::array<std::unordered_map<unsigned /*glyph index*/, Glyph>, 4> cache_;
    Font& regularFont_;
    std::vector<Font::GlyphPosition> glyphPositions_;
    GLuint vbo_;
    GLuint vao_;
    glm::mat4 projectionMatrix_;
    Shader shader_;
};

