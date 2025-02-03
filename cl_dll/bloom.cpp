
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

extern engine_studio_api_t IEngineStudio;

#define GL_TEXTURE_RECTANGLE_NV 0x84F5

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

    glEnable(GL_TEXTURE_RECTANGLE_NV);
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
    glDisable(GL_TEXTURE_RECTANGLE_NV);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    R_RestoreGLStates();
    
}