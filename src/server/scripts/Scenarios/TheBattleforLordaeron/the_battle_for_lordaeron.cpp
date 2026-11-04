/*
 * Copyright (C) 2021 BfaCore Reforged
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

#include "the_battle_for_lordaeron.h"
#include "DB2Stores.h"
#include "LFGMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptedGossip.h"

enum IsabellaGossip
{
    GOSSIP_MENU_ISABELLA            = 19870,
    GOSSIP_OPTION_SAIL_TO_LORDAERON = 1, // "I am ready to sail to Lordaeron."
    GOSSIP_OPTION_SKIP_SCENARIO     = 2, // "I've heard this tale before... <Skip the scenario...>"

    ACTION_SAIL_TO_LORDAERON        = GOSSIP_ACTION_INFO_DEF + 1,
    ACTION_SKIP_SCENARIO            = GOSSIP_ACTION_INFO_DEF + 2,
};

// Isabella (Orgrimmar) queues Horde players for The Battle for Lordaeron (LFG 1759).
// The DB menu gives both options the same action, so the menu is built here.
class npc_isabella_battle_for_lordaeron : public CreatureScript
{
public:
    npc_isabella_battle_for_lordaeron() : CreatureScript("npc_isabella_battle_for_lordaeron") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (player->GetQuestStatus(QUEST_THE_BATTLE_FOR_LORDAERON_H) != QUEST_STATUS_INCOMPLETE)
            return false; // default DB gossip

        ClearGossipMenuFor(player);
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        AddGossipItemFor(player, GOSSIP_MENU_ISABELLA, GOSSIP_OPTION_SAIL_TO_LORDAERON, GOSSIP_SENDER_MAIN, ACTION_SAIL_TO_LORDAERON);
        AddGossipItemFor(player, GOSSIP_MENU_ISABELLA, GOSSIP_OPTION_SKIP_SCENARIO, GOSSIP_SENDER_MAIN, ACTION_SKIP_SCENARIO);
        SendGossipMenuFor(player, player->GetGossipTextId(GOSSIP_MENU_ISABELLA, creature), creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*sender*/, uint32 action) override
    {
        CloseGossipMenuFor(player);

        switch (action)
        {
            case ACTION_SAIL_TO_LORDAERON:
                sLFGMgr->JoinLfg(player, LFG_DUNGEON_BATTLE_FOR_LORDAERON_H);
                break;
            case ACTION_SKIP_SCENARIO:
                player->CompleteQuest(QUEST_THE_BATTLE_FOR_LORDAERON_H);
                break;
            default:
                return false;
        }
        return true;
    }
};

void AddSC_the_battle_for_lordaeron()
{
    new npc_isabella_battle_for_lordaeron();
}
