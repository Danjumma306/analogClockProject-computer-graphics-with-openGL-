// clock_glfw.cpp - Analog Clock Simulation (C++ / OpenGL / GLFW)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"          // only used to load logo.jpg

#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iostream>

const double PI = 3.14159265358979323846;
const int    WINDOW_SIZE = 600;
const float  CLOCK_RADIUS = 0.85f;
const float  CLOCK_SCALE = 0.87f;   // shrinks the clock to leave room for text below
const float  CLOCK_Y = 0.17f;   // moves the clock up

// Logo file and crop region (pixels) of the original 596x335 image
const char* LOGO_FILE = "logo.jpeg";

// Logo file and crop region for the 720 x 732 image
const float SRC_W = 720.0f;
const float SRC_H = 732.0f;

// Centered square crop: 720 x 720
const float CROP_X0 = 0.0f;
const float CROP_X1 = 720.0f;
const float CROP_Y0 = 6.0f;
const float CROP_Y1 = 700.0f;
const float LOGO_W = 0.50f;
const float LOGO_H = LOGO_W * (CROP_Y1 - CROP_Y0) / (CROP_X1 - CROP_X0);
const float LOGO_CY = 0.26f;

GLuint logoTex = 0;
bool   logoLoaded = false;

const char* DAYS[] = { "SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY" };
const char* MONTHS[] = { "JANUARY","FEBRUARY","MARCH","APRIL","MAY","JUNE",
                        "JULY","AUGUST","SEPTEMBER","OCTOBER","NOVEMBER","DECEMBER" };

// Palm University College colours
const float GREEN[3] = { 0.043f, 0.365f, 0.231f };
const float GOLD[3] = { 0.784f, 0.573f, 0.102f };

// ---------- Built-in 5x7 pixel font (no external library) ----------
// Each glyph is 5 columns; bit 0 of each byte is the TOP row.

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
    { 0x7F,0x49,0x49,0x49,0x41 },{ 0x7F,0x09,0x09,0x09,0x01 }, // E F
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
                    float y0 = y + (6 - row) * px;     // row 0 is the top
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

// ---------- Logo texture ----------

void loadLogo() {
    int w, h, n;
    unsigned char* data = stbi_load(LOGO_FILE, &w, &h, &n, 3);
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

// ---------- Drawing helpers ----------

void drawCircle(float cx, float cy, float r, int segments = 120) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        double a = 2.0 * PI * i / segments;
        glVertex2f(cx + r * (float)cos(a), cy + r * (float)sin(a));
    }
    glEnd();
}

void drawRing(float r, float lineWidth, int segments = 120) {
    glLineWidth(lineWidth);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; i++) {
        double a = 2.0 * PI * i / segments;
        glVertex2f(r * (float)cos(a), r * (float)sin(a));
    }
    glEnd();
}

// Hand as a thin quad. angleDeg is measured clockwise from 12 o'clock.
void drawHand(float angleDeg, float length, float width, float tail, const float* c) {
    glPushMatrix();
    glRotatef(-angleDeg, 0.0f, 0.0f, 1.0f);
    glColor3f(c[0], c[1], c[2]);
    glBegin(GL_QUADS);
    glVertex2f(-width / 2, -tail);
    glVertex2f(width / 2, -tail);
    glVertex2f(width / 2, length);
    glVertex2f(-width / 2, length);
    glEnd();
    glPopMatrix();
}

// Logo multiplied onto the face so its white background disappears
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
    glColor3f(0.984f, 0.980f, 0.957f);
    drawCircle(0.0f, 0.0f, CLOCK_RADIUS);

    drawLogo();

    glColor3f(GREEN[0], GREEN[1], GREEN[2]);
    drawRing(CLOCK_RADIUS, 6.0f);

    // 60 tick marks, thicker every 5
    for (int i = 0; i < 60; i++) {
        double a = i * PI / 30.0;
        bool major = (i % 5 == 0);
        float r1 = CLOCK_RADIUS - 0.03f;
        float r2 = CLOCK_RADIUS - (major ? 0.11f : 0.06f);
        glLineWidth(major ? 3.0f : 1.0f);
        glColor3f(GREEN[0], GREEN[1], GREEN[2]);
        glBegin(GL_LINES);
        glVertex2f(r1 * (float)sin(a), r1 * (float)cos(a));
        glVertex2f(r2 * (float)sin(a), r2 * (float)cos(a));
        glEnd();
    }

    // Numerals 1-12 using the built-in pixel font
    glColor3f(GREEN[0], GREEN[1], GREEN[2]);
    for (int n = 1; n <= 12; n++) {
        double a = n * PI / 6.0;
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", n);
        drawText(buf, 0.66f * (float)sin(a), 0.66f * (float)cos(a), 0.0125f);
    }
}

// ---------- Rendering one frame ----------

void render() {
    std::time_t t = std::time(nullptr);

    std::tm localTime{};
    if (localtime_s(&localTime, &t) != 0) {
        return; // Time conversion failed
    }

    std::tm* lt = &localTime;

    double sec = lt->tm_sec;                    // whole seconds, so the hand ticks
    double min = lt->tm_min + sec / 60.0;
    double hour = (lt->tm_hour % 12) + min / 60.0;

    glClearColor(0.93f, 0.93f, 0.91f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Clock (moved up and scaled down to leave room for the readout)
    glPushMatrix();
    glTranslatef(0.0f, CLOCK_Y, 0.0f);
    glScalef(CLOCK_SCALE, CLOCK_SCALE, 1.0f);

    drawFace();
    drawHand((float)(hour * 30.0), 0.40f, 0.040f, 0.06f, GREEN);
    drawHand((float)(min * 6.0), 0.58f, 0.025f, 0.07f, GREEN);
    drawHand((float)(sec * 6.0), 0.70f, 0.010f, 0.14f, GOLD);

    glColor3f(GOLD[0], GOLD[1], GOLD[2]);
    drawCircle(0.0f, 0.0f, 0.035f);
    glColor3f(0.984f, 0.980f, 0.957f);
    drawCircle(0.0f, 0.0f, 0.014f);
    glPopMatrix();

    // Digital time
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
        lt->tm_hour, lt->tm_min, lt->tm_sec);
    glColor3f(GREEN[0], GREEN[1], GREEN[2]);
    drawText(timeBuf, 0.0f, -0.70f, 0.016f);

    // Date
    char dateBuf[64];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %d %s %d",
        DAYS[lt->tm_wday], lt->tm_mday, MONTHS[lt->tm_mon], 1900 + lt->tm_year);
    glColor3f(0.35f, 0.35f, 0.38f);
    drawText(dateBuf, 0.0f, -0.87f, 0.009f);
}

// ---------- GLFW callbacks ----------

void framebufferSizeCallback(GLFWwindow*, int w, int h) {
    int side = w < h ? w : h;                       // keep the clock circular
    glViewport((w - side) / 2, (h - side) / 2, side, side);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action == GLFW_PRESS && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q))
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// ---------- Main ----------

int main() {
    if (!glfwInit()) {
        printf("Failed to initialise GLFW\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);                // anti-aliasing

    GLFWwindow* window = glfwCreateWindow(WINDOW_SIZE, WINDOW_SIZE,
        "Analog Clock Simulation - Palm University College", nullptr, nullptr);
    if (!window) {
        printf("Failed to create window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);                            // sync to screen refresh

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    loadLogo();                                     // needs the GL context to exist

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    framebufferSizeCallback(window, fbW, fbH);

    while (!glfwWindowShouldClose(window)) {
        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}