
#include "gl/glew.h"
#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"

#include <windows.h>
#include <gl/gl.h>

#include "r_studioint.h"

#include "bsprenderer.h"

#include "opengl_utils/GL_TextureHandler.h"
#include "opengl_utils/GL_StateHandler.h"

extern engine_studio_api_t IEngineStudio;

cvar_t* glow_blur_steps = NULL;
cvar_t* glow_darken_steps = NULL;
cvar_t* glow_strength = NULL;
cvar_t* r_bloom_effect = NULL;
cvar_t* glow_multiplier = NULL;
float glow_mult = 0.0f;

bool CBloom::Init(void)
{
    // create a load of blank pixels to create textures with
    unsigned char* pBlankTex = new unsigned char[ScreenWidth * ScreenHeight * 3];
    memset(pBlankTex, 0, ScreenWidth * ScreenHeight * 3);

    GL_TextureHandler::gl_texturecreationinfo_t texinfo;

    // Create the SCREEN-HOLDING TEXTURE
    texinfo.SetInfo(std::string(), GL_TextureHandler::_Rectangle, GL_RGB8, ScreenWidth, ScreenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE);
    g_pScreenTex = new GL_TextureHandler(&texinfo);
    g_pScreenTex->UploadPixelData(pBlankTex);

    // Create the BLURRED TEXTURE
    texinfo.SetInfo(std::string(), GL_TextureHandler::_Rectangle, GL_RGB8, ScreenWidth / 2, ScreenHeight / 2, 0, GL_RGB, GL_UNSIGNED_BYTE);
    g_pGlowTex = new GL_TextureHandler(&texinfo);
    g_pGlowTex->UploadPixelData(pBlankTex);


    // free the memory
    delete[] pBlankTex;

    glow_blur_steps = CVAR_CREATE("glow_blur_steps", "8", FCVAR_ARCHIVE);
    glow_darken_steps = CVAR_CREATE("glow_darken_steps", "1", FCVAR_ARCHIVE);
    glow_strength = CVAR_CREATE("glow_strength", "1", FCVAR_ARCHIVE);

    r_bloom_effect = CVAR_CREATE("r_bloom", "1", FCVAR_ARCHIVE);
    glow_multiplier = CVAR_CREATE("glow_multiplier", "1", FCVAR_ARCHIVE);

    return true;
}

void CBloom::DrawQuad(int width, int height, int ofsX, int ofsY)
{
    glTexCoord2f(ofsX, ofsY);
    glVertex3f(0, 1, -1);
    glTexCoord2f(ofsX, height + ofsY);
    glVertex3f(0, 0, -1);
    glTexCoord2f(width + ofsX, height + ofsY);
    glVertex3f(1, 0, -1);
    glTexCoord2f(width + ofsX, ofsY);
    glVertex3f(1, 1, -1);
}

