#include "stdio.h"
#include "stdlib.h"
#include <string>
#include <vector>
#include <string.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "com_model.h"
#include "UserMessages.h"

// imgui
#include "PlatformHeaders.h"
#include "SDL2/SDL.h"
#include <gl/GL.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl2.h"
#include "backends/imgui_impl_sdl.h"


struct CClientPointMessage
{
	Vector origin;
	std::string message;
	bool developerOnly;
	float maxDistance;
};

char *V_strupr( char *start )
{
	unsigned char *str = (unsigned char *)start;
	while( *str )
	{
		if( (unsigned char)(*str - 'a') <= ('z' - 'a') )
			*str -= 'a' - 'A';
		else if( (unsigned char)*str >= 0x80 ) // non-ascii, fall back to CRT
			*str = toupper( *str );
		str++;
	}
	return start;
}

extern SDL_Window* mainWindow;
extern SDL_GLContext mainContext;
std::vector<CClientPointMessage> MessageList;

DECLARE_MESSAGE(m_PointMessage, PointMsg);

DECLARE_MESSAGE(m_PointMessage, Readable);

#include "stb_image.h"

void LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width = nullptr, int* out_height = nullptr)
{
	if (!filename)
		return;

	// Load from file
	std::string path = gEngfuncs.pfnGetGameDirectory() + std::string("/") + std::string(filename);
	int image_width = 0;
	int image_height = 0;
	unsigned char* image_data = stbi_load(path.c_str(), &image_width, &image_height, NULL, 4);
	if (image_data == NULL)
		return;

	// Create a OpenGL texture identifier
	GLuint image_texture;
	glGenTextures(1, &image_texture);
	glBindTexture(GL_TEXTURE_2D, image_texture);

	// Setup filtering parameters for display
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
	stbi_image_free(image_data);

	if (out_texture)
		*out_texture = image_texture;
	if (out_width)
		*out_width = image_width;
	if (out_height)
		*out_height = image_height;

	return;
}

GLuint readableTex = 0;
int readableWidth, readableHeight;

int CPointMessageRenderer::Init()
{
	m_iFlags |= HUD_ACTIVE;
	HOOK_MESSAGE(PointMsg)
	HOOK_MESSAGE(Readable);

	gHUD.AddHudElem(this);

	//mainWindow = SDL_GetWindowFromID(1);
	// mainContext = SDL_GL_CreateContext(mainWindow);

	// Setup Dear ImGui context
	//IMGUI_CHECKVERSION();
	//ImGui::CreateContext();
	//ImGuiIO& io = ImGui::GetIO();

	//io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	// Setup Dear ImGui style
	//ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	//ImGui_ImplOpenGL2_Init();
	//ImGui_ImplSDL2_InitForOpenGL(mainWindow, ImGui::GetCurrentContext());

	return true;
}

bool CPointMessageRenderer::MsgFunc_PointMsg(const char* pszName, int iSize, void* pbuf)
{
	CClientPointMessage local;

	BEGIN_READ(pbuf, iSize);
	local.origin.x = READ_COORD();
	local.origin.y = READ_COORD();
	local.origin.z = READ_COORD();
	local.message = std::string(READ_STRING());
	local.developerOnly = (bool)READ_BYTE();
	local.maxDistance = READ_FLOAT();

	// add to our list
	MessageList.push_back(local);

	return true;
}

bool CPointMessageRenderer::MsgFunc_Readable(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	readableOrg.x = READ_COORD();
	readableOrg.y = READ_COORD();
	readableOrg.z = READ_COORD();
	char* path = READ_STRING();
	readablePath = path;
	int oldwidth = readableWidth;
	int oldheight = readableHeight;
	LoadTextureFromFile(path, &readableTex, &readableWidth, &readableHeight);
	if ((m_bDrawReadable == true) && oldwidth == readableWidth && oldheight == readableHeight)
	{
		m_bDrawReadable = false;
		readableTex = 0;
		return true;
	}
	m_bDrawReadable = true;

	m_flUseKeyDelay = gEngfuncs.GetAbsoluteTime() + 1.0f;


	return true;
}

