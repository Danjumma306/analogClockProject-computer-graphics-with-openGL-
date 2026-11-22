#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"          

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <filesystem>

const double PI = 3.14159265358979323846;
const int    WINDOW_SIZE = 600;
const float  CLOCK_RADIUS = 0.85f;
const float FACE_DEPTH = 0.12f;
const float GLASS_DEPTH = 0.25f;
const float CASE_RADIUS = 0.94f;
const float DEFAULT_YAW = -23.0f;
const float DEFAULT_VIEW_PITCH = 13.0f;

float yaw = DEFAULT_YAW;
float pitch = DEFAULT_VIEW_PITCH;
bool dragging = false;
double dragX = 0.0;
double dragY = 0.0;
std::time_t timeOffset = 0;
int framebufferWidth = 800;
int framebufferHeight = 600;

struct TimeButton {
    const char* label;
    int seconds;
};
const TimeButton timeButtons[] = {
    { "Hour +", 3600 }, { "Hour -", -3600 },
    { "Minute +", 60 }, { "Minute -", -60 },
    { "Second +", 1 }, { "Second -", -1 }
};
GLuint uiFont = 0;
GLYPHMETRICSFLOAT uiGlyphs[128]{};

void loadUiFont() {
    const HDC device = wglGetCurrentDC();
    HFONT font = CreateFontW(-32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (!font) return;
    const HGDIOBJ previous = SelectObject(device, font);
    uiFont = glGenLists(128);
    if (uiFont && !wglUseFontOutlinesA(device, 0, 128, uiFont, 0.001f, 0.0f,
        WGL_FONT_POLYGONS, uiGlyphs)) {
        glDeleteLists(uiFont, 128);
        uiFont = 0;
    }
    SelectObject(device, previous);
    DeleteObject(font);
}

// Logo file resolved relative to the executable.
const char* LOGO_FILE = "logo.jpeg";

// Logo file and crop region for the 720 x 732 image
const float SRC_W = 720.0f;
const float SRC_H = 732.0f;

// Centered square crop: 720 x 720
const float CROP_X0 = 0.0f;
const float CROP_X1 = 720.0f;
const float CROP_Y0 = 6.0f;
const float CROP_Y1 = 700.0f;
GLuint logoTex = 0;
bool   logoLoaded = false;

const char* DAYS[] = { "SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY" };
const char* MONTHS[] = { "JANUARY","FEBRUARY","MARCH","APRIL","MAY","JUNE",
                        "JULY","AUGUST","SEPTEMBER","OCTOBER","NOVEMBER","DECEMBER" };


const float GREEN[3] = { 0.043f, 0.365f, 0.231f };


static const unsigned char FONT_DIGITS[10][5] = {
    { 0x3E,0x51,0x49,0x45,0x3E },{ 0x00,0x42,0x7F,0x40,0x00 },
    { 0x42,0x61,0x51,0x49,0x46 },{ 0x21,0x41,0x45,0x4B,0x31 },
    { 0x18,0x14,0x12,0x7F,0x10 },{ 0x27,0x45,0x45,0x45,0x39 },
    { 0x3C,0x4A,0x49,0x49,0x30 },{ 0x01,0x71,0x09,0x05,0x03 },
    { 0x36,0x49,0x49,0x49,0x36 },{ 0x06,0x49,0x49,0x29,0x1E }
};

static const unsigned char FONT_LETTERS[26][5] = {
    { 0x7E,0x11,0x11,0x11,0x7E },{ 0x7F,0x49,0x49,0x49,0x36 }, // A B
    { 0x3E,0x41,0x41,0x41,0x22 },{ 0x7F,0x41,0x41,0x22,0x1C }, // C D
    { 0x7F,0x49,0x49,0x49,0x41 },{ 0x7F,0x09,0x09,0x09,0x01 }, // zE F
    { 0x3E,0x41,0x49,0x49,0x7A },{ 0x7F,0x08,0x08,0x08,0x7F }, // G H
    { 0x00,0x41,0x7F,0x41,0x00 },{ 0x20,0x40,0x41,0x3F,0x01 }, // I J
    { 0x7F,0x08,0x14,0x22,0x41 },{ 0x7F,0x40,0x40,0x40,0x40 }, // K L
    { 0x7F,0x02,0x0C,0x02,0x7F },{ 0x7F,0x04,0x08,0x10,0x7F }, // M N
    { 0x3E,0x41,0x41,0x41,0x3E },{ 0x7F,0x09,0x09,0x09,0x06 }, // O P
    { 0x3E,0x41,0x51,0x21,0x5E },{ 0x7F,0x09,0x19,0x29,0x46 }, // Q R
    { 0x46,0x49,0x49,0x49,0x31 },{ 0x01,0x01,0x7F,0x01,0x01 }, // S T
    { 0x3F,0x40,0x40,0x40,0x3F },{ 0x1F,0x20,0x40,0x20,0x1F }, // U V
    { 0x3F,0x40,0x38,0x40,0x3F },{ 0x63,0x14,0x08,0x14,0x63 }, // W X
    { 0x07,0x08,0x70,0x08,0x07 },{ 0x61,0x51,0x49,0x45,0x43 }  // Y Z
};

void getGlyph(char c, unsigned char out[5]) {
    memset(out, 0, 5);
    if (c >= '0' && c <= '9')      memcpy(out, FONT_DIGITS[c - '0'], 5);
    else if (c >= 'A' && c <= 'Z') memcpy(out, FONT_LETTERS[c - 'A'], 5);
    else if (c == ':') { out[1] = 0x36; out[2] = 0x36; }
    else if (c == ',') { out[1] = 0x50; out[2] = 0x30; }
    else if (c == '-') { for (int i = 0; i < 5; i++) out[i] = 0x08; }
    else if (c == '+') { for (int i = 0; i < 5; i++) out[i] = 0x08; out[2] = 0x3E; }
}

float textWidth(const char* s, float px) {
    size_t n = strlen(s);
    return n ? (float)(n * 6 - 1) * px : 0.0f;
}

// Draws text centred on (cx, cy). px = size of one font "pixel" in world units.
void drawText(const char* s, float cx, float cy, float px) {
    float x = cx - textWidth(s, px) / 2.0f;
    float y = cy - 3.5f * px;
    glBegin(GL_QUADS);
    for (const char* p = s; *p; ++p) {
        unsigned char g[5];
        getGlyph(*p, g);
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                if (g[col] & (1 << row)) {
                    float x0 = x + col * px;
                    float y0 = y + (6 - row) * px;     
                    glVertex2f(x0, y0);
                    glVertex2f(x0 + px, y0);
                    glVertex2f(x0 + px, y0 + px);
                    glVertex2f(x0, y0 + px);
                }
            }
        }
        x += 6 * px;
    }
    glEnd();
}

