//===========================================================================//
//
// Author: Nanoman2525
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
//===========================================================================//

#include "scanner.hpp"
#include "modules.hpp"

#include "eiface.h"
#include "public/cdll_int.h"
#include "public/steam/steamclientpublic.h"
#include "vscript/ivscript.h"
#include "irecipientfilter.h"
#include "p2mm.hpp"

extern IVEngineServer *engine;
extern CGlobalVars *gpGlobals;
extern IScriptVM *g_pScriptVM;
extern IServerTools *g_pServerTools;

ConVar p2mm_developer("p2mm_developer", "0");

const char *GetPlayerName(int index)
{
	if (index <= 0)
	{
		return "";
	}

	player_info_t playerinfo;
	if (!engine->GetPlayerInfo(index, &playerinfo))
	{
		return "";
	}

	return playerinfo.name;
}
int GetSteamID(int index)
{
	edict_t *pEdict = NULL;
	if ( index >= 0 && index < gpGlobals->maxEntities )
	{
		pEdict = (edict_t *)(gpGlobals->pEdicts + index);
	}
	if (!pEdict)
	{
		return -1;
	}

	player_info_t playerinfo;
	if (!engine->GetPlayerInfo(index, &playerinfo))
	{
		return -1;
	}

	const CSteamID* pSteamID = engine->GetClientSteamID(pEdict);
	if (!pSteamID || pSteamID->GetAccountID() == 0)
	{
		return -1;
	}

	return pSteamID->GetAccountID();
}
bool IsMapValid(const char* map)
{
	return engine->IsMapValid(map);
}
int GetDeveloperLevelP2MM()
{
	return p2mm_developer.GetInt();
}
void SetPhysTypeConvar(int newval)
{
	g_pCVar->FindVar("player_held_object_use_view_model")->SetValue(newval);
}
void SetMaxPortalSeparationConvar(int newval)
{
	if (engine->IsDedicatedServer())
	{
		return;
	}
	g_pCVar->FindVar("portal_max_separation_force")->SetValue(newval);
}
bool IsDedicatedServer()
{
	return engine->IsDedicatedServer();
}
void InitializeEntity(HSCRIPT ent)
{
	static uintptr_t func = (uintptr_t)Memory::Scanner::Scan<void*>(Memory::Modules::Get("server"), "E8 ?? ?? ?? ?? 8B 4D 18 8B 57 5C", 1);
	static auto GetCBaseEntityScriptDesc = reinterpret_cast<ScriptClassDesc_t *(*)()>(*reinterpret_cast<uintptr_t *>(func) + func + sizeof(func));
	void *pEntity = reinterpret_cast<void *>(g_pScriptVM->GetInstanceValue(ent, GetCBaseEntityScriptDesc()));;
	if (pEntity)
	{
		g_pServerTools->DispatchSpawn(pEntity);

		static auto Activate = *reinterpret_cast<void(__thiscall **)(void *)>(*reinterpret_cast<uintptr_t *>(pEntity) + 148);
		Activate(pEntity);
	}
}

class CFilter : public IRecipientFilter
{
public:
	CFilter() { recipient_count = 0; };
	~CFilter() {};

	virtual bool IsReliable() const { return false; };
	virtual bool IsInitMessage() const { return false; };

	virtual int GetRecipientCount() const { return recipient_count; };
	virtual int GetRecipientIndex(int slot) const { return ((slot < 0) || (slot >= recipient_count)) ? -1 : recipients[slot]; };
	void AddPlayer(int player_index)
	{
		if (recipient_count > 255)
		{
			return;
		}

		recipients[recipient_count] = player_index;
		recipient_count++;
	}

private:
	int recipients[256];
	int recipient_count;
};
void SendToChat(const char *msg, int index)
{
	if (!msg)
	{
		return;
	}
	bf_write *netmsg;
	CFilter recipient_filter;

	// Send to all players
	if (index == 0)
	{
		for (int i = 1; i < gpGlobals->maxClients; i++)
		{
			player_info_t playerinfo;
			if (engine->GetPlayerInfo(i, &playerinfo))
			{
				recipient_filter.AddPlayer(i);
			}
		}
	}
	else
	{
		recipient_filter.AddPlayer(index);
	}

	netmsg = engine->UserMessageBegin(&recipient_filter, 3, "SayText2");
	netmsg->WriteByte(0);
	netmsg->WriteString(msg);
	netmsg->WriteByte(1);
	engine->MessageEnd();
}
const char *GetGameDirectory()
{
	return CommandLine()->ParmValue("-game", CommandLine()->ParmValue("-defaultgamedir", "hl2"));
}

void RegisterFuncsAndRun()
{
    g_pScriptVM = **Memory::Scanner::Scan<IScriptVM ***>(Memory::Modules::Get("server"), "8B 1D ?? ?? ?? ?? 57 85 DB", 2);
	if ( !g_pScriptVM )
	{
		P2MMLog( "Could not run or register our VScript stuff.", 1 );
		return;
	}

	ScriptRegisterFunction(g_pScriptVM, GetPlayerName, "Gets player username by index.");
	ScriptRegisterFunction(g_pScriptVM, GetSteamID, "Gets the account ID component of player SteamID by index.");
	ScriptRegisterFunction(g_pScriptVM, SetPhysTypeConvar, "Sets 'player_held_object_use_view_model' to supplied integer value.");
	ScriptRegisterFunction(g_pScriptVM, SetMaxPortalSeparationConvar, "Sets 'portal_max_separation_force' to supplied integer value.");
	ScriptRegisterFunction(g_pScriptVM, IsDedicatedServer, "Returns true if this is a dedicated server.");
	ScriptRegisterFunction(g_pScriptVM, IsMapValid, "Returns true is the supplied string is a valid map name.");
	ScriptRegisterFunction(g_pScriptVM, GetDeveloperLevelP2MM, "Returns the value of p2mm_developer.");
	ScriptRegisterFunction(g_pScriptVM, SendToChat, "Sends a raw message to the chat HUD.");
	ScriptRegisterFunction(g_pScriptVM, InitializeEntity, "Inits an entity.");
	ScriptRegisterFunction(g_pScriptVM, GetGameDirectory, "Game dir.");

	g_pScriptVM->Run("IncludeScript(\"multiplayermod/p2mm\");");
}
