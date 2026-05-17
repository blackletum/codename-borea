/*
 *
 *    This program is free software; you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published by the
 *    Free Software Foundation; either version 2 of the License, or (at
 *    your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software Foundation,
 *    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    In addition, as a special exception, the author gives permission to
 *    link the code of this program with the Half-Life Game Engine ("HL
 *    Engine") and Modified Game Libraries ("MODs") developed by Valve,
 *    L.L.C ("Valve").  You must obey the GNU General Public License in all
 *    respects for all of the code used other than the HL Engine and MODs
 *    from Valve.  If you modify this file, you may extend this exception
 *    to your version of the file, but you are not obligated to do so.  If
 *    you do not wish to do so, delete this exception statement from your
 *    version.
 *
 */

#include "hud.h"
#include "cdll_int.h"
#include "cl_util.h"
#include "Sequence.h"
#include "filesystem_utils.h"

#define DEMO_MESSAGE "__DEMOMESSAGE__"
#define NETWORK_MESSAGE1 "__NETMESSAGE__1"
#define NETWORK_MESSAGE2 "__NETMESSAGE__2"
#define NETWORK_MESSAGE3 "__NETMESSAGE__3"
#define NETWORK_MESSAGE4 "__NETMESSAGE__4"
#define NETWORK_MESSAGE5 "__NETMESSAGE__5"
#define NETWORK_MESSAGE6 "__NETMESSAGE__6"

#define MAX_NETMESSAGE 6

#define MSGFILE_NAME 0
#define MSGFILE_TEXT 1
#define MAX_MESSAGES 600 // I don't know if this table will balloon like every other feature in Half-Life
						 // But, for now, I've set this to a reasonable value

struct characterset_t
{
	char set[256];
};

// This is essentially a strpbrk() using a precalculated lookup table
//-----------------------------------------------------------------------------
// Purpose: builds a simple lookup table of a group of important characters
// Input  : *pSetBuffer - pointer to the buffer for the group
//			*pSetString - list of characters to flag
//-----------------------------------------------------------------------------
static void CharacterSetBuild(characterset_t* pSetBuffer, const char* pszSetString)
{
	int i = 0;

	// Test our pointers
	if (!pSetBuffer || !pszSetString)
		return;

	memset(pSetBuffer->set, 0, sizeof(pSetBuffer->set));

	while (pszSetString[i])
	{
		pSetBuffer->set[(unsigned)pszSetString[i]] = 1;
		i++;
	}

}