void drawUiText(const char* text, float x, float y, float size) {
    if (!uiFont) {
        drawText(text, x, y, size / 7.0f);
        return;
    }
    float width = 0.0f;
    for (const char* p = text; *p; ++p) width += uiGlyphs[static_cast<unsigned char>(*p)].gmfCellIncX;
    glPushMatrix();
    const float horizontalSize = std::abs(size);
    glTranslatef(x - width * horizontalSize / 2.0f, y - size * 0.35f, 0.0f);
    glScalef(horizontalSize, size, horizontalSize);
    glPushAttrib(GL_LIST_BIT);
    glListBase(uiFont);
    glCallLists(static_cast<GLsizei>(strlen(text)), GL_UNSIGNED_BYTE, text);
    glPopAttrib();
    glPopMatrix();
}


void loadLogo() {
    int w, h, n;
    // Resolve assets beside the executable, regardless of the launch directory.
    wchar_t executablePath[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath, 32768);
    if (length == 0 || length >= 32768) {
        printf("Could not resolve executable path.\n");
        return;
    }
    const auto logoPath = std::filesystem::path(executablePath).parent_path() / LOGO_FILE;
    FILE* file = nullptr;
    if (_wfopen_s(&file, logoPath.c_str(), L"rb") != 0 || !file) {
        printf("Could not open logo beside executable.\n");
        return;
    }
    unsigned char* data = stbi_load_from_file(file, &w, &h, &n, 3);
    fclose(file);
    if (!data) {
        printf("Could not load %s - running without logo.\n", LOGO_FILE);
        return;
    }
    glGenTextures(1, &logoTex);
    glBindTexture(GL_TEXTURE_2D, logoTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    logoLoaded = true;
}



void drawCircle(float cx, float cy, float r, int segments = 120) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        double a = 2.0 * PI * i / segments;
        glVertex2f(cx + r * (float)cos(a), cy + r * (float)sin(a));
    }
    glEnd();
}

