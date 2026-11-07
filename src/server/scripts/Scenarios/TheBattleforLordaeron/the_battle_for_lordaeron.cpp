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
#include "InstanceScript.h"
#include "LFGMgr.h"
#include "Log.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"

using namespace BattleForLordaeron;

// 130367 - High Overlord Saurfang (Horde scenario)
// The instance script moves him around; this AI only lets him fight and keeps him alive.
struct npc_lordaeron_saurfang : public ScriptedAI
{
    npc_lordaeron_saurfang(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        // The template is disarmed and not selectable; here he has to fight.
        me->RemoveUnitFlag(UnitFlags(UNIT_FLAG_DISARMED | UNIT_FLAG_NOT_SELECTABLE));
        me->SetReactState(REACT_AGGRESSIVE);
    }

    // After a fight he stays where he is; the instance script tells him where to go next.
    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        me->RemoveAllAuras();
        me->CombatStop(true);
        me->ResetPlayerDamageReq();
        me->GetMotionMaster()->Clear();
        Reset();
    }

    // He fights alongside the players but the story needs him alive.
    void DamageTaken(Unit* /*attacker*/, uint32& damage) override
    {
        if (damage >= me->GetHealth())
            damage = me->GetHealth() - 1;
    }
};

// 269807 - Leave for Lordaeron Queue
// Isabella (Orgrimmar) makes the player cast this from her gossip (SmartAI).
class spell_lordaeron_leave_for_lordaeron_queue : public SpellScript
{
    PrepareSpellScript(spell_lordaeron_leave_for_lordaeron_queue);

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
        AfterCast += SpellCastFn(spell_lordaeron_leave_for_lordaeron_queue::HandleAfterCast);
    }
};

// 261807 - Evacuate!
// Granted by 261808 (override bar 995) during "Evacuate the Heart of the City".
// Every citizen the spell hits is evacuated; citizens around the caster count as well.
class spell_lordaeron_evacuate : public SpellScript
{
    PrepareSpellScript(spell_lordaeron_evacuate);

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Creature* citizen = GetHitCreature())
            if (InstanceScript* instance = GetCaster()->GetInstanceScript())
                instance->SetGuidData(DATA_EVACUATE_CITIZEN, citizen->GetGUID());
    }

    void HandleAfterCast()
    {
        if (InstanceScript* instance = GetCaster()->GetInstanceScript())
            instance->SetGuidData(DATA_EVACUATE_NEAR, GetCaster()->GetGUID());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lordaeron_evacuate::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_lordaeron_evacuate::HandleAfterCast);
    }
};

// 259094 - Teleport to Lordaeron
// Cast on spellclick of Anastasia's portal in the heart of the city; the teleport itself
// uses spell_target_position.
class spell_lordaeron_teleport_to_lordaeron : public SpellScript
{
    PrepareSpellScript(spell_lordaeron_teleport_to_lordaeron);

    void HandleAfterCast()
    {
        if (InstanceScript* instance = GetCaster()->GetInstanceScript())
            instance->SetGuidData(DATA_PLAYER_USED_KEEP_PORTAL, GetCaster()->GetGUID());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_lordaeron_teleport_to_lordaeron::HandleAfterCast);
    }
};

void AddSC_the_battle_for_lordaeron()
{
    RegisterCreatureAI(npc_lordaeron_saurfang);
    RegisterSpellScript(spell_lordaeron_leave_for_lordaeron_queue);
    RegisterSpellScript(spell_lordaeron_evacuate);
    RegisterSpellScript(spell_lordaeron_teleport_to_lordaeron);
}
