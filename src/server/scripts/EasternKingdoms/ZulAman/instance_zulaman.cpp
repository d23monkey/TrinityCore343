/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "InstanceMapScript.h"
#include "InstanceScript.h"
#include "Player.h"
#include "TemporarySummon.h"
#include "zulaman.h"

enum Misc
{
    RAND_VENDOR                    = 2,
    WORLDSTATE_SHOW_TIMER          = 3104,
    WORLDSTATE_TIME_TO_SACRIFICE   = 3106
};

DoorData const doorData[] =
{
    { GO_ZULJIN_FIREWALL,      DATA_ZULJIN,  DOOR_TYPE_ROOM    },
    { GO_DOOR_HALAZZI,         DATA_HALAZZI, DOOR_TYPE_PASSAGE },
    { GO_LYNX_TEMPLE_ENTRANCE, DATA_HALAZZI, DOOR_TYPE_ROOM    },
    { GO_DOOR_AKILZON,         DATA_AKILZON, DOOR_TYPE_ROOM    },
    { GO_GATE_ZULJIN,          DATA_HEXLORD, DOOR_TYPE_PASSAGE },
    { 0,                       0,            DOOR_TYPE_ROOM    } // END
};

ObjectData const creatureData[] =
{
    { NPC_JANALAI,        DATA_JANALAI        },
    { NPC_SPIRIT_LYNX,    DATA_SPIRIT_LYNX    },
    { NPC_HARRISON_JONES, DATA_HARRISON_JONES },
    { NPC_KRAZ,           DATA_KRAZ           },
    { NPC_TANZAR,         DATA_TANZAR         },
    { NPC_HARKOR,         DATA_HARKOR         },
    { NPC_ASHLI,          DATA_ASHLI          },
    { 0,                  0                   }
};

ObjectData const gameObjectData[] =
{
    { GO_STRANGE_GONG, DATA_STRANGE_GONG },
    { GO_MASSIVE_GATE, DATA_MASSIVE_GATE },
    { GO_GATE_HEXLORD, DATA_HEXLORD_GATE },
    { 0,               0                 }
};

ObjectData const summonData[] =
{
    { NPC_AMANI_HATCHLING, DATA_JANALAI },
    { 0,                   0            }
};

BossBoundaryData const boundaries =
{
    { DATA_HEXLORD,    new RectangleBoundary(80.50557f, 920.9858f, 155.88986f, 1015.27563f)}
};

class instance_zulaman : public InstanceMapScript
{
public:
    instance_zulaman() : InstanceMapScript("instance_zulaman", 568) { }

    struct instance_zulaman_InstanceMapScript : public InstanceScript
    {
        instance_zulaman_InstanceMapScript(Map* map) : InstanceScript(map) {}

        void Initialize() override
        {
            SetHeaders(DataHeader);
            SetBossNumber(MAX_ENCOUNTER);
            SetPersistentDataCount(PersistentDataCount);
            LoadObjectData(creatureData, gameObjectData);
            LoadBossBoundaries(boundaries);
            LoadDoorData(doorData);
            LoadSummonData(summonData);

            for (uint8 i = 0; i < RAND_VENDOR; ++i)
                RandVendor[i] = NOT_STARTED;
        }

        void OnPlayerEnter(Player* /*player*/) override
        {
            if (!scheduler.IsGroupScheduled(GROUP_TIMED_RUN))
                DoAction(ACTION_START_TIMED_RUN);
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            if (go->GetEntry() == GO_GATE_HEXLORD)
                CheckInstanceStatus();

            InstanceScript::OnGameObjectCreate(go);
        }

        void KillHostages()
        {
            if (AllBossesDone({ DATA_NALORAKK, DATA_AKILZON, DATA_JANALAI, DATA_HALAZZI }))
                return;

            for (uint8 i = DATA_ASHLI; i < DATA_KRAZ; ++i)
            {
                Creature* hostage = GetCreature(i);
                if (!hostage)
                    break;

                if (i == DATA_ASHLI && GetBossState(DATA_HALAZZI) == DONE)
                    break;

                if (i == DATA_TANZAR && GetBossState(DATA_NALORAKK) == DONE)
                    break;

                if (i == DATA_HARKOR && GetBossState(DATA_AKILZON) == DONE)
                    break;

                if (i == DATA_KRAZ && GetBossState(DATA_JANALAI) == DONE)
                    break;

                hostage->UpdateEntry(i + 24427);
                hostage->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                hostage->CastSpell(hostage, SPELL_COSMETIC_IMMOLATION);
                hostage->HandleEmoteCommand(EMOTE_STATE_DEAD); // Not confirmed
            }
        }