struct RimPoint {
    float radius;
    float depth;
};

struct Finish {
    float red;
    float green;
    float blue;
    float gloss;
    float shininess;
};

constexpr Finish enamel{ 0.045f, 0.24f, 0.16f, 0.43f, 45.0f };
constexpr Finish brass{ 0.67f, 0.43f, 0.16f, 0.76f, 54.0f };
constexpr Finish gasket{ 0.045f, 0.064f, 0.054f, 0.05f, 12.0f };
constexpr Finish dial{ 0.95f, 0.90f, 0.77f, 0.06f, 18.0f };
constexpr Finish steel{ 0.64f, 0.62f, 0.53f, 0.70f, 72.0f };
constexpr Finish backPlate{ 0.12f, 0.18f, 0.15f, 0.13f, 24.0f };
constexpr Finish coverPlastic{ 0.19f, 0.24f, 0.21f, 0.19f, 26.0f };

void setFinish(const Finish& finish) {
    const GLfloat color[] = { finish.red, finish.green, finish.blue, 1.0f };
    const GLfloat reflection[] = { finish.gloss, finish.gloss, finish.gloss, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, reflection);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, finish.shininess);
}

// Revolve a profile ordered from the rear of the case toward the dial.
template <size_t N>
void drawProfile(const RimPoint (&profile)[N], const Finish& finish) {
    setFinish(finish);
    for (size_t band = 1; band < N; ++band) {
        const RimPoint rear = profile[band - 1];
        const RimPoint front = profile[band];
        const float radialNormal = front.depth - rear.depth;
        const float axialNormal = rear.radius - front.radius;
        const float normalLength = std::hypot(radialNormal, axialNormal);

        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= 192; ++i) {
            const double angle = 2.0 * PI * i / 192;
            const float x = static_cast<float>(std::cos(angle));
            const float y = static_cast<float>(std::sin(angle));
            glNormal3f(x * radialNormal / normalLength, y * radialNormal / normalLength,
                axialNormal / normalLength);
            glVertex3f(front.radius * x, front.radius * y, front.depth);
            glVertex3f(rear.radius * x, rear.radius * y, rear.depth);
        }
        glEnd();
    }
}

void drawCase() {
    const RimPoint body[] = {
        { 0.0f, -0.245f }, { 0.87f, -0.245f }, { 0.909f, -0.231f },
        { 0.933f, -0.207f }, { CASE_RADIUS, -0.175f }, { CASE_RADIUS, 0.180f }
    };
    const RimPoint bezel[] = {
        { CASE_RADIUS, 0.166f }, { 0.952f, 0.190f }, { 0.952f, 0.221f },
        { 0.944f, 0.250f }, { 0.925f, 0.273f }, { 0.894f, 0.278f },
        { 0.877f, 0.267f }, { 0.869f, GLASS_DEPTH }
    };
    const RimPoint liner[] = {
        { 0.870f, GLASS_DEPTH }, { 0.858f, 0.240f }, { 0.843f, FACE_DEPTH - 0.001f }
    };
    // Profile order also determines the direction of the surface normal.
    const RimPoint backSeam[] = {
        { CASE_RADIUS, -0.180f }, { 0.943f, -0.176f },
        { 0.943f, -0.159f }, { CASE_RADIUS, -0.155f }
    };
    drawProfile(body, enamel);
    drawProfile(backSeam, brass);
    drawProfile(bezel, brass);
    drawProfile(liner, gasket);
}

