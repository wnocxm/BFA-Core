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
#include "SpellScript.h"

// 269807 - Leave for Lordaeron Queue
// Isabella (Orgrimmar) makes the player cast this from her gossip (SmartAI, see world SQL).
// Puts the player in the LFG queue for The Battle for Lordaeron.
class spell_leave_for_lordaeron_queue : public SpellScript
{
    PrepareSpellScript(spell_leave_for_lordaeron_queue);

    void HandleAfterCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;

        sLFGMgr->JoinLfg(player, player->GetTeamId() == TEAM_ALLIANCE
            ? uint32(LFG_DUNGEON_BATTLE_FOR_LORDAERON_A) : uint32(LFG_DUNGEON_BATTLE_FOR_LORDAERON_H));
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_leave_for_lordaeron_queue::HandleAfterCast);
    }
};

void AddSC_the_battle_for_lordaeron()
{
    RegisterSpellScript(spell_leave_for_lordaeron_queue);
}