        void DoAction(int32 actionId) override
        {
            if (actionId == ACTION_START_TIMED_RUN)
            {
                if (uint32 timer = GetPersistentData(DATA_TIMED_RUN))
                {
                    DoUpdateWorldState(WORLDSTATE_SHOW_TIMER, 1);
                    DoUpdateWorldState(WORLDSTATE_TIME_TO_SACRIFICE, timer);
                }

                scheduler.Schedule(1min, GROUP_TIMED_RUN, [this](TaskContext context)
                {
                    if (uint32 timer = GetPersistentData(DATA_TIMED_RUN))
                    {
                        --timer;
                        DoUpdateWorldState(WORLDSTATE_SHOW_TIMER, 1);
                        DoUpdateWorldState(WORLDSTATE_TIME_TO_SACRIFICE, timer);
                        StorePersistentData(DATA_TIMED_RUN, timer);
                        context.Repeat();
                    }
                    else
                        DoUpdateWorldState(WORLDSTATE_SHOW_TIMER, 0);
                });
            }
        }

        void CheckInstanceStatus()
        {
            if (AllBossesDone({ DATA_NALORAKK, DATA_AKILZON, DATA_JANALAI, DATA_HALAZZI }))
                HandleGameObject(ObjectGuid::Empty, true, GetGameObject(DATA_HEXLORD_GATE));
        }

        void SetData(uint32 type, uint32 data) override
        {
            if (type == TYPE_RAND_VENDOR_1)
                RandVendor[0] = data;
            else if (type == TYPE_RAND_VENDOR_2)
                RandVendor[1] = data;
        }

        bool SetBossState(uint32 type, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            switch (type)
            {
                case DATA_NALORAKK:
                    if (state == DONE)
                    {
                        if (uint32 timer = GetPersistentData(DATA_TIMED_RUN))
                        {
                            StorePersistentData(DATA_TIMED_RUN, timer += 15);
                            DoUpdateWorldState(WORLDSTATE_TIME_TO_SACRIFICE, timer);
                        }
                    }
                    break;
                case DATA_AKILZON:
                    if (state == DONE)
                    {
                        if (uint32 timer = GetPersistentData(DATA_TIMED_RUN))
                        {
                            StorePersistentData(DATA_TIMED_RUN, timer += 10);
                            DoUpdateWorldState(WORLDSTATE_TIME_TO_SACRIFICE, timer);
                        }
                    }
                    break;
                case DATA_HEXLORD:
                    if (state == IN_PROGRESS)
                        HandleGameObject(ObjectGuid::Empty, false, GetGameObject(DATA_HEXLORD_GATE));
                    else if (state == NOT_STARTED)
                        CheckInstanceStatus();
                    break;
            }

            if (state == DONE)
            {
                if (GetPersistentData(DATA_TIMED_RUN) && AllBossesDone({ DATA_NALORAKK, DATA_AKILZON, DATA_JANALAI, DATA_HALAZZI }))
                {
                    StorePersistentData(DATA_TIMED_RUN, 0);
                    DoUpdateWorldState(WORLDSTATE_SHOW_TIMER, 0);
                }

                CheckInstanceStatus();
            }

            return true;
        }

        uint32 GetData(uint32 type) const override
        {
            if (type == TYPE_RAND_VENDOR_1)
                return RandVendor[0];
            else if (type == TYPE_RAND_VENDOR_2)
                return RandVendor[1];

            return 0;
        }

        void Update(uint32 diff) override
        {
            scheduler.Update(diff);
        }

        private:
            uint32 RandVendor[RAND_VENDOR];
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_zulaman_InstanceMapScript(map);
    }
};

void AddSC_instance_zulaman()
{
    new instance_zulaman();
}