// Molded rectangular part with chamfered corners and a beveled rear edge.
void drawRearPart(float halfWidth, float halfHeight, float base, float top,
    float bevel, const Finish& finish) {
    const float corner = 2.0f * bevel;
    const float outline[][2] = {
        { halfWidth - corner, halfHeight }, { -halfWidth + corner, halfHeight },
        { -halfWidth, halfHeight - corner }, { -halfWidth, -halfHeight + corner },
        { -halfWidth + corner, -halfHeight }, { halfWidth - corner, -halfHeight },
        { halfWidth, -halfHeight + corner }, { halfWidth, halfHeight - corner }
    };
    constexpr size_t count = sizeof(outline) / sizeof(outline[0]);
    const float shoulder = top + bevel;
    // Inset the top by moving each edge inward by the bevel width.
    const float cornerInset = (std::sqrt(2.0f) - 1.0f) * bevel;
    float inset[count][2];
    for (size_t i = 0; i < count; ++i) {
        const bool onVerticalEdge = i == 2 || i == 3 || i == 6 || i == 7;
        inset[i][0] = outline[i][0] - std::copysign(onVerticalEdge ? bevel : cornerInset, outline[i][0]);
        inset[i][1] = outline[i][1] - std::copysign(onVerticalEdge ? cornerInset : bevel, outline[i][1]);
    }

    setFinish(finish);
    glBegin(GL_QUADS);
    for (size_t i = 0; i < count; ++i) {
        const size_t j = (i + 1) % count;
        const float dx = outline[j][0] - outline[i][0];
        const float dy = outline[j][1] - outline[i][1];
        const float length = std::hypot(dx, dy);
        const float nx = dy / length;
        const float ny = -dx / length;
        glNormal3f(nx, ny, 0.0f);
        glVertex3f(outline[i][0], outline[i][1], base);
        glVertex3f(outline[i][0], outline[i][1], shoulder);
        glVertex3f(outline[j][0], outline[j][1], shoulder);
        glVertex3f(outline[j][0], outline[j][1], base);

        constexpr float diagonal = 0.70710678f;
        glNormal3f(nx * diagonal, ny * diagonal, -diagonal);
        glVertex3f(outline[i][0], outline[i][1], shoulder);
        glVertex3f(inset[i][0], inset[i][1], top);
        glVertex3f(inset[j][0], inset[j][1], top);
        glVertex3f(outline[j][0], outline[j][1], shoulder);
    }
    glEnd();

    glNormal3f(0.0f, 0.0f, -1.0f);
    glBegin(GL_POLYGON);
    for (size_t i = count; i > 0; --i) glVertex3f(inset[i - 1][0], inset[i - 1][1], top);
    glEnd();
}

