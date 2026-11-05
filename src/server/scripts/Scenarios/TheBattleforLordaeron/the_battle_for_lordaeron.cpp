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
// Spies in Our Midst: follows the players and fights whoever is in the way.
// Afterwards he leads the way to the heart of the city and later to the front line; the
// instance script tells him where to go and hears when he arrives.
struct npc_lordaeron_saurfang : public ScriptedAI
{
    npc_lordaeron_saurfang(Creature* creature) : ScriptedAI(creature) { }

    static constexpr float ENGAGE_RANGE = 20.0f;
    static constexpr float FOLLOW_DISTANCE = 3.0f;

    void Reset() override
    {
        // The template is disarmed and not selectable; here he has to fight.
        me->RemoveUnitFlag(UnitFlags(UNIT_FLAG_DISARMED | UNIT_FLAG_NOT_SELECTABLE));
        me->SetReactState(REACT_AGGRESSIVE);
    }

    void DoAction(int32 action) override
    {
        TC_LOG_INFO("scripts", "Battle for Lordaeron: Saurfang action %d", action);
        _fightAlongside = action == ACTION_SAURFANG_FIGHT_ALONGSIDE;

        switch (action)
        {
            case ACTION_SAURFANG_FIGHT_ALONGSIDE:
                FollowPlayers();
                break;
            case ACTION_SAURFANG_GO_TO_AMBUSH:
                MoveTo(POINT_SAURFANG_CANAL, SaurfangCanal);
                break;
            case ACTION_SAURFANG_GO_TO_GATE:
                MoveTo(POINT_SAURFANG_GATE, SaurfangGate);
                break;
            case ACTION_SAURFANG_GO_TO_HEART:
                MoveTo(POINT_SAURFANG_HEART, SaurfangHeart);
                break;
            case ACTION_SAURFANG_GO_TO_FRONT_LINE:
                MoveTo(POINT_SAURFANG_FRONT_LINE, SaurfangFrontLine);
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
        {
            // Engage the closest enemy around while fighting alongside the players.
            if (_fightAlongside)
                if (Unit* enemy = me->SelectNearestTarget(ENGAGE_RANGE))
                    AttackStart(enemy);
            return;
        }

        DoMeleeAttackIfReady();
    }

    // After a fight he goes back to the players (or keeps walking) instead of
    // returning to where he was summoned.
    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        me->RemoveAllAuras();
        me->CombatStop(true);
        me->ResetPlayerDamageReq();
        Reset();

        if (_fightAlongside)
            FollowPlayers();
        else if (_pendingPointId)
            me->GetMotionMaster()->MovePoint(_pendingPointId, _pendingPoint);
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (type != POINT_MOTION_TYPE || pointId != _pendingPointId)
            return;

        _pendingPointId = 0;

        if (pointId == POINT_SAURFANG_CANAL)
        {
            MoveTo(POINT_SAURFANG_AMBUSH, SaurfangAmbush);
            return;
        }

        if (InstanceScript* instance = me->GetInstanceScript())
            instance->SetData(DATA_SAURFANG_REACHED, pointId);
    }

    // He fights alongside the player but the story needs him alive.
    void DamageTaken(Unit* /*attacker*/, uint32& damage) override
    {
        if (damage >= me->GetHealth())
            damage = me->GetHealth() - 1;
    }

private:
    void FollowPlayers()
    {
        for (auto& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (player && player->IsAlive())
            {
                me->GetMotionMaster()->MoveFollow(player, FOLLOW_DISTANCE, float(M_PI) / 2.0f);
                return;
            }
        }
    }

    void MoveTo(uint32 pointId, Position const& pos)
    {
        _pendingPointId = pointId;
        _pendingPoint = pos;
        me->SetHomePosition(pos);
        me->SetWalk(false);
        me->GetMotionMaster()->MovePoint(pointId, pos);
    }

    bool _fightAlongside = false;
    uint32 _pendingPointId = 0;
    Position _pendingPoint;
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
class spell_lordaeron_evacuate : public SpellScript
{
    PrepareSpellScript(spell_lordaeron_evacuate);

    void HandleAfterCast()
    {
        if (InstanceScript* instance = GetCaster()->GetInstanceScript())
            instance->SetGuidData(DATA_EVACUATE_NEAR, GetCaster()->GetGUID());
    }

    void Register() override
    {
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
