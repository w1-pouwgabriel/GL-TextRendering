#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "graphics/shader.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

//Load std
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Function declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void RenderText(Shader& shader, std::string text, float x, float y, float scale, glm::vec3 color);

unsigned int VAO, VBO;

struct Text
{
    int width;
    int height;
    unsigned char* pixels;
};


int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Text rendering", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // OpenGL state
	// ------------
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // build and compile shaders
    // -------------------------
    Shader ourShader("src/resources/shaders/TextVertexShader.vert", "src/resources/shaders/TextPixelShader.frag");
    // shader setup
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH), 0.0f, static_cast<float>(SCR_HEIGHT));
    ourShader.use();
    glUniformMatrix4fv(glGetUniformLocation(ourShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // load font and create a bitmap
    // -----------------------------
    long size;
    unsigned char* fontBuffer;
    Text text{0, 0};

    FILE* fontFile = fopen("src/resources/fonts/verdana.ttf", "rb");
    fseek(fontFile, 0, SEEK_END);
    size = ftell(fontFile); /* how long is the file ? */
    fseek(fontFile, 0, SEEK_SET); /* reset */

    fontBuffer = static_cast<unsigned char*>(malloc(size));

    fread(fontBuffer, size, 1, fontFile);
    fclose(fontFile);

    /* prepare font */
    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontBuffer, 0))
    {
        printf("failed\n");
    }

    float scale = stbtt_ScaleForPixelHeight(&fontInfo, 48 * 2);

    int ascent, descent;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, 0);
    int baseline = (int)(ascent * scale);

    const char* content = " text ";

    //Create a empty space we can draw our text to
    for (int i = 0; i < strlen(content); i++) 
    {
        int advance, leftSideBearing;
        stbtt_GetCodepointHMetrics(&fontInfo, content[i], &advance, &leftSideBearing);

        text.width += advance * scale;
    }

    text.height = (ascent + (descent * -1)) * scale;
    text.pixels = static_cast<unsigned char*>(calloc(text.width * text.height, sizeof(unsigned char)));

    int x = 0;
    int i;
    for (i = 0; i < strlen(content); ++i)
    {
        /* how wide is this character */
        int ax;
        int lsb;
        stbtt_GetCodepointHMetrics(&fontInfo, content[i], &ax, &lsb);
        /* (Note that each Codepoint call has an alternative Glyph version which caches the work required to lookup the character word[i].) */

        /* get bounding box for character (may be offset to account for chars that dip above or below the line) */
        int c_x1, c_y1, c_x2, c_y2;
        stbtt_GetCodepointBitmapBox(&fontInfo, content[i], scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);

        /* compute y (different characters have different heights) */
        int y = ascent + c_y1;

        /* render character (stride and offset is important here) */
        int byteOffset = x + roundf(lsb * scale) + (y * text.width);
        stbtt_MakeCodepointBitmap(&fontInfo, text.pixels + byteOffset, c_x2 - c_x1, c_y2 - c_y1, text.width, scale, scale, content[i]);

        /* advance x */
        x += roundf(ax * scale);

        /* add kerning */
        int kern;
        kern = stbtt_GetCodepointKernAdvance(&fontInfo, content[i], content[i + 1]);
        x += roundf(kern * scale);
    }

    // This step works fine, this means that the data in pixels is good
    stbi_write_png("src/resources/textures/image.png", text.width, text.height, 1, text.pixels, 0);

    // configure VAO/VBO for texture quads
    // -----------------------------------
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // render loop
    // -----------
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        // -------
        processInput(window);

        // render
        // ------
        glClearColor(0.8f, 0.3f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
};

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}
