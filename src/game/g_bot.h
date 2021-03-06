/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//g_bot.h - declarations of bot specific functions
//#include "g_local.h"

int G_BotBuyUpgrade ( gentity_t *ent, int upgrade );
void G_BotThink(gentity_t *self);
void G_BotModusManager(gentity_t *self);
void G_BotAttack(gentity_t *self, usercmd_t *botCmdBuffer);
void G_BotRepair(gentity_t *self, usercmd_t *botCmdBuffer);
void G_BotHeal(gentity_t *self, usercmd_t *botCmdBuffer);
void G_BotRoam(gentity_t *self, usercmd_t *botCmdBuffer);
void G_BotBuy ( gentity_t *self, usercmd_t *botCmdBuffer);
void G_BotGoto(gentity_t *self, botTarget_t target, usercmd_t *botCmdBuffer);
void G_BotMoveDirectlyToGoal( gentity_t *self, usercmd_t *botCmdBuffer );
void G_BotReactToEnemy(gentity_t *self, usercmd_t *botCmdBuffer);
void G_BotDodge(gentity_t *self, usercmd_t *botCmdBuffer);
int botGetAimEntityNumber(gentity_t *self);
qboolean botTargetInAttackRange(gentity_t *self, botTarget_t target);
void botFireWeapon(gentity_t *self, usercmd_t *botCmdBuffer);
int getTargetEntityNumber(botTarget_t target);
void getTargetPos(botTarget_t target, vec3_t *rVec);
int getTargetTeam( botTarget_t target);
int getTargetType( botTarget_t target);
qboolean targetIsEntity( botTarget_t target);
void setGoalEntity(gentity_t *self, gentity_t *goal );
void setGoalCoordinate(gentity_t *self, vec3_t goal );
void setTargetEntity(botTarget_t *target, gentity_t *goal );
void setTargetCoordinate(botTarget_t *target, vec3_t goal );

int G_BotEvolveToClass( gentity_t *ent, char *classname, usercmd_t *botCmdBuffer);
void G_BotEvolve ( gentity_t *self , usercmd_t *botCmdBuffer);
void botGetAimLocation(gentity_t *self, botTarget_t target, vec3_t *aimLocation);
void botSlowAim( gentity_t *self, vec3_t target, float slow, vec3_t *rVec);
void botShakeAim( gentity_t *self, vec3_t *rVec );
void botAimAtLocation( gentity_t *self, vec3_t target , usercmd_t *rAngles);
int botFindDamagedFriendlyStructure( gentity_t *self );
int botFindClosestEnemy( gentity_t *self, qboolean includeTeam );
int botFindBuilding(gentity_t *self, int buildingType, int range);

int findClosestNode( botTarget_t startTarget);
void findNewNode(gentity_t *self, usercmd_t *botCmdBuffer);
void findNextNode(gentity_t *self);
qboolean haveVisited( gentity_t *self, int id);
void addVisited(gentity_t *self,int id);
void doLastNodeAction(gentity_t *self, usercmd_t *botCmdBuffer);
int distanceToTargetNode(gentity_t *self);
void findRouteToTarget( gentity_t *self, botTarget_t target );
void setNewRoute(gentity_t *self);
void dynamicTrace(trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int skipNum, int mask);

qboolean botTargetInRange( gentity_t *self, botTarget_t target, int mask );
qboolean G_BotCheckForSpawningPlayers( gentity_t *self );
qboolean botWeaponHasLowAmmo(gentity_t *self);
void setSkill(gentity_t *self, int skill);
int getStrafeDirection(gentity_t *self);
qboolean botPathIsBlocked(gentity_t *self);
qboolean botShouldJump(gentity_t *self);
qboolean botNeedsItem(gentity_t *self);
qboolean botCanShop(gentity_t *self);
qboolean botStructureIsDamaged(int team);
qboolean buildableIsDamaged(gentity_t *building);
int G_BotBuyWeapon(gentity_t *ent, int weapon);
float calcPounceAimDelta(gentity_t *self, botTarget_t target);
float calcBarbAimDelta(gentity_t *self, botTarget_t target);
qboolean botOnLadder( gentity_t *self );
qboolean botNeedsToHeal(gentity_t *self);
qboolean goalIsEnemy(gentity_t *self);
qboolean goalIsMedistat(gentity_t *self);
qboolean goalIsArmoury(gentity_t *self);
qboolean goalIsDamagedBuildingOnTeam(gentity_t *self);
void requestNewGoal(gentity_t *self);
int getEntityTeam(gentity_t *ent);
//configureable constants
//For a reference of how far a number represents, take a look at tremulous.h

//how far the bots can be from a medistat to walk to it to heal, if they are too far away, they will not go to it
#define BOT_MEDI_RANGE 1500.0f

//how far the bots can be from an armoury to walk to it to buy stuff, if they are too far, they will not go to the arm
#define BOT_ARM_RANGE 1500.0f

//when closer to the enemy than this range, the human bots will backup
#define BOT_BACKUP_RANGE 300.0f

//How long in milliseconds the bots will chase an enemy if he goes out of their sight (humans) or radar (aliens)
#define BOT_ENEMY_CHASETIME 5000

//How long in milliseconds the bots will chase a friend if he goes out of their sight (humans) or radar (aliens)
#define BOT_FRIEND_CHASETIME 5000

//How often in milliseconds, we will search for a new (closer) enemy this needs to be kept <= 10000 for now
#define BOT_ENEMYSEARCH_INTERVAL 500

//at what hp do we use medkit?
#define BOT_USEMEDKIT_HP 50

//when human bots reach this ammo percentage left or less(and no enemy), they will head back to the base to refuel ammo when in range of arm as defined by BOT_ARM_RANGE
#define BOT_LOW_AMMO 0.50f

//when human bots reach this health or below (and no medkit/enemy) they will head back to the base to heal when in range of medi as defined by BOT_MEDI_RANGE
#define BOT_LOW_HP 100

//When the bot gets closer than these distances to the enemy (and they can see said enemy), they will stop following the route and attack them by aiming at the target and going forward/dodging
#define DRETCH_RANGE 300
#define BASI_RANGE 300
#define MARA_RANGE 400
#define DRAGOON_RANGE 500

//AKA: the tyrant's claw range
#define TYRANT_RANGE 128

//TODO: implement the rest of these, currently they do nothing :)

//when the bots get closer than this distance to an enemy Egg/Node, they will head toward it (killing stuff in the way if need be) instead of roaming randomly
//Line of sight to the egg/node does not matter
#define ROUTETO_SPAWN_RANGE 2000.0f 





//If the "The base is under attack" message appears, the bots will head back to the base to defend it, if they are at least this close to the base (and no enemy in range).
#define H_ROUTETO_BASE_RANGE 5000.0f

//When the "The Overmind is under Attack!" message appears, the alien bots will head back to the base to defend it, if they are at least this close to it.
#define A_ROUTETO_BASE_RANGE 5000.0f