void CBloom::Draw(void)
{
    // check to see if (a) we can render it, and (b) we're meant to render it

    if ((int)glow_blur_steps->value == 0 || (int)glow_strength->value == 0)
        return;

    if (!(int)r_bloom_effect->value)
        return;

    // enable some OpenGL stuff
    glEnable(GL_TEXTURE_RECTANGLE);
    glColor3f(1, 1, 1);
    g_GlobalGLState.SetDepthTest(false);

    // STEP 1: Grab the screen and put it into a texture

    glBindTexture(GL_TEXTURE_RECTANGLE, g_pScreenTex->GetTextureID());
    glCopyTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGB, 0, 0, ScreenWidth, ScreenHeight, 0);

    // STEP 2: Set up an orthogonal projection

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, 0.1, 100);

    // STEP 3: Render the current scene to a new, lower-res texture, darkening non-bright areas of the scene
    // by multiplying it with itself a few times.

    glViewport(0, 0, ScreenWidth / 2, ScreenHeight / 2);

    glBindTexture(GL_TEXTURE_RECTANGLE, g_pScreenTex->GetTextureID());

    g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_ZERO);

    g_GlobalGLState.SetBlend(false);

    glBegin(GL_QUADS);
        DrawQuad(ScreenWidth, ScreenHeight);
    glEnd();

    g_GlobalGLState.SetBlend(true);

    // Dynamic Bloom - some janky ass math here
    auto mult = gHUD.m_fLight - 100.0f;
    mult = 100 - mult;
    mult = mult / 400.0f;
    glow_mult = lerp(glow_mult, 0.75 + (mult), gHUD.m_flTimeDelta * 3.0f);

    glColor4f(glow_mult, glow_mult, glow_mult, 1.0f);

    glBegin(GL_QUADS);
    for (int i = 0; i < (int)glow_darken_steps->value; i++)
        DrawQuad(ScreenWidth, ScreenHeight);
    glEnd();

    glBindTexture(GL_TEXTURE_RECTANGLE, g_pGlowTex->GetTextureID());
    glCopyTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGB, 0, 0, ScreenWidth / 2, ScreenHeight / 2, 0);

    // STEP 4: Blur the now darkened scene in the horizontal direction.
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    float blurAlpha = 1 / (glow_blur_steps->value * 2 + 1);

    glColor4f(1, 1, 1, blurAlpha);

    g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ZERO);

    glBegin(GL_QUADS);
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2);
    glEnd();

    g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glBegin(GL_QUADS);
    for (int i = 1; i <= (int)glow_blur_steps->value; i++) {
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2, -i, 0);
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2, i, 0);
    }
    glEnd();

    glCopyTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGB, 0, 0, ScreenWidth / 2, ScreenHeight / 2, 0);

    // STEP 5: Blur the horizontally blurred image in the vertical direction.

    g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ZERO);

    glBegin(GL_QUADS);
     DrawQuad(ScreenWidth / 2, ScreenHeight / 2);
    glEnd();

    g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glBegin(GL_QUADS);
    for (int i = 1; i <= (int)glow_blur_steps->value; i++) {
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2, 0, -i);
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2, 0, i);
    }
    glEnd();

    glCopyTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGB, 0, 0, ScreenWidth / 2, ScreenHeight / 2, 0);

    // STEP 6: Combine the blur with the original image.

    glViewport(0, 0, ScreenWidth, ScreenHeight);

    g_GlobalGLState.SetBlend(false);

    glBegin(GL_QUADS);
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2);
    glEnd();

    g_GlobalGLState.SetBlend(true);
    g_GlobalGLState.SetBlendFunc(GL_ONE, GL_ONE);
    glColor4f(glow_mult, glow_mult, glow_mult, 1.0f);
    glBegin(GL_QUADS);
    for (int i = 1; i < (int)glow_strength->value; i++) {
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2);
    }
    glEnd();

    glColor4f(1.0, 1.0, 1.0, 1.0f);

    float end_mult = glow_mult / 2.0f;
    end_mult = 0.5 + end_mult;

    glColor4f(end_mult, end_mult, end_mult, 1.0f);
    glBindTexture(GL_TEXTURE_RECTANGLE, g_pScreenTex->GetTextureID());
    glBegin(GL_QUADS);
    DrawQuad(ScreenWidth, ScreenHeight);
    glEnd();

    // STEP 7: Restore the original projection and modelview matrices and disable rectangular textures.

    glColor4f(1.0, 1.0, 1.0, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glDisable(GL_TEXTURE_RECTANGLE);
    g_GlobalGLState.SetDepthTest(true);
    g_GlobalGLState.SetBlend(false);
}