void drawBack() {
    const RimPoint plate[] = {
        { 0.0f, -0.263f }, { 0.792f, -0.263f },
        { 0.825f, -0.260f }, { 0.853f, -0.244f }
    };
    drawProfile(plate, backPlate);

    // Raised movement housing with a seam around the snap-on setting cover.
    glPushMatrix();
    glTranslatef(0.0f, -0.07f, 0.0f);
    drawRearPart(0.325f, 0.295f, -0.262f, -0.343f, 0.027f, gasket);
    drawRearPart(0.294f, 0.264f, -0.342f, -0.384f, 0.022f, coverPlastic);

    // Three raised ribs give the release tab a grip.
    for (int i = 0; i < 3; ++i) {
        glPushMatrix();
        glTranslatef(0.0f, -0.155f - i * 0.025f, 0.0f);
        drawRearPart(0.084f, 0.008f, -0.383f, -0.392f, 0.0035f, backPlate);
        glPopMatrix();
    }
    glPopMatrix();

    // Screw heads sit near the edge of the rear panel.
    const RimPoint screw[] = {
        { 0.0f, -0.277f }, { 0.025f, -0.277f },
        { 0.032f, -0.271f }, { 0.032f, -0.262f }
    };
    for (int i = 0; i < 4; ++i) {
        glPushMatrix();
        glRotatef(45.0f + 90.0f * i, 0.0f, 0.0f, 1.0f);
        glTranslatef(0.0f, 0.72f, 0.0f);
        drawProfile(screw, steel);
        glDisable(GL_LIGHTING);
        glColor3f(0.10f, 0.11f, 0.10f);
        glTranslatef(0.0f, 0.0f, -0.278f);
        glRectf(-0.019f, -0.0035f, 0.019f, 0.0035f);
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }

    // The dark keyhole insert represents the wall-hanging slot.
    glPushMatrix();
    glTranslatef(0.0f, 0.59f, 0.0f);
    drawRearPart(0.078f, 0.106f, -0.262f, -0.284f, 0.014f, coverPlastic);
    glDisable(GL_LIGHTING);
    glTranslatef(0.0f, 0.0f, -0.285f);
    glColor3f(0.025f, 0.036f, 0.031f);
    drawCircle(0.0f, -0.025f, 0.038f, 32);
    glRectf(-0.017f, -0.026f, 0.017f, 0.060f);
    drawCircle(0.0f, 0.060f, 0.017f, 24);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

struct Hand {
    float angle;
    float length;
    float width;
    float tail;
    float depth;
    const Finish& finish;
};

void drawHand(const Hand& hand, bool shadow) {
    const float halfWidth = hand.width / 2;
    const float outline[][2] = {
        { -halfWidth * 0.7f, -hand.tail }, { halfWidth * 0.7f, -hand.tail },
        { halfWidth, 0.0f }, { halfWidth * 0.62f, hand.length * 0.94f },
        { 0.0f, hand.length }, { -halfWidth * 0.62f, hand.length * 0.94f },
        { -halfWidth, 0.0f }
    };
    constexpr size_t count = sizeof(outline) / sizeof(outline[0]);
    const float thickness = hand.width * 0.24f;
    glPushMatrix();
    if (shadow) {
        const float gap = hand.depth - FACE_DEPTH;
        glTranslatef(gap * 0.48f, -gap * 0.65f, FACE_DEPTH + 0.003f);
        glColor4f(0.12f, 0.10f, 0.07f, 0.20f);
    } else {
        glTranslatef(0.0f, 0.0f, hand.depth);
        setFinish(hand.finish);
    }
    glRotatef(-hand.angle, 0.0f, 0.0f, 1.0f);

    glNormal3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_POLYGON);
    for (const auto& point : outline) glVertex2fv(point);
    glEnd();

    if (!shadow) {
        glBegin(GL_QUADS);
        for (size_t i = 0; i < count; ++i) {
            const auto& a = outline[i];
            const auto& b = outline[(i + 1) % count];
            const float dx = b[0] - a[0];
            const float dy = b[1] - a[1];
            const float length = std::hypot(dx, dy);
            glNormal3f(dy / length, -dx / length, 0.0f);
            glVertex3f(a[0], a[1], -thickness);
            glVertex3f(b[0], b[1], -thickness);
            glVertex2fv(b);
            glVertex2fv(a);
        }
        glEnd();
    }
    glPopMatrix();
}