// Copied from sound.cpp in the DLL
static char* memfgets(unsigned char* pMemFile, int fileSize, int* pFilePos, char* pBuffer, int bufferSize)
{
	int filePos = *pFilePos;
	int i, last, stop;

	// Bullet-proofing
	if (!pMemFile || !pBuffer)
		return NULL;

	if (filePos >= fileSize)
		return NULL;

	i = filePos;
	last = fileSize;

	// fgets always NULL terminates, so only read bufferSize-1 characters
	if (last - filePos > (bufferSize - 1))
		last = filePos + (bufferSize - 1);

	stop = 0;

	// Stop at the next newline (inclusive) or end of buffer
	while (i < last && !stop)
	{
		if (pMemFile[i] == '\n')
			stop = 1;
		i++;
	}


	// If we actually advanced the pointer, copy it over
	if (i != filePos)
	{
		// We read in size bytes
		int size = i - filePos;
		// copy it out
		memcpy(pBuffer, pMemFile + filePos, sizeof(unsigned char) * size);

		// If the buffer isn't full, terminate (this is always true)
		if (size < bufferSize)
			pBuffer[size] = 0;

		// Update file pointer
		*pFilePos = i;
		return pBuffer;
	}

	// No data read, bail
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetBuffer - pre-build group buffer
//			character - character to lookup
// Output : int - 1 if the character was in the set
//-----------------------------------------------------------------------------
#define IN_CHARACTERSET( SetBuffer, character )		((SetBuffer).set[ (unsigned char) (character) ])

// Defined in other files.
static characterset_t g_WhiteSpace;

client_textmessagecustom_t gMessageParms;
client_textmessagecustom_t* gMessageTable = NULL;
int gMessageTableCount = 0;

char gNetworkTextMessageBuffer[MAX_NETMESSAGE][512];
const char* gNetworkMessageNames[MAX_NETMESSAGE] = { NETWORK_MESSAGE1, NETWORK_MESSAGE2, NETWORK_MESSAGE3, NETWORK_MESSAGE4, NETWORK_MESSAGE5, NETWORK_MESSAGE6 };

client_textmessagecustom_t gNetworkTextMessage[MAX_NETMESSAGE] =
{
	{
		0, // effect
		255, 255, 255, 255,
		255, 255, 255, 255,
		-1.0f,						 // x
		-1.0f,						 // y
		0.0f,						 // fadein
		0.0f,						 // fadeout
		0.0f,						 // holdtime
		0.0f,						 // fxTime,
		//NULL,						 // pVGuiSchemeFontName (NULL == default)
		NETWORK_MESSAGE1,			 // pName message name.
		gNetworkTextMessageBuffer[0] // pMessage
	} };

char	gDemoMessageBuffer[512];
client_textmessagecustom_t tm_demomessage =
{
	0, // effect
	255,255,255,255,
	255,255,255,255,
	-1.0f, // x
	-1.0f, // y
	0.0f, // fadein
	0.0f, // fadeout
	0.0f, // holdtime
	0.0f, // fxTime,
	//NULL,// pVGuiSchemeFontName (NULL == default)
	DEMO_MESSAGE,  // pName message name.
	gDemoMessageBuffer    // pMessage
};

static client_textmessagecustom_t orig_demo_message = tm_demomessage;

// The string "pText" is assumed to have all whitespace from both ends cut out
int IsComment(char* pText)
{
	if (pText)
	{
		int length = strlen(pText);

		if (length >= 2 && pText[0] == '/' && pText[1] == '/')
			return 1;

		// No text?
		if (length > 0)
			return 0;
	}
	// No text is a comment too
	return 1;
}

int IsStartOfText(char* pText)
{
	return pText && pText[0] == '{';
}

int IsEndOfText(char* pText)
{
	return pText && pText[0] == '}';
}

#define IsWhiteSpace(space) IN_CHARACTERSET(g_WhiteSpace, space)

const char* SkipSpace(const char* pText)
{
	if (pText)
	{
		int pos = 0;
		while (pText[pos] && IsWhiteSpace(pText[pos]))
			pos++;

		return pText + pos;
	}
	return NULL;
}

const char* SkipText(const char* pText)
{
	if (pText)
	{
		int pos = 0;
		while (pText[pos] && !IsWhiteSpace(pText[pos]))
			pos++;
		return pText + pos;
	}
	return NULL;
}

int ParseFloats(const char* pText, float* pFloat, int count)
{
	const char* pTemp = pText;
	int index = 0;

	while (pTemp && count > 0)
	{
		// Skip current token / float
		pTemp = SkipText(pTemp);
		// Skip any whitespace in between
		pTemp = SkipSpace(pTemp);
		if (pTemp)
		{
			// Parse a float
			pFloat[index] = (float)atof(pTemp);
			count--;
			index++;
		}
	}

	if (count == 0)
		return 1;

	return 0;
}

int ParseString(char const* pText, char* buf, size_t bufsize)
{
	const char* pTemp = pText;

	// Skip current token / float
	pTemp = SkipText(pTemp);
	// Skip any whitespace in between
	pTemp = SkipSpace(pTemp);

	if (pTemp)
	{
		char const* pStart = pTemp;
		pTemp = SkipText(pTemp);

		intp len = V_min(pTemp - pStart + 1, (ptrdiff_t)bufsize - 1);
		strncpy(buf, pStart, len);
		buf[len] = 0;
		return 1;
	}

	return 0;
}

void TrimSpace(const char* source, char* dest)
{
	int start, end, length;

	length = strlen(source);

	for (start = 0; start < length; start++)
	{
		if (!IsWhiteSpace(source[start]))
			break;
	}

	for (end = length - 1; end > start; end--)
	{
		if (!IsWhiteSpace(source[end]))
			break;
	}

	length = end - start + 1;

	if (length <= 0)
	{
		dest[0] = '\0';
	}
	else
	{
		memmove(dest, &source[start], length);
		dest[length] = '\0';
	}
}

int IsToken(const char* pText, const char* pTokenName)
{
	if (!pText || !pTokenName)
		return 0;

	if (!strnicmp(pText + 1, pTokenName, strlen(pTokenName)))
		return 1;

	return 0;
}

static char g_pchSkipName[64];

int ParseDirective(const char* pText)
{
	if (pText && pText[0] == '$')
	{
		float tempFloat[8];

		if (IsToken(pText, "position"))
		{
			if (ParseFloats(pText, tempFloat, 2))
			{
				gMessageParms.x = tempFloat[0];
				gMessageParms.y = tempFloat[1];
			}
		}
		else if (IsToken(pText, "effect"))
		{
			if (ParseFloats(pText, tempFloat, 1))
			{
				gMessageParms.effect = (int)tempFloat[0];
			}
		}
		else if (IsToken(pText, "fxtime"))
		{
			if (ParseFloats(pText, tempFloat, 1))
			{
				gMessageParms.fxtime = tempFloat[0];
			}
		}
		else if (IsToken(pText, "color2"))
		{
			if (ParseFloats(pText, tempFloat, 3))
			{
				gMessageParms.r2 = (int)tempFloat[0];
				gMessageParms.g2 = (int)tempFloat[1];
				gMessageParms.b2 = (int)tempFloat[2];
			}
		}
		else if (IsToken(pText, "color"))
		{
			if (ParseFloats(pText, tempFloat, 3))
			{
				gMessageParms.r1 = (int)tempFloat[0];
				gMessageParms.g1 = (int)tempFloat[1];
				gMessageParms.b1 = (int)tempFloat[2];
			}
		}
		else if (IsToken(pText, "fadein"))
		{
			if (ParseFloats(pText, tempFloat, 1))
			{
				gMessageParms.fadein = tempFloat[0];
			}
		}
		else if (IsToken(pText, "fadeout"))
		{
			if (ParseFloats(pText, tempFloat, 3))
			{
				gMessageParms.fadeout = tempFloat[0];
			}
		}
		else if (IsToken(pText, "holdtime"))
		{
			if (ParseFloats(pText, tempFloat, 3))
			{
				gMessageParms.holdtime = tempFloat[0];
			}
		}
		else if (IsToken(pText, "boxsize"))
		{
			if (ParseFloats(pText, tempFloat, 1))
			{
				gMessageParms.bRoundedRectBackdropBox = tempFloat[0] != 0.0f;
				gMessageParms.flBoxSize = tempFloat[0];
			}
		}
		else if (IsToken(pText, "boxcolor"))
		{
			if (ParseFloats(pText, tempFloat, 4))
			{
				// that's original code, msvc2015 generates illegal instruction on amd64 architecture
				/*for ( int i = 0; i < 4; ++i )
				{
					gMessageParms.boxcolor[ i ] = (byte)(int)tempFloat[ i ];
				}*/

				// workaround
				gMessageParms.boxcolor[0] = (int)tempFloat[0];
				gMessageParms.boxcolor[1] = (int)tempFloat[1];
				gMessageParms.boxcolor[2] = (int)tempFloat[2];
				gMessageParms.boxcolor[3] = (int)tempFloat[3];
			}
		}
		else if (IsToken(pText, "clearmessage"))
		{
			if (ParseString(pText, g_pchSkipName, sizeof(g_pchSkipName)))
			{
				if (!g_pchSkipName[0] || !stricmp(g_pchSkipName, "0"))
				{
					gMessageParms.pClearMessage = NULL;
				}
				else
				{
					gMessageParms.pClearMessage = g_pchSkipName;
				}
			}
		}
		else
		{
			gEngfuncs.Con_DPrintf("Unknown token: %s\n", pText);
		}

		return 1;
	}
	return 0;
}

#define NAME_HEAP_SIZE 16384

void TextMessageParse(unsigned char* pMemFile, int fileSize)
{
	char buf[512], trim[512];
	char* pCurrentText = 0, * pNameHeap;
	char currentName[512], nameHeap[NAME_HEAP_SIZE];
	int lastNamePos;

	int mode = MSGFILE_NAME; // Searching for a message name
	int lineNumber, filePos, lastLinePos;
	int messageCount;

	client_textmessagecustom_t textMessages[MAX_MESSAGES];

	int i, nameHeapSize, textHeapSize, messageSize;
	intp nameOffset;

	lastNamePos = 0;
	lineNumber = 0;
	filePos = 0;
	lastLinePos = 0;
	messageCount = 0;

	bool bSpew = gEngfuncs.CheckParm("-textmessagedebug", nullptr) ? true : false;

	CharacterSetBuild(&g_WhiteSpace, " \r\n\t");

	while (memfgets(pMemFile, fileSize, &filePos, buf, 512) != NULL)
	{
		if (messageCount >= MAX_MESSAGES)
		{
			//Sys_Error("tmessage::TextMessageParse : messageCount>=MAX_MESSAGES");
		}

		TrimSpace(buf, trim);
		switch (mode)
		{
		case MSGFILE_NAME:
			if (IsComment(trim))	// Skip comment lines
				break;

			if (ParseDirective(trim))	// Is this a directive "$command"?, if so parse it and break
				break;

			if (IsStartOfText(trim))
			{
				mode = MSGFILE_TEXT;
				pCurrentText = (char*)(pMemFile + filePos);
				break;
			}
			if (IsEndOfText(trim))
			{
				gEngfuncs.Con_DPrintf("Unexpected '}' found, line %d\n", lineNumber);
				return;
			}
			strncpy(currentName, trim, sizeof(currentName));
			break;

		case MSGFILE_TEXT:
			if (IsEndOfText(trim))
			{
				int length = strlen(currentName);

				// Save name on name heap
				if (lastNamePos + length > 8192)
				{
					gEngfuncs.Con_DPrintf("Error parsing file!\n");
					return;
				}
				strcpy(nameHeap + lastNamePos, currentName);

				// Terminate text in-place in the memory file (it's temporary memory that will be deleted)
				// If the string starts with #, it's a localization string and we don't
				// want the \n (or \r) on the end or the Find() lookup will fail (so subtract 2)
				if (pCurrentText && pCurrentText[0] && pCurrentText[0] == '#' && lastLinePos > 1 &&
					((pMemFile[lastLinePos - 2] == '\n') || (pMemFile[lastLinePos - 2] == '\r')))
				{
					pMemFile[lastLinePos - 2] = 0;
				}
				else
				{
					pMemFile[lastLinePos - 1] = 0;
				}

				// Save name/text on heap
				textMessages[messageCount] = gMessageParms;
				textMessages[messageCount].pName = nameHeap + lastNamePos;
				lastNamePos += strlen(currentName) + 1;
				if (gMessageParms.pClearMessage)
				{
					strncpy(nameHeap + lastNamePos, textMessages[messageCount].pClearMessage, strlen(textMessages[messageCount].pClearMessage) + 1);
					textMessages[messageCount].pClearMessage = nameHeap + lastNamePos;
					lastNamePos += strlen(textMessages[messageCount].pClearMessage) + 1;
				}
				textMessages[messageCount].pMessage = pCurrentText;

				if (bSpew)
				{
					client_textmessagecustom_t* m = &textMessages[messageCount];
					//Msg("%d %s\n",
					//	messageCount, m->pName ? m->pName : "(null)");
					//Msg("  effect %d, color1(%d,%d,%d,%d), color2(%d,%d,%d,%d)\n",
					//	m->effect, m->r1, m->g1, m->b1, m->a1, m->r2, m->g2, m->b2, m->a2);
					//Msg("  pos %f,%f, fadein %f fadeout %f hold %f fxtime %f\n",
					//	m->x, m->y, m->fadein, m->fadeout, m->holdtime, m->fxtime);
					//Msg("  '%s'\n", m->pMessage ? m->pMessage : "(null)");
					//
					//Msg("  box %s, size %f, color(%d,%d,%d,%d)\n",
					//	m->bRoundedRectBackdropBox ? "yes" : "no", m->flBoxSize, m->boxcolor[0], m->boxcolor[1], m->boxcolor[2], m->boxcolor[3]);
					//
					//if (m->pClearMessage)
					//{
					//	Msg("  will clear '%s'\n", m->pClearMessage);
					//}
				}

				messageCount++;

				// Reset parser to search for names
				mode = MSGFILE_NAME;
				break;
			}
			if (IsStartOfText(trim))
			{
				gEngfuncs.Con_DPrintf("Unexpected '{' found, line %d\n", lineNumber);
				return;
			}
			break;
		}
		lineNumber++;
		lastLinePos = filePos;

		if (messageCount >= MAX_MESSAGES)
		{
			gEngfuncs.Con_Printf("WARNING: TOO MANY MESSAGES IN TITLES.TXT, MAX IS %d\n", MAX_MESSAGES);
			break;
		}
	}

	gEngfuncs.Con_DPrintf("Parsed %d text messages\n", messageCount);
	nameHeapSize = lastNamePos;
	textHeapSize = 0;
	for (i = 0; i < messageCount; i++)
		textHeapSize += strlen(textMessages[i].pMessage) + 1;

	messageSize = (messageCount * sizeof(client_textmessagecustom_t));

	// Must malloc because we need to be able to clear it after initialization
	gMessageTable = (client_textmessagecustom_t*)malloc(textHeapSize + nameHeapSize + messageSize);

	// Copy table over
	memcpy(gMessageTable, textMessages, messageSize);

	// Copy Name heap
	pNameHeap = ((char*)gMessageTable) + messageSize;
	memcpy(pNameHeap, nameHeap, nameHeapSize);
	nameOffset = pNameHeap - gMessageTable[0].pName;

	// Copy text & fixup pointers
	pCurrentText = pNameHeap + nameHeapSize;

	for (i = 0; i < messageCount; i++)
	{
		gMessageTable[i].pName += nameOffset;
		if (gMessageTable[i].pClearMessage)
		{
			gMessageTable[i].pClearMessage += nameOffset;
		}
		strcpy(pCurrentText, gMessageTable[i].pMessage); // Copy text over
		gMessageTable[i].pMessage = pCurrentText;
		pCurrentText += strlen(pCurrentText) + 1;
	}

#if _DEBUG
	if ((pCurrentText - (char*)gMessageTable) != (textHeapSize + nameHeapSize + messageSize))
		gEngfuncs.Con_Printf("Overflow text message buffer!!!!!\n");
#endif
	gMessageTableCount = messageCount;
}

void TextMessageShutdown(void)
{
	if (gMessageTable)
	{
		free(gMessageTable);
		gMessageTable = NULL;
	}
}

void TextMessageInit(void)
{
	std::vector<std::byte> pMemFile;

	// Clear out any old data that's sitting around.
	if (gMessageTable)
	{
		free(gMessageTable);
		gMessageTable = NULL;
	}

	static cvar_t* r_titleslang = gEngfuncs.pfnGetCvarPointer("r_titleslang");

	pMemFile = FileSystem_LoadFileIntoBuffer(r_titleslang->string, FileContentFormat::Text);

	if (!pMemFile.empty())
	{
		TextMessageParse((byte*)pMemFile.data(), pMemFile.size());
		pMemFile.clear();
	}

	for (int i = 0; i < MAX_NETMESSAGE; i++)
	{
		gNetworkTextMessage[i].pMessage =
			gNetworkTextMessageBuffer[i];
	}
}

void TextMessage_DemoMessage(const char* pszMessage, float fFadeInTime, float fFadeOutTime, float fHoldTime)
{
	if (!pszMessage || !pszMessage[0])
		return;

	// Restore
	tm_demomessage = orig_demo_message;

	strncpy(gDemoMessageBuffer, (char*)pszMessage, sizeof(gDemoMessageBuffer));
	tm_demomessage.fadein = fFadeInTime;
	tm_demomessage.fadeout = fFadeOutTime;
	tm_demomessage.holdtime = fHoldTime;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pszMessage -
//			*message -
//-----------------------------------------------------------------------------
void TextMessage_DemoMessageFull(const char* pszMessage, client_textmessagecustom_t const* message)
{
	assert(message);
	if (!message)
		return;

	if (!pszMessage || !pszMessage[0])
		return;

	memcpy(&tm_demomessage, message, sizeof(tm_demomessage));
	tm_demomessage.pMessage = orig_demo_message.pMessage;
	tm_demomessage.pName = orig_demo_message.pName;
	strncpy(gDemoMessageBuffer, pszMessage, sizeof(gDemoMessageBuffer));
}


client_textmessagecustom_t* TextMessageGet(const char* pName)
{
	if (!stricmp(pName, DEMO_MESSAGE))
		return &tm_demomessage;

	// HACKHACK -- add 4 "channels" of network text
	if (!stricmp(pName, NETWORK_MESSAGE1))
		return gNetworkTextMessage;
	else if (!stricmp(pName, NETWORK_MESSAGE2))
		return gNetworkTextMessage + 1;
	else if (!stricmp(pName, NETWORK_MESSAGE3))
		return gNetworkTextMessage + 2;
	else if (!stricmp(pName, NETWORK_MESSAGE4))
		return gNetworkTextMessage + 3;
	else if (!stricmp(pName, NETWORK_MESSAGE5))
		return gNetworkTextMessage + 4;
	else if (!stricmp(pName, NETWORK_MESSAGE6))
		return gNetworkTextMessage + 5;

	for (int i = 0; i < gMessageTableCount; i++)
	{
		if (!stricmp(pName, gMessageTable[i].pName))
			return &gMessageTable[i];
	}

	return NULL;
}