void CPointMessageRenderer::Reset()
{

}

int CPointMessageRenderer::VidInit()
{
	// reset list
	MessageList.clear();

	m_flUseKeyDelay = 0;
	m_bDrawReadable = false;

	return true;
}

int CPointMessageRenderer::Draw(float flTime)
{
	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplSDL2_NewFrame(mainWindow);
	ImGui::NewFrame();

	// draw stuffs here 
	for (int i = 0; i < MessageList.size(); i++)
	{
		if (!MessageList[i].message.empty())
		{
			// specify length
			int len = MessageList[i].message.length();
			len *= 10;
			ImGui::SetNextWindowSize(ImVec2(len, 25));

			// World2Screen conversion
			float screen[2];
			gEngfuncs.pTriAPI->WorldToScreen(MessageList[i].origin, screen);
			int x = XPROJECT(screen[0]);
			int y = YPROJECT(screen[1]);
			ImGui::SetNextWindowPos(ImVec2(x - (len / 2), y - (25 / 2)));

			// check player rotation
			Vector vecDir = MessageList[i].origin - gEngfuncs.GetLocalPlayer()->curstate.origin;
			vecDir.z = 0;
			Vector forward, angle;
			angle = gEngfuncs.GetLocalPlayer()->curstate.angles;
			angle.x = 0;
			AngleVectors(angle, &forward, null, null);

			// check if the coords are simply out of the screen coord
			if (((((x - (len / 2)) > 0) && ((x - (len / 2)) < ScreenWidth)) && ((y > 0) && (y < ScreenHeight)))
				&& DotProduct(forward, vecDir) > 90										// or player is facing different way,
				&& vecDir.Length() < MessageList[i].maxDistance							// or way too far (out of radius)
				&& !(CVAR_GET_FLOAT("developer") == 0 && MessageList[i].developerOnly))	// or if its developer only
			{
				ImGui::Begin(std::to_string(i).c_str(), null, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
				ImGui::Text(MessageList[i].message.c_str());
				ImGui::End();
			}
		}
	}

	// Draw readable window
	if (m_bDrawReadable)
	{
		float scaleX = (ScreenWidth / 1366.f) * 0.63f;
		float scaleY = (ScreenHeight / 768.f) * 0.63f;

		float scale = fmin(scaleX, scaleY);

		float imageW = readableWidth * scale;
		float imageH = readableHeight * scale;
		Vector2D window(imageW, imageH);

		ImGui::SetNextWindowPos(ImVec2((ScreenWidth / 2) - (window.x / 2), (ScreenHeight / 2) - (window.y / 2)));
		ImGui::SetNextWindowSize(ImVec2(window.x * 1.6, window.y * 1.6 )); //make sure its not cropped
		ImGui::Begin("test", null, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
		ImGui::Image((void*)(intptr_t)readableTex, ImVec2(window.x, window.y));
	//	std::string text = "Press E or walk away to close.";
		char text[128];
		char key_c[128];
		const char *key = gEngfuncs.Key_LookupBinding( "use" );
		if( !key || key[0] == '\0' )
			key = "< not bound >";

		std::snprintf( key_c, sizeof( key_c ), "%s", key );
		V_strupr( key_c );

		std::snprintf( text, sizeof( text ), "Press %s or walk away to close.", key_c );
		auto windowWidth = ImGui::GetWindowSize().x;
		auto textWidth = ImGui::CalcTextSize(text).x;
		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
		ImGui::Text(text);

		ImGui::End();

		// if the player is walking away, stop drawing
		if (Vector(readableOrg - gEngfuncs.GetLocalPlayer()->curstate.origin).Length2D() > 150)
		{
			m_bDrawReadable = false;
		}

	}

	ImGui::Render();
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

	return true;
}