// Logo is multiplied onto the face so its white background disappears
void drawLogo() {
    if (!logoLoaded) {
        return;
    }

    const float u0 = CROP_X0 / SRC_W;
    const float u1 = CROP_X1 / SRC_W;
    const float v0 = CROP_Y0 / SRC_H;
    const float v1 = CROP_Y1 / SRC_H;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, logoTex);

    // Multiply the image with the clock-face color.
    glBlendFunc(GL_DST_COLOR, GL_ZERO);
    glColor3f(1.0f, 1.0f, 1.0f);

    // Draw the image as a circle so it cannot extend outside the clock face.
    glBegin(GL_TRIANGLE_FAN);

    // Center of the clock face
    glTexCoord2f((u0 + u1) / 2.0f, (v0 + v1) / 2.0f);
    glVertex2f(0.0f, 0.0f);

    for (int i = 0; i <= 120; ++i) {
        const double angle = 2.0 * PI * i / 120.0;
        const float x = static_cast<float>(cos(angle));
        const float y = static_cast<float>(sin(angle));

        // Map the square crop to the circle.
        const float u = (u0 + u1) / 2.0f
            + (u1 - u0) * x / 2.0f;

        // Flip the vertical texture coordinate because stb_image
        // loads the image from the top-left.
        const float v = (v0 + v1) / 2.0f
            - (v1 - v0) * y / 2.0f;

        glTexCoord2f(u, v);
        glVertex2f(CLOCK_RADIUS * x, CLOCK_RADIUS * y);
    }

    glEnd();

    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void drawFace() {
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, FACE_DEPTH);
    setFinish(dial);
    glNormal3f(0.0f, 0.0f, 1.0f);
    drawCircle(0.0f, 0.0f, CLOCK_RADIUS, 192);

    glDisable(GL_LIGHTING);
    glTranslatef(0.0f, 0.0f, 0.001f);
    drawLogo();

    glTranslatef(0.0f, 0.0f, 0.001f);
    glColor3fv(GREEN);
    for (int i = 0; i < 60; ++i) {
        const bool major = i % 5 == 0;
        const float halfWidth = major ? 0.005f : 0.002f;
        glPushMatrix();
        glRotatef(-i * 6.0f, 0.0f, 0.0f, 1.0f);
        glRectf(-halfWidth, major ? 0.744f : 0.783f, halfWidth, 0.818f);
        glPopMatrix();
    }

    for (int n = 1; n <= 12; ++n) {
        const double angle = n * PI / 6.0;
        char number[4];
        snprintf(number, sizeof(number), "%d", n);
        drawText(number, 0.66f * static_cast<float>(std::sin(angle)),
            0.66f * static_cast<float>(std::cos(angle)), 0.0125f);
    }
    glPopMatrix();
}

void drawGlass() {
    // Keep the depth buffer available for the bezel to occlude the glass at oblique angles.
    glDepthMask(GL_FALSE);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, GLASS_DEPTH);
    glColor4f(0.65f, 0.79f, 0.74f, 0.06f);
    drawCircle(0.0f, 0.0f, 0.869f, 192);

    // A broad, soft reflection across the upper glass shifts as the case rotates.
    const float yawRadians = yaw * static_cast<float>(PI / 180.0);
    glRotatef(-19.0f - 5.0f * std::sin(yawRadians), 0.0f, 0.0f, 1.0f);
    constexpr float radius = 0.866f;
    const float shift = pitch * 0.0015f;
    const float heights[] = { 0.30f, 0.42f, 0.54f, 0.58f };
    const float opacities[] = { 0.0f, 0.12f, 0.18f, 0.0f };
    glBegin(GL_QUAD_STRIP);
    for (int row = 0; row < 4; ++row) {
        const float y = heights[row] + shift;
        const float edge = std::sqrt(radius * radius - y * y);
        glColor4f(1.0f, 0.99f, 0.95f, opacities[row]);
        glVertex2f(-edge, y);
        glVertex2f(edge, y);
    }
    glEnd();
    glPopMatrix();
    glDepthMask(GL_TRUE);
}

