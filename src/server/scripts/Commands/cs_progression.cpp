/*
 * Copyright (C) 2020 BfaCore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
Name: progression_commandscript
%Complete: 100
Comment: Opens and closes content patches on a progressive realm
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ProgressionMgr.h"
#include "RBAC.h"
#include "World.h"
#include <cstdlib>
#include <cstring>

class progression_commandscript : public CommandScript
{
public:
    progression_commandscript() : CommandScript("progression_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> progressionCommandTable =
        {
            { "info",   rbac::RBAC_PERM_COMMAND_PROGRESSION_INFO,   true, &HandleInfoCommand,   "" },
            { "list",   rbac::RBAC_PERM_COMMAND_PROGRESSION_INFO,   true, &HandleListCommand,   "" },
            { "check",  rbac::RBAC_PERM_COMMAND_PROGRESSION_INFO,   true, &HandleCheckCommand,  "" },
            { "set",    rbac::RBAC_PERM_COMMAND_PROGRESSION_SET,    true, &HandleSetCommand,    "" },
            { "next",   rbac::RBAC_PERM_COMMAND_PROGRESSION_SET,    true, &HandleNextCommand,   "" },
            { "reload", rbac::RBAC_PERM_COMMAND_PROGRESSION_RELOAD, true, &HandleReloadCommand, "" },
        };
        static std::vector<ChatCommand> commandTable =
        {
            { "progression", rbac::RBAC_PERM_COMMAND_PROGRESSION, true, nullptr, "", progressionCommandTable },
        };
        return commandTable;
    }

    static bool CheckEnabled(ChatHandler* handler)
    {
        if (sProgressionMgr->IsEnabled())
            return true;

        handler->SendSysMessage("Progression is disabled (Progression.Enable = 0). All content is open.");
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleInfoCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return false;

        uint32 patch = sProgressionMgr->GetCurrentPatch();
        ProgressionPatchInfo const* info = sProgressionMgr->GetCurrentPatchInfo();
        handler->PSendSysMessage("Content patch: %s - %s", ProgressionMgr::PatchToString(patch).c_str(), info ? info->Name.c_str() : "?");
        handler->PSendSysMessage("Mythic+ season: %u, PvP season: %u", sProgressionMgr->GetMythicPlusSeason(), sProgressionMgr->GetPvpSeason());
        handler->PSendSysMessage("Item level cap: %u, Heart of Azeroth cap: %u", sProgressionMgr->GetItemLevelCap(), info ? info->AzeriteLevelCap : 0);
        handler->PSendSysMessage("Essences: %s, Corruption: %s, Heart of Azeroth catch-up: %s",
            sProgressionMgr->IsFeatureAvailable(PROGRESSION_FEATURE_AZERITE_ESSENCES) ? "open" : "locked",
            sProgressionMgr->IsFeatureAvailable(PROGRESSION_FEATURE_CORRUPTION) ? "open" : "locked",
            sProgressionMgr->IsFeatureAvailable(PROGRESSION_FEATURE_AZERITE_KNOWLEDGE) ? "open" : "locked");

        if (uint32 next = sProgressionMgr->GetNextPatch())
            handler->PSendSysMessage("Next patch: %s (.progression next)", ProgressionMgr::PatchToString(next).c_str());
        else
            handler->SendSysMessage("This is the last patch.");
        return true;
    }

    static bool HandleListCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return false;

        for (auto const& patchPair : sProgressionMgr->GetPatches())
        {
            ProgressionPatchInfo const& info = patchPair.second;
            handler->PSendSysMessage("%s %s - %s (M+ %u, PvP %u, ilvl cap %u)", info.Patch == sProgressionMgr->GetCurrentPatch() ? ">>" : "  ",
                ProgressionMgr::PatchToString(info.Patch).c_str(), info.Name.c_str(), info.MythicPlusSeason, info.PvpSeason, info.ItemLevelCap);
        }
        return true;
    }

    static bool ChangePatch(ChatHandler* handler, uint32 patch)
    {
        uint32 oldPatch = sProgressionMgr->GetCurrentPatch();
        if (!sProgressionMgr->SetCurrentPatch(patch))
        {
            handler->PSendSysMessage("Unknown patch %s. Use .progression list.", ProgressionMgr::PatchToString(patch).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        ProgressionPatchInfo const* info = sProgressionMgr->GetCurrentPatchInfo();
        std::string message = "Content patch " + ProgressionMgr::PatchToString(patch) + (info ? " - " + info->Name : "") + " is now live!";
        sWorld->SendServerMessage(SERVER_MSG_STRING, message);

        handler->PSendSysMessage("Content patch changed from %s to %s.", ProgressionMgr::PatchToString(oldPatch).c_str(), ProgressionMgr::PatchToString(patch).c_str());
        handler->SendSysMessage("Maps, quests, loot, vendors, races, seasons and caps changed now. NPC and object spawns change when their grid loads again (restart to be sure).");
        return true;
    }

    // .progression set 8.1.5 | 80105
    static bool HandleSetCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return false;

        if (!*args)
            return false;

        uint32 patch = ProgressionMgr::ParsePatch(args);
        if (!patch)
            return false;

        return ChangePatch(handler, patch);
    }

    // .progression next - opens the next patch
    static bool HandleNextCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return false;

        uint32 next = sProgressionMgr->GetNextPatch();
        if (!next)
        {
            handler->SendSysMessage("The realm is already on the last patch.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        return ChangePatch(handler, next);
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckEnabled(handler))
            return false;

        sProgressionMgr->LoadFromDB();
        handler->SendSysMessage("Progression tables reloaded.");
        return true;
    }

    // .progression check <type> <entry>
    static bool HandleCheckCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckEnabled(handler))
            return false;

        char* typeStr = strtok((char*)args, " ");
        char* entryStr = strtok(nullptr, " ");
        if (!typeStr || !entryStr)
        {
            handler->SendSysMessage("Usage: .progression check <type> <entry>. Types: 0 map, 1 quest, 2 quest zone, 3 zone, 4 creature, 5 gameobject, "
                "6 creature guid, 7 gameobject guid, 8 item, 9 race, 10 feature, 11 LFG dungeon");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 type = uint32(strtoul(typeStr, nullptr, 10));
        uint32 entry = uint32(strtoul(entryStr, nullptr, 10));
        if (type >= MAX_PROGRESSION_CONTENT_TYPE)
            return false;

        uint32 minPatch = 0, maxPatch = 0;
        if (!sProgressionMgr->GetRule(ProgressionContentType(type), entry, minPatch, maxPatch))
        {
            handler->PSendSysMessage("Type %u entry %u has no rule: always open.", type, entry);
            return true;
        }

        handler->PSendSysMessage("Type %u entry %u opens in %s%s%s: %s now.", type, entry, ProgressionMgr::PatchToString(minPatch).c_str(),
            maxPatch ? " and closes in " : "", maxPatch ? ProgressionMgr::PatchToString(maxPatch).c_str() : "",
            sProgressionMgr->IsAvailable(ProgressionContentType(type), entry) ? "OPEN" : "LOCKED");
        return true;
    }
};

void AddSC_progression_commandscript()
{
    new progression_commandscript();
}