// GLSL BLOOM - UNUSED
/*

extern engine_studio_api_t IEngineStudio;

#define GL_TEXTURE_RECTANGLE 0x84F5

cvar_t* te_bloom_effect = NULL;
cvar_t* te_bloom_val = NULL;

cvar_t* te_fxaa = NULL;

float glow_mult = 0.0f;

extern ShaderUtil postProcessShader;

extern ShaderUtil FXAAShader;

bool CBloom::Init(void)
{
    // create a load of blank pixels to create textures with
    unsigned char* pBlankTex = new unsigned char[ScreenWidth * ScreenHeight * 3];
    memset(pBlankTex, 0, ScreenWidth * ScreenHeight * 3);

    // Create the SCREEN-HOLDING TEXTURE
    glGenTextures(1, &g_uiScreenTex);
    glBindTexture(GL_TEXTURE_2D, g_uiScreenTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, ScreenWidth, ScreenHeight, 0, GL_RGB8, GL_UNSIGNED_BYTE, pBlankTex);

    // free the memory
    delete[] pBlankTex;

    te_bloom_effect = CVAR_CREATE("te_bloom", "1", FCVAR_ARCHIVE);
    te_fxaa = CVAR_CREATE("te_fxaa", "1", FCVAR_ARCHIVE);

    te_bloom_val = CVAR_CREATE("te_bloom_val", "1", FCVAR_ARCHIVE);


    return true;
}

void CBloom::DrawQuad(int width, int height, int ofsX, int ofsY)
{
    glTexCoord2f(ofsX, ofsY);
    glVertex3f(0, 1, -1);
    glTexCoord2f(ofsX, height + ofsY);
    glVertex3f(0, 0, -1);
    glTexCoord2f(width + ofsX, height + ofsY);
    glVertex3f(1, 0, -1);
    glTexCoord2f(width + ofsX, ofsY);
    glVertex3f(1, 1, -1);
}

void CBloom::Draw(void)
{
    // check to see if (a) we can render it, and (b) we're meant to render it

    if (IEngineStudio.IsHardware() != 1)
        return;

    if ((int)te_bloom_effect->value == 0 && (int)te_fxaa->value == 0)
        return;


    R_SaveGLStates();

    // enable some OpenGL stuff
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    glEnable(GL_TEXTURE_RECTANGLE);
    glColor3f(1, 1, 1);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, 0.1, 100);

    if(te_fxaa->value > 0)
    {
        // fxaa pass

        glBindTexture(GL_TEXTURE_2D, g_uiScreenTex);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, ScreenWidth, ScreenHeight, 0);

        FXAAShader.Use();

        glUniform1i(glGetUniformLocation(FXAAShader.GetProgramID(), "iChannel0"), 0);
        glUniform1i(glGetUniformLocation(FXAAShader.GetProgramID(), "iWidth"), ScreenWidth);
        glUniform1i(glGetUniformLocation(FXAAShader.GetProgramID(), "iHeight"), ScreenHeight);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_uiScreenTex);

        glViewport(0, 0, ScreenWidth, ScreenHeight);
        glColor4f(1, 1, 1, 1);
        glBegin(GL_QUADS);
        gHUD.gBloomRenderer.DrawQuad(ScreenWidth, ScreenHeight);
        glEnd();
        FXAAShader.Unuse();

        // fxaa end
    }

    if(te_bloom_effect->value > 0)
    {
        // bloom pass

        glBindTexture(GL_TEXTURE_2D, g_uiScreenTex);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, ScreenWidth, ScreenHeight, 0);

        auto mult = gHUD.m_fLight - 100.0f;
        mult = 100 - mult;
        mult = mult / 100.0f;

        glow_mult = lerp(glow_mult, (mult * te_bloom_val->value), gHUD.m_flTimeDelta * 3.0f);

        postProcessShader.Use();

        glUniform1i(glGetUniformLocation(postProcessShader.GetProgramID(), "iChannel0"), 0);
        glUniform1f(glGetUniformLocation(postProcessShader.GetProgramID(), "mult"), glow_mult);
        glUniform1i(glGetUniformLocation(FXAAShader.GetProgramID(), "iWidth"), ScreenWidth);
        glUniform1i(glGetUniformLocation(FXAAShader.GetProgramID(), "iHeight"), ScreenHeight);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_uiScreenTex);

        glViewport(0, 0, ScreenWidth, ScreenHeight);
        glColor4f(1, 1, 1, 1);
        glBegin(GL_QUADS);
        gHUD.gBloomRenderer.DrawQuad(ScreenWidth, ScreenHeight);
        glEnd();
        postProcessShader.Unuse();

        // bloom end
    }

    // reset state
    glViewport(0, 0, ScreenWidth, ScreenHeight);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glDisable(GL_TEXTURE_RECTANGLE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    R_RestoreGLStates();
    
}

*/