void drawBackdrop() {
    glBegin(GL_QUADS);
    glColor3f(0.08f, 0.12f, 0.11f);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f(1.0f, -1.0f);
    glColor3f(0.27f, 0.34f, 0.30f);
    glVertex2f(1.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();
}



void render() {
    std::time_t t = std::time(nullptr) + timeOffset;

    std::tm localTime{};
    if (localtime_s(&localTime, &t) != 0) {
        return; // Time conversion failed
    }

    std::tm* lt = &localTime;

    double sec = lt->tm_sec;               
    double min = lt->tm_min + sec / 60.0;
    double hour = (lt->tm_hour % 12) + min / 60.0;

    glClearColor(0.08f, 0.12f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const int panelWidth = framebufferWidth / 6;
    glViewport(0, 0, panelWidth, framebufferHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, panelWidth, framebufferHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glColor3f(0.10f, 0.16f, 0.13f);
    glRectf(0.0f, 0.0f, static_cast<float>(panelWidth), static_cast<float>(framebufferHeight));
    glColor3f(0.96f, 0.92f, 0.81f);
    const float unit = framebufferHeight / 600.0f;
    drawUiText("Set time", panelWidth / 2.0f, 52.0f * unit, -14.0f * unit);
    for (int i = 0; i < 6; ++i) {
        const float top = (78.0f + i * 38.0f) * unit;
        glColor3f(0.18f, 0.26f, 0.21f);
        glRectf(12.0f * unit, top, panelWidth - 12.0f * unit, top + 28.0f * unit);
        glColor3f(0.96f, 0.92f, 0.81f);
        drawUiText(timeButtons[i].label, panelWidth / 2.0f, top + 14.0f * unit, -12.0f * unit);
    }
    const int availableWidth = framebufferWidth - panelWidth;
    const int side = std::min(availableWidth, framebufferHeight);
    glViewport(panelWidth + (availableWidth - side) / 2,
        (framebufferHeight - side) / 2, side, side);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    drawBackdrop();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glFrustum(-0.35, 0.35, -0.35, 0.35, 1.0, 12.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    const GLfloat keyPosition[] = { -0.6f, 0.8f, 1.0f, 0.0f };
    const GLfloat fillPosition[] = { 0.8f, -0.2f, 0.4f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, keyPosition);
    glLightfv(GL_LIGHT1, GL_POSITION, fillPosition);
    glTranslatef(0.0f, 0.17f, -3.55f);
    glRotatef(pitch, 1.0f, 0.0f, 0.0f);
    glRotatef(yaw, 0.0f, 1.0f, 0.0f);

    drawCase();
    drawBack();
    drawFace();

    const Hand hands[] = {
        { static_cast<float>(hour * 30.0), 0.43f, 0.066f, 0.070f, 0.160f, enamel },
        { static_cast<float>(min * 6.0), 0.61f, 0.041f, 0.085f, 0.184f, enamel },
        { static_cast<float>(sec * 6.0), 0.74f, 0.014f, 0.145f, 0.208f, brass }
    };
    glDepthMask(GL_FALSE);
    for (const Hand& hand : hands) drawHand(hand, true);
    glDepthMask(GL_TRUE);

    glEnable(GL_LIGHTING);
    for (const Hand& hand : hands) drawHand(hand, false);
    const RimPoint hub[] = {
        { 0.047f, 0.151f }, { 0.047f, 0.213f }, { 0.038f, 0.231f }, { 0.0f, 0.231f }
    };
    const RimPoint pin[] = {
        { 0.015f, 0.231f }, { 0.014f, 0.238f }, { 0.0f, 0.238f }
    };
    drawProfile(hub, brass);
    drawProfile(pin, steel);

    glDisable(GL_LIGHTING);
    drawGlass();
    glDisable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // Digital time
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
        lt->tm_hour, lt->tm_min, lt->tm_sec);
    glColor3f(0.96f, 0.92f, 0.81f);
    drawText(timeBuf, 0.0f, -0.76f, 0.014f);

    // Date
    char dateBuf[64];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %d %s %d",
        DAYS[lt->tm_wday], lt->tm_mday, MONTHS[lt->tm_mon], 1900 + lt->tm_year);
    glColor3f(0.72f, 0.78f, 0.73f);
    drawText(dateBuf, 0.0f, -0.87f, 0.009f);
}


void framebufferSizeCallback(GLFWwindow*, int w, int h) {
    framebufferWidth = w;
    framebufferHeight = h;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        glfwGetCursorPos(window, &dragX, &dragY);
        dragging = false;
        if (action != GLFW_PRESS) return;
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        if (width <= 0 || height <= 0) return;
        const double panelWidth = width / 6.0;
        if (dragX < panelWidth) {
            const double unit = height / 600.0;
            for (int i = 0; i < 6; ++i) {
                const double top = (78.0 + i * 38.0) * unit;
                if (dragX >= 12.0 * unit && dragX <= panelWidth - 12.0 * unit &&
                    dragY >= top && dragY <= top + 28.0 * unit) {
                    timeOffset += timeButtons[i].seconds;
                    break;
                }
            }
            return;
        }
        dragging = true;
    }
}

void cursorCallback(GLFWwindow*, double x, double y) {
    if (!dragging) return;

    yaw = std::remainder(yaw + static_cast<float>(x - dragX) * 0.35f, 360.0f);
    pitch = std::clamp(pitch + static_cast<float>(y - dragY) * 0.35f, -45.0f, 45.0f);
    dragX = x;
    dragY = y;
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_B) {
        yaw = std::remainder(yaw + 180.0f, 360.0f);
    } else if (key == GLFW_KEY_R) {
        yaw = DEFAULT_YAW;
        pitch = DEFAULT_VIEW_PITCH;
    }
}



int main(int argc, char* argv[]) {
    const bool smokeTest = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
    if (!glfwInit()) {
        printf("Failed to initialise GLFW\n");
        return EXIT_FAILURE;
    }

    if (smokeTest) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_SAMPLES, 4);                // anti-aliasing

    GLFWwindow* window = glfwCreateWindow(800, WINDOW_SIZE,
        "Analog Clock Simulation - Palm University College", nullptr, nullptr);
    if (!window) {
        printf("Failed to create window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    loadUiFont();
    glfwSwapInterval(1);                            // sync to screen refresh

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorCallback);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const GLfloat ambient[] = { 0.38f, 0.38f, 0.38f, 1.0f };
    const GLfloat key[] = { 0.75f, 0.70f, 0.63f, 1.0f };
    const GLfloat fill[] = { 0.25f, 0.34f, 0.34f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, key);
    glLightfv(GL_LIGHT0, GL_SPECULAR, key);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fill);
    glLightfv(GL_LIGHT1, GL_SPECULAR, fill);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    loadLogo();

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    framebufferSizeCallback(window, fbW, fbH);

    int frames = 0;
    while (!glfwWindowShouldClose(window)) {
        if (smokeTest) yaw = DEFAULT_YAW + frames * 90.0f;
        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
        if (smokeTest && ++frames >= 3) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    const bool passed = logoLoaded && glGetError() == GL_NO_ERROR;
    if (smokeTest) printf("Smoke test: %s (OpenGL rendering and logo loading)\n", passed ? "PASS" : "FAIL");
    if (logoLoaded) glDeleteTextures(1, &logoTex);
    if (uiFont) glDeleteLists(uiFont, 128);
    glfwDestroyWindow(window);
    glfwTerminate();
    return smokeTest && !passed ? EXIT_FAILURE : EXIT_SUCCESS;
}
