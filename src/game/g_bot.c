/*
   ===========================================================================
   Copyright (C) 2007 Amine Haddad

   This file is part of Tremulous.

   The original works of vcxzet (lamebot3) were used a guide to create TremBot.
   
   The works of Amine (TremBot) and Sex (PBot) where used/modified to create COW Bot

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

#include "g_local.h"
#include "g_bot.h"

#ifndef RAND_MAX
#define RAND_MAX 32768
#endif

//uncomment to enable debuging messages
//#define BOT_DEBUG 1

botMemory_t g_botMind[MAX_CLIENTS];

void G_BotAdd( char *name, int team, int skill ) {
    int i;
    int clientNum;
    char userinfo[MAX_INFO_STRING];
    int reservedSlots = 0;
    gentity_t *bot;
    reservedSlots = trap_Cvar_VariableIntegerValue( "sv_privateclients" );

    // find what clientNum to use for bot
    clientNum = -1;
    for( i = 0; i < reservedSlots; i++ ) {
        if( !g_entities[i].inuse ) {
            clientNum = i;
            break;
        }
    }

    if(clientNum < 0) {
        trap_Printf("no more slots for bot\n");
        return;
    }
    bot = &g_entities[ clientNum ];
    bot->r.svFlags |= SVF_BOT;
    bot->inuse = qtrue;


    //default bot data
    bot->botMind = &g_botMind[clientNum];
    bot->botMind->enemyLastSeen = 0;
    bot->botMind->botTeam = team;
    bot->botMind->command = BOT_AUTO;
    bot->botMind->spawnItem = WP_HBUILD;
    bot->botMind->followingRoute = qfalse;
    bot->botMind->needsNewGoal = qtrue;
    setSkill(bot, skill);

    

    // register user information
    userinfo[0] = '\0';
    Info_SetValueForKey( userinfo, "name", name );
    Info_SetValueForKey( userinfo, "rate", "25000" );
    Info_SetValueForKey( userinfo, "snaps", "20" );
    
    //so we can connect if server is password protected
    if(g_needpass.integer == 1)
      Info_SetValueForKey( userinfo, "password", g_password.string);
    
    trap_SetUserinfo( clientNum, userinfo );

    // have it connect to the game as a normal client
    if(ClientConnect(clientNum, qtrue) != NULL )
        // won't let us join
        return;

    ClientBegin( clientNum );
    G_ChangeTeam( bot, team );
}

void G_BotDel( int clientNum ) {
    gentity_t *bot;

    bot = &g_entities[clientNum];
    if( !( bot->r.svFlags & SVF_BOT ) ) {
        trap_Printf( va("'^7%s^7' is not a bot\n", bot->client->pers.netname) );
        return;
    }

    ClientDisconnect(clientNum);
    //trap_BotFreeClient(clientNum);
}

void G_BotCmd( gentity_t *master, int clientNum, char *command) {
    gentity_t *bot;

    bot = &g_entities[clientNum];
    if( !( bot->r.svFlags & SVF_BOT ) )
        return;

    if( !Q_stricmp( command, "auto" ) )
        bot->botMind->command = BOT_AUTO;
    else if( !Q_stricmp( command, "attack" ) )
        bot->botMind->command = BOT_ATTACK;
    else if( !Q_stricmp( command, "idle" ) )
        bot->botMind->command = BOT_IDLE;
    else if( !Q_stricmp( command, "repair" ) )
        bot->botMind->command = BOT_REPAIR;
    else if( !Q_stricmp( command, "spawnrifle" ) )
        bot->botMind->spawnItem = WP_MACHINEGUN;
    else if( !Q_stricmp( command, "spawnckit" ) )
        bot->botMind->spawnItem = WP_HBUILD;
    else {
        bot->botMind->command = BOT_AUTO;
    }
    return;
}

qboolean botShouldJump(gentity_t *self) {
    float           jumpSpeed, jumpHeight, gravity;
    int                     selfClass = self->client->ps.stats[ STAT_PCLASS ];
    int stepSize = 18;
    vec3_t          mins, maxs, start;
    vec3_t          forward,right;
    vec3_t startLeft, startRight, endLeft, endRight;
    trace_t         traceRightLow,traceLeftLow,traceRightHigh,traceLeftHigh;
    vec3_t halfMins,halfMaxs;
    
    if(self->s.groundEntityNum == ENTITYNUM_NONE)
        return qfalse;
    
    //getting max jump height
    jumpSpeed = BG_FindJumpMagnitudeForClass( selfClass );
    gravity = self->client->ps.gravity;
    jumpHeight = (jumpSpeed*jumpSpeed)/(gravity*2);
    
    BG_FindBBoxForClass( selfClass, mins, maxs, NULL, NULL, NULL );
    AngleVectors( self->client->ps.viewangles, forward, right, NULL);
    forward[2] = 0.0f;
    VectorNormalize(forward);
    
    VectorMA(self->s.origin, maxs[0], forward, start);
    start[2] += stepSize;
    VectorScale(right, maxs[1]/2,right);
    VectorAdd(start, right, startRight);
    VectorSubtract(start, right, startLeft);
    VectorSet(halfMins,mins[0],mins[1]/2,mins[2]);
    VectorSet(halfMaxs,maxs[0],maxs[1]/2,maxs[2]);
    VectorMA(startRight, 30.0f, forward, endRight);
    VectorMA(startLeft, 30.0f, forward, endLeft);
    
    //trace half the bbox from the left and right sides, before and after jump, to see if jumping will help us get over obstacle
    trap_Trace(&traceRightLow, startRight, halfMins,halfMaxs, endRight, self->s.number, MASK_SHOT);
    trap_Trace(&traceLeftLow, startLeft, halfMins, halfMaxs, endLeft, self->s.number, MASK_SHOT);
    
    startLeft[2] += jumpHeight;
    startRight[2] += jumpHeight;
    endLeft[2] += jumpHeight;
    endRight[2] += jumpHeight;
    trap_Trace( &traceLeftHigh, startLeft, halfMins, halfMaxs, endLeft, self->s.number, MASK_SHOT );
    trap_Trace( &traceRightHigh,startRight,halfMins, halfMaxs, endRight, self->s.number, MASK_SHOT);
    
    if(traceRightLow.fraction < traceRightHigh.fraction || traceLeftLow.fraction < traceLeftHigh.fraction)
        return qtrue;
    else
        return qfalse;
}
int getStrafeDirection(gentity_t *self) {
    
    trace_t traceRight,traceLeft;
    vec3_t traceHeight;
    vec3_t startRight,startLeft;
    vec3_t mins,maxs;
    vec3_t forward,right;
    vec3_t endRight,endLeft;
    
    int strafe = 0;
    BG_FindBBoxForClass( self->client->ps.stats[STAT_PCLASS], mins, maxs, NULL, NULL, NULL);
    AngleVectors( self->client->ps.viewangles,forward , right, NULL);
    
    
    VectorScale(right, maxs[1], right);
    
    VectorAdd( self->s.origin, right, startRight );
    VectorSubtract( self->s.origin, right, startLeft );
    
    forward[2] = 0.0f;
    VectorNormalize(forward);
    VectorMA(startRight, maxs[0] +30, forward, endRight);
    VectorMA(startLeft, maxs[0] + 30, forward, endLeft);
    
    startRight[2] += mins[2];
    startLeft[2] += mins[2];
    
    VectorSet(traceHeight, 0.0f, 1.0f, (maxs[2] - mins[2]));
    
    trap_Trace( &traceRight, startRight, NULL, traceHeight, endRight, self->s.number, MASK_SHOT);
    trap_Trace( &traceLeft, startLeft, NULL, traceHeight, endLeft, self->s.number, MASK_SHOT);
    
    if( traceRight.fraction == 1.0f && traceLeft.fraction != 1.0f ) {
        strafe = 127;
    } else if( traceRight.fraction != 1.0f && traceLeft.fraction == 1.0f ) {
        strafe = -127;
    } else {
        if(self->client->time10000 % 5000 > 2500)
            strafe = 127;
        else
            strafe = -127;
    }
    return strafe;
}
qboolean botPathIsBlocked(gentity_t *self) {
    vec3_t forward,forwardDerived1, forwardDerived2,right,up;
    vec3_t start, end;
    vec3_t mins, maxs;
    trace_t trace;
    gentity_t *traceEnt;
    int blockerTeam;
    
    if( !self->client )
        return qfalse;
    
    BG_FindBBoxForClass( self->client->ps.stats[ STAT_PCLASS ], mins, maxs, NULL, NULL, NULL );
    
    
    AngleVectors( self->client->ps.viewangles, forward, right, NULL);
    VectorCopy(self->client->ps.origin, end);
    end[2] += mins[2] - 20;
        trap_Trace(&trace, self->client->ps.origin, NULL, NULL, end, self->s.number,MASK_SHOT);
        //forward vector is the direction we are aiming, NOT the direction we are moving, so derive it from the right vector and the normal of the plane we are on
        VectorCopy(trace.plane.normal, up);
        
        //cross product can return 2 different vectors depending on the order of the arguments
        //these vectors are simply pointing the opposite directions, so we just need to invert one of them to get the second one
        CrossProduct(up, right, forwardDerived1);
        VectorNormalize(forwardDerived1);
        VectorCopy(forwardDerived1,forwardDerived2);
        VectorInverse(forwardDerived2);
        //choose the one closest to the forward vector according to our view
        if(Q_fabs(acos(DotProduct(forward,forwardDerived1))) < Q_fabs(acos(DotProduct(forward, forwardDerived2))))
            VectorCopy(forwardDerived1, forward);
        else
            VectorCopy(forwardDerived2, forward);
    //scaling the vector
    VectorMA( self->client->ps.origin, maxs[0], forward, start );
    VectorMA(start, 30, forward,end);
    
    trap_Trace( &trace, self->client->ps.origin, mins, maxs, end, self->s.number, MASK_SHOT );
    traceEnt = &g_entities[trace.entityNum];
    if(traceEnt) {
        if(traceEnt->s.eType == ET_BUILDABLE)
            blockerTeam = traceEnt->biteam;
        else if(traceEnt->client)
            blockerTeam = traceEnt->client->ps.stats[STAT_PTEAM];
        else
            blockerTeam = self->client->ps.stats[STAT_PTEAM]; //we are colliding with world or something weird... so make us return true
    } else {
        blockerTeam = PTE_NONE;
    }
    if( trace.fraction == 1.0f || (blockerTeam != PTE_ALIENS && self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS))
            return qfalse;
    else
        return qtrue;
}
//copy of PM_CheckLadder in bg_pmove.c
qboolean botOnLadder( gentity_t *self )
{
    vec3_t forward, end;
    vec3_t mins, maxs;
    trace_t trace;
    
    if( !BG_ClassHasAbility( self->client->ps.stats[ STAT_PCLASS ], SCA_CANUSELADDERS ) )
        return qfalse;
    
    AngleVectors( self->client->ps.viewangles, forward, NULL, NULL);
    
    forward[ 2 ] = 0.0f;
    BG_FindBBoxForClass( self->client->ps.stats[ STAT_PCLASS ], mins, maxs, NULL, NULL, NULL );
    VectorMA( self->s.origin, 1.0f, forward, end );
    
    trap_Trace( &trace, self->s.origin, mins, maxs, end, self->s.number, MASK_PLAYERSOLID );
    
    if( trace.fraction < 1.0f && trace.surfaceFlags & SURF_LADDER )
        return qtrue;
    else
        return qfalse;
}
/**G_BotThink
 * Does Misc bot actions
 * Calls G_BotModusManager to decide which Modus the bot should be in
 * Executes the different bot functions based on the bot's mode
*/
void G_BotThink( gentity_t *self) {
    usercmd_t  botCmdBuffer = self->client->pers.cmd;
    botCmdBuffer.buttons = 0;
    botCmdBuffer.forwardmove = 0;
    botCmdBuffer.rightmove = 0;
    
    //use medkit when hp is low
    if(self->health < BOT_USEMEDKIT_HP && BG_InventoryContainsUpgrade(UP_MEDKIT,self->client->ps.stats))
        BG_ActivateUpgrade(UP_MEDKIT,self->client->ps.stats);
    
    //try to evolve every so often (aliens only)
    if(g_bot_evolve.integer > 0 && self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS && self->client->ps.persistant[PERS_CREDIT] > 0)
        G_BotEvolve(self,&botCmdBuffer);
    
    //infinite funds cvar
    if(g_bot_infinite_funds.integer == 1)
        G_AddCreditToClient(self->client, HUMAN_MAX_CREDITS, qtrue);
    //hacky ping fix
    self->client->ps.ping = rand() % 50 + 50;
    
    G_BotModusManager(self);
    switch(self->botMind->currentModus) {
        case ATTACK:
            G_BotAttack(self, &botCmdBuffer);
            break;
        case BUILD:
           // G_BotBuild(self, &botCmdBuffer);
           break;
        case BUY:
            G_BotBuy(self, &botCmdBuffer);
            break;
        case HEAL:
            G_BotHeal(self, &botCmdBuffer);
            break;
        case REPAIR:
            G_BotRepair(self, &botCmdBuffer);
            break;
        case ROAM:
            G_BotRoam(self, &botCmdBuffer);
            break;
        case IDLE:
            break;
    }
    self->client->pers.cmd =botCmdBuffer;
}
/**G_BotModusManager
 * Changes the bot's current Modus based on the surrounding conditions
 * Decides when a bot will Build,Attack,Roam,etc
 */
void G_BotModusManager( gentity_t *self ) {
    
    int enemyIndex = ENTITYNUM_NONE;
    
    int damagedBuildingIndex = botFindDamagedFriendlyStructure(self);
    int medistatIndex = botFindBuilding(self, BA_H_MEDISTAT, BOT_MEDI_RANGE);
    int armouryIndex = botFindBuilding(self, BA_H_ARMOURY, BOT_ARM_RANGE);
    
    //search for a new enemy every so often
   if(self->client->time10000 % BOT_ENEMYSEARCH_INTERVAL == 0)
        enemyIndex = botFindClosestEnemy(self, qfalse);
   
   //we dont want bots to go out of these modus when an enemy appears unless said enemy is closer than goal position (+300)
    if(self->botMind->currentModus == BUY || self->botMind->currentModus == REPAIR || self->botMind->currentModus == HEAL) {
        if(!self->botMind->needsNewGoal && DistanceSquared(self->s.origin, g_entities[enemyIndex].s.origin) > DistanceSquared(self->s.origin, self->botMind->goal.ent->s.origin) + Square(300) && enemyIndex != ENTITYNUM_NONE)
            return;
    }
    switch(self->botMind->command) {
        case BOT_AUTO:
            if(enemyIndex != ENTITYNUM_NONE || goalIsEnemy(self)) {
                self->botMind->currentModus = ATTACK;
                if(!goalIsEnemy(self)) {
                    setGoalEntity(self, &g_entities[enemyIndex]);
                    self->botMind->enemyLastSeen = level.time;
                    self->botMind->needsNewGoal = qfalse;
                }
            } else if((damagedBuildingIndex != ENTITYNUM_NONE || goalIsDamagedBuildingOnTeam(self)) && BG_InventoryContainsWeapon(WP_HBUILD,self->client->ps.stats)) {
                self->botMind->currentModus = REPAIR;
                if(!goalIsDamagedBuildingOnTeam(self)) {
                    setGoalEntity(self, &g_entities[damagedBuildingIndex]);
                    self->botMind->needsNewGoal = qfalse;
                }
            } else if((medistatIndex != ENTITYNUM_NONE || goalIsMedistat(self)) &&  botNeedsToHeal(self) && getEntityTeam(self) == PTE_HUMANS) {
                self->botMind->currentModus = HEAL;
                if(!goalIsMedistat(self)) {
                    setGoalEntity(self, &g_entities[medistatIndex]);
                    self->botMind->needsNewGoal = qfalse;
                }
            } else if((armouryIndex != ENTITYNUM_NONE || goalIsArmoury(self)) && botNeedsItem(self) && g_bot_buy.integer > 0 && getEntityTeam(self) == PTE_HUMANS) {
                self->botMind->currentModus = BUY;
                if(!goalIsArmoury(self)) {
                    setGoalEntity(self, &g_entities[armouryIndex]);
                    self->botMind->needsNewGoal = qfalse;
                }
            } else if(g_bot_roam.integer > 0){
                if(targetIsEntity(self->botMind->goal) || self->botMind->needsNewGoal ) {
                    setGoalCoordinate(self, level.nodes[rand() % level.numNodes].coord);
                    self->botMind->needsNewGoal = qfalse;
                }
                self->botMind->currentModus = ROAM;
            } else {
                self->botMind->currentModus = IDLE;
            }
            break;
        case BOT_ATTACK:
            if(enemyIndex != ENTITYNUM_NONE || goalIsEnemy(self)) {
                self->botMind->currentModus = ATTACK;
                if(!goalIsEnemy(self)) {
                    setGoalEntity(self, &g_entities[enemyIndex]);
                    self->botMind->enemyLastSeen = level.time;
                    self->botMind->needsNewGoal = qfalse;
                }
            } else if((medistatIndex != ENTITYNUM_NONE || goalIsMedistat(self)) &&  botNeedsToHeal(self) 
                && getEntityTeam(self) == PTE_HUMANS) {
                self->botMind->currentModus = HEAL;
                if(!goalIsMedistat(self)) {
                    setGoalEntity(self, &g_entities[medistatIndex]);
                    self->botMind->needsNewGoal = qfalse;
                }
            } else if((armouryIndex != ENTITYNUM_NONE || goalIsArmoury(self)) && botNeedsItem(self) && g_bot_buy.integer > 0 && getEntityTeam(self) == PTE_HUMANS) {
                self->botMind->currentModus = BUY;
                if(!goalIsArmoury(self)) {
                    setGoalEntity(self, &g_entities[armouryIndex]);
                    self->botMind->needsNewGoal = qfalse;
                }
            } else if(g_bot_roam.integer > 0){
                if(targetIsEntity(self->botMind->goal) || self->botMind->needsNewGoal ) {
                    setGoalCoordinate(self, level.nodes[rand() % level.numNodes].coord);
                    self->botMind->needsNewGoal = qfalse;
                }
                self->botMind->currentModus = ROAM;
            } else {
                self->botMind->currentModus = IDLE;
            }
            break;
        case BOT_REPAIR:
            if((damagedBuildingIndex != ENTITYNUM_NONE || goalIsDamagedBuildingOnTeam(self)) && BG_InventoryContainsWeapon(WP_HBUILD,self->client->ps.stats)) {
                self->botMind->currentModus = REPAIR;
                if(!goalIsDamagedBuildingOnTeam(self)) {
                    setGoalEntity(self, &g_entities[damagedBuildingIndex]);
                    self->botMind->needsNewGoal = qfalse;
                }
            } else {
                self->botMind->currentModus = IDLE;
            }
            break;
        case BOT_IDLE:
            self->botMind->currentModus = IDLE;
            break;
    }

        
}
qboolean botNeedsToHeal(gentity_t *self) {
    return self->health < BOT_LOW_HP && !BG_InventoryContainsUpgrade(UP_MEDKIT, self->client->ps.stats);
}
qboolean goalIsEnemy(gentity_t *self) {
    return targetIsEntity(self->botMind->goal) && getTargetTeam(self->botMind->goal) != getEntityTeam(self) && getTargetTeam(self->botMind->goal) != PTE_NONE && !self->botMind->needsNewGoal;
}
qboolean goalIsMedistat(gentity_t *self) {
    return targetIsEntity(self->botMind->goal) && getTargetType(self->botMind->goal) == ET_BUILDABLE 
    && BG_FindBuildNumForEntityName(self->botMind->goal.ent->classname) == BA_H_MEDISTAT && !self->botMind->needsNewGoal;
}
qboolean goalIsArmoury(gentity_t *self) {
    return targetIsEntity(self->botMind->goal) && getTargetType(self->botMind->goal) == ET_BUILDABLE 
    && BG_FindBuildNumForEntityName(self->botMind->goal.ent->classname) == BA_H_ARMOURY && !self->botMind->needsNewGoal;
}
qboolean goalIsDamagedBuildingOnTeam(gentity_t *self) {
    return targetIsEntity(self->botMind->goal) && getTargetType(self->botMind->goal) == ET_BUILDABLE 
    && buildableIsDamaged(self->botMind->goal.ent) && getTargetTeam(self->botMind->goal) == getEntityTeam(self) && !self->botMind->needsNewGoal;
}
void requestNewGoal(gentity_t *self) {
    self->botMind->needsNewGoal = qtrue;
}
/**G_BotMoveDirectlyToGoal
*Used whenever we need to go directly to the goal
*Uses the route to get to the closest node to the goal
*Then goes from the last node to the goal if the goal is seen
*If the goal is not seen, then it computes a new route to the goal and follows it
*/
void G_BotMoveDirectlyToGoal( gentity_t *self, usercmd_t *botCmdBuffer ) {
    
    vec3_t targetPos;
    getTargetPos(self->botMind->goal, &targetPos);
    if(self->botMind->followingRoute && self->botMind->targetNodeID != -1) {
        setTargetCoordinate(&self->botMind->targetNode, level.nodes[self->botMind->targetNodeID].coord);
        G_BotGoto( self, self->botMind->targetNode, botCmdBuffer );
        doLastNodeAction(self, botCmdBuffer);
        
        //apparently, we are stuck, so find a new route to the goal and use that
        if(level.time - self->botMind->timeFoundNode > level.nodes[self->botMind->lastNodeID].timeout)
        {
            findRouteToTarget(self, self->botMind->goal);
            setNewRoute(self);
        }
        
        else if( level.time - self->botMind->timeFoundNode > 10000 )
        {
            findRouteToTarget(self, self->botMind->goal);
            setNewRoute(self);
        }
        if(distanceToTargetNode(self) < 70)
        {
            self->botMind->lastNodeID = self->botMind->targetNodeID;
            self->botMind->targetNodeID = self->botMind->routeToTarget[self->botMind->targetNodeID];
            self->botMind->timeFoundNode = level.time;
            
        }
        //our route has ended or we have been told to quit following it 
        //can we see the goal? Then go directly to it. Else find a new route and folow it
    } else if(botTargetInRange(self, self->botMind->goal, MASK_DEADSOLID)){
        G_BotGoto(self,self->botMind->goal,botCmdBuffer);
    } else {
        findRouteToTarget(self, self->botMind->goal);
        setNewRoute(self);
    }
    
}



void G_BotSearchForGoal(gentity_t *self, usercmd_t *botCmdBuffer) {
    vec3_t targetPos;
    vec3_t playerPos;
    VectorCopy(self->s.origin,playerPos);
    playerPos[2] += self->r.mins[2];
    getTargetPos(self->botMind->goal,&targetPos);
    //we have arrived at the goal so request a new one
    if(DistanceSquared(playerPos, targetPos) < Square(70)) {
        requestNewGoal(self);
        return;
    }
    //Go to the goal
    G_BotMoveDirectlyToGoal(self, botCmdBuffer);
}

/**G_BotGoto
 * Used to make the bot travel between waypoints or to the target from the last waypoint
 * Also can be used to make the bot travel other short distances
*/
void G_BotGoto(gentity_t *self, botTarget_t target, usercmd_t *botCmdBuffer) {
    
    vec3_t tmpVec;
    vec3_t forward;
    //aim at the destination
    botGetAimLocation(self, target, &tmpVec);
    
    //aim faster than skill if targeting node so we dont fall off path
    //also aim fast when trying to get to arm to buy, medi to heal, or building to repair, so we dont end up circling the target
    if(!targetIsEntity(target) || self->botMind->currentModus == REPAIR || self->botMind->currentModus == BUY || self->botMind->currentModus == HEAL)
        botSlowAim(self, tmpVec, 0.5f, &tmpVec);
    else
        botSlowAim(self, tmpVec, self->botMind->botSkill.aimSlowness, &tmpVec);
    
    if(getTargetType(target) != ET_BUILDABLE && targetIsEntity(target)) {
        botShakeAim(self, &tmpVec);
    }
    
        
    botAimAtLocation(self, tmpVec, botCmdBuffer);
    
    //humans should not move if they are targetting, and can hit, a building
    if(botTargetInAttackRange(self, target) && getTargetType(target) == ET_BUILDABLE && self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS && getTargetTeam(target) == PTE_ALIENS)
        return;
    
    //move forward
    botCmdBuffer->forwardmove = 127;
    
    
    //dodge if going toward enemy
    if(self->client->ps.stats[STAT_PTEAM] != getTargetTeam(target) && getTargetTeam(target) != PTE_NONE) {
        G_BotDodge(self, botCmdBuffer);
        //sprint 
        if(self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS)
            self->client->ps.stats[STAT_STATE] |= SS_SPEEDBOOST;
    }
    
    //this is here so we dont run out of stamina..
    //basically, just me being too lazy to make bots stop and regain stamina
    self->client->ps.stats[ STAT_STAMINA ] = MAX_STAMINA;
    
    //we have stopped moving forward, try to get around whatever is blocking us
    if( botPathIsBlocked(self)) {
        //handle ladders
        if(botOnLadder(self)) {
            AngleVectors(self->client->ps.viewangles, forward, NULL, NULL);
            forward[2] = 30;
            //aim at ladder (well, just a bit above)
            VectorCopy(self->s.origin, tmpVec);
            tmpVec[2] += self->client->ps.viewheight;
            VectorAdd(tmpVec,forward, tmpVec);
            botAimAtLocation(self, tmpVec, botCmdBuffer);
        } else {
            if(botShouldJump(self)) {
                botCmdBuffer->upmove = 127;
            } else {
                botCmdBuffer->rightmove = getStrafeDirection(self);
            }
        }
            
    }
    
    //need to periodically reset upmove to 0 for jump to work
    if( self->client->time10000 % 1000)
        botCmdBuffer->upmove = 0;
    
    // enable wallwalk for classes that can wallwalk
    if( BG_ClassHasAbility( self->client->ps.stats[STAT_PCLASS], SCA_WALLCLIMBER ) )
        botCmdBuffer->upmove = -1;
    
    //stay away from enemy as human
        getTargetPos(target, &tmpVec);
        if(self->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS && 
        DistanceSquared(self->s.pos.trBase,tmpVec) < Square(300) && DistanceSquared(self->s.pos.trBase,tmpVec) > Square(100) && botTargetInAttackRange(self, target) && self->s.weapon != WP_PAIN_SAW
        && getTargetTeam(target) == PTE_ALIENS)
        {
            botCmdBuffer->forwardmove = -100;
        }
}
/**
 * G_BotAttack
 * The AI for attacking an enemy
 * Is called in G_BotThink
 * Decided when to be called in G_BotModusManager
 */
void G_BotAttack(gentity_t *self, usercmd_t *botCmdBuffer) {
    botTarget_t proposedTarget;
    setTargetEntity(&proposedTarget, &g_entities[botFindClosestEnemy(self, qfalse)]);
    //enemy has died so signal that the goal is unusable
    if(g_entities[getTargetEntityNumber(self->botMind->goal)].health <= 0) {
        requestNewGoal(self);
        return;
    }
    //if we cant see our current target, but we can see the closest Enemy, we should stop attacking our current target and attack said closest enemy
    if(!botTargetInRange(self, self->botMind->goal, MASK_SHOT) && botTargetInRange(self,proposedTarget, MASK_SHOT)) {
        requestNewGoal(self);
        return;
        //we havent "seen" the current enemy for a period of time, so signal that the goal is unusuable
    } else if(level.time - self->botMind->enemyLastSeen > BOT_ENEMY_CHASETIME) {
        requestNewGoal(self);
        return;
    }
    
    //switch to blaster
    if((BG_WeaponIsEmpty(self->client->ps.weapon, self->client->ps.ammo, self->client->ps.powerups)
        || self->client->ps.weapon == WP_HBUILD) && self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS)
        G_ForceWeaponChange( self, WP_BLASTER );
    
    //aliens have radar so they will always 'see' the enemy if they are in radar range
    if(self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS) {
        self->botMind->enemyLastSeen = level.time;
    }
    
    
    if(botTargetInAttackRange(self, self->botMind->goal)) {
        
        self->botMind->followingRoute = qfalse;
        G_BotMoveDirectlyToGoal(self, botCmdBuffer);
        botFireWeapon(self, botCmdBuffer);
        self->botMind->enemyLastSeen = level.time;
    } else if(botTargetInRange(self, self->botMind->goal, MASK_SHOT)) {
        G_BotMoveDirectlyToGoal(self, botCmdBuffer);
        G_BotReactToEnemy(self, botCmdBuffer);
        self->botMind->enemyLastSeen = level.time;
    } else if(!self->botMind->followingRoute || self->botMind->targetNodeID == -1){
        findRouteToTarget(self, self->botMind->goal);
        setNewRoute(self);
        G_BotMoveDirectlyToGoal(self, botCmdBuffer);
    } else {
        G_BotMoveDirectlyToGoal(self, botCmdBuffer);
    }
    
    
}
/**
 * G_BotRepair
 * The AI for repairing a structure
 * Is called in G_BotThink
 * Decided when to be called in G_BotModusManager
 */
void G_BotRepair(gentity_t *self, usercmd_t *botCmdBuffer) {
    //the target building has died so signal that the goal is unusable
    if(g_entities[getTargetEntityNumber(self->botMind->goal)].health <= 0) {
        requestNewGoal(self);
        return;
    }
    if(self->client->ps.weapon != WP_HBUILD)
        G_ForceWeaponChange( self, WP_HBUILD );
    if(botTargetInAttackRange(self, self->botMind->goal) && botGetAimEntityNumber(self) == getTargetEntityNumber(self->botMind->goal) ) {
        self->botMind->followingRoute = qfalse;
        botFireWeapon( self, botCmdBuffer );
    } else
        G_BotMoveDirectlyToGoal(self, botCmdBuffer);
}
/**
 * G_BotHeal
 * The AI for making the bot go to a medistation to heal
 * Is called in G_BotThink
 * Decided when to be called in G_BotModusManager
 */
void G_BotHeal(gentity_t *self, usercmd_t *botCmdBuffer) {
    vec3_t targetPos;
    //the medi has died so signal that the goal is unusable
    if(g_entities[getTargetEntityNumber(self->botMind->goal)].health <= 0) {
        requestNewGoal(self);
        return;
    }
    //this medi is no longer powered so signal that the goal is unusable
    if(!g_entities[getTargetEntityNumber(self->botMind->goal)].powered) {
        requestNewGoal(self);
        return;
    }
    getTargetPos(self->botMind->goal, &targetPos);
    if(DistanceSquared(self->s.origin, targetPos) > Square(50))
        G_BotMoveDirectlyToGoal(self, botCmdBuffer);
    
}
/**
 * G_BotBuy
 * The AI for making the bot go to an armoury and buying something
 * Is called in G_BotThink
 * Decided when to be called in G_BotModusManager
 */
void G_BotBuy(gentity_t *self, usercmd_t *botCmdBuffer) {
    vec3_t targetPos;
    int i;
    //the armoury has died so signal that the goal is unuseable
    if(g_entities[getTargetEntityNumber(self->botMind->goal)].health <= 0) {
        requestNewGoal(self);
        return;
    }
    //this armoury is no longer powered, so signal that the goal is unusable
    if(!g_entities[getTargetEntityNumber(self->botMind->goal)].powered) {
        requestNewGoal(self);
        return;
    }
    getTargetPos(self->botMind->goal, &targetPos);
    if(DistanceSquared(self->s.pos.trBase, targetPos) > Square(100)) 
        G_BotMoveDirectlyToGoal(self, botCmdBuffer);
    else if( self->client->time10000 % 1000 == 0){
        // sell current weapon
        for( i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++ )
        {
            if( BG_InventoryContainsWeapon( i, self->client->ps.stats ) &&
                BG_FindPurchasableForWeapon( i ) )
            {
                BG_RemoveWeaponFromInventory( i, self->client->ps.stats );
                
                //add to funds
                G_AddCreditToClient( self->client, (short)BG_FindPriceForWeapon( i ), qfalse );
            }
            
            //if we have this weapon selected, force a new selection
            if( i == self->client->ps.weapon )
                G_ForceWeaponChange( self, WP_NONE );
        }
        //try to buy helmet/lightarmour
        G_BotBuyUpgrade( self, UP_HELMET);
        G_BotBuyUpgrade( self, UP_LIGHTARMOUR);
        
        // buy most expensive first, then one cheaper, etc, dirty but working way
        if( !G_BotBuyWeapon( self, WP_LUCIFER_CANNON ) )
            if( !G_BotBuyWeapon( self, WP_FLAMER ) )
                if( !G_BotBuyWeapon( self, WP_PULSE_RIFLE ) )
                    if( !G_BotBuyWeapon( self, WP_CHAINGUN ) )
                        if( !G_BotBuyWeapon( self, WP_MASS_DRIVER ) )
                            if( !G_BotBuyWeapon( self, WP_LAS_GUN ) )
                                if( !G_BotBuyWeapon( self, WP_SHOTGUN ) )
                                    if( !G_BotBuyWeapon( self, WP_PAIN_SAW ) )
                                        G_BotBuyWeapon( self, WP_MACHINEGUN );
                                    
        //buy ammo/batpack
        if( BG_FindUsesEnergyForWeapon( self->client->ps.weapon )) {
            G_BotBuyUpgrade( self, UP_BATTPACK );
        }else {
            G_BotBuyUpgrade( self, UP_AMMO );
        }
    }
}
/**
 * G_BotEvolve
 * Used to make an alien evolve
 * Is called in G_BotThink
 */
void G_BotEvolve ( gentity_t *self, usercmd_t *botCmdBuffer )
{
    // very not-clean code, but hea it works and I'm lazy 
    int res;
    if(!G_BotEvolveToClass(self, "level4", botCmdBuffer))
        if(!G_BotEvolveToClass(self, "level3upg", botCmdBuffer)) {
            res = (random()>0.5) ? G_BotEvolveToClass(self, "level3", botCmdBuffer) : G_BotEvolveToClass(self, "level2upg", botCmdBuffer);
            if(!res) {
                res = (random()>0.5) ? G_BotEvolveToClass(self, "level2", botCmdBuffer) : G_BotEvolveToClass(self, "level1upg", botCmdBuffer);
                if(!res)
                    if(!G_BotEvolveToClass(self, "level1", botCmdBuffer))
                        G_BotEvolveToClass(self, "level0", botCmdBuffer);
            }
        }
}
void G_BotRoam(gentity_t *self, usercmd_t *botCmdBuffer) {
    //int buildingIndex = ENTITYNUM_NONE;
    //qboolean teamRush;
    /*if(self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS) {
        buildingIndex = botFindBuilding(self, BA_A_OVERMIND, -1);
        if(buildingIndex == ENTITYNUM_NONE) {
            buildingIndex = botFindBuilding(self, BA_A_SPAWN, -1);
        }
        teamRush = (level.time % 300000 < 150000) ? qtrue : qfalse;
    } else {
        buildingIndex = botFindBuilding(self, BA_H_REACTOR, -1);
        if(buildingIndex == ENTITYNUM_NONE) {
            buildingIndex = botFindBuilding(self, BA_H_SPAWN, -1);
        }
        teamRush = (level.time % 300000 > 150000) ? qtrue : qfalse;
    }
    if(buildingIndex != ENTITYNUM_NONE && teamRush ) {
        if(buildingIndex != getTargetEntityNumber(self->botMind->goal))
            setGoalEntity(self,&g_entities[buildingIndex]);
        else
            G_BotMoveDirectlyToGoal(self, botCmdBuffer);
    }else {*/
        G_BotSearchForGoal(self, botCmdBuffer); 
   // }
}
/**
 * G_BotReactToEnemy
 * Class specific movement upon seeing an enemy
 * We need this to prevent some alien classes from being an easy target by walking in a straight line
 * ,but also prevent others from being unable to get to the enemy
 * please only use if the bot can SEE his enemy (botTargetinRange == qtrue)
*/
void G_BotReactToEnemy(gentity_t *self, usercmd_t *botCmdBuffer) {
    vec3_t forward,right,up,muzzle, targetPos;
    AngleVectors(self->client->ps.viewangles, forward, right, up);
    CalcMuzzlePoint(self, forward, right, up, muzzle);
    
    switch(self->client->ps.stats[STAT_PCLASS]) {
        case PCL_ALIEN_LEVEL0:
            getTargetPos(self->botMind->goal, &targetPos);
            //stop following the route if close enough
            if(DistanceSquared(self->s.pos.trBase, targetPos) <= Square(DRETCH_RANGE))
                self->botMind->followingRoute = qfalse;
            //dodge
                G_BotDodge(self,botCmdBuffer);
            break;
        case PCL_ALIEN_LEVEL1:
        case PCL_ALIEN_LEVEL1_UPG:
            getTargetPos(self->botMind->goal, &targetPos);
            //stop following the route if close enough
            if(DistanceSquared(self->s.pos.trBase, targetPos) <= Square(BASI_RANGE))
                self->botMind->followingRoute = qfalse;
            //dodge
            G_BotDodge(self,botCmdBuffer);
            break;
        case PCL_ALIEN_LEVEL2:
        case PCL_ALIEN_LEVEL2_UPG:
            getTargetPos(self->botMind->goal,&targetPos);
            if(DistanceSquared(self->s.pos.trBase, targetPos) <= Square(MARA_RANGE))
                self->botMind->followingRoute = qfalse;
            G_BotDodge(self,botCmdBuffer);
            break;
        case PCL_ALIEN_LEVEL3:
        case PCL_ALIEN_LEVEL3_UPG:
            getTargetPos(self->botMind->goal, &targetPos);
            if(DistanceSquared(self->s.pos.trBase, targetPos) <= Square(DRAGOON_RANGE))
                self->botMind->followingRoute = qfalse;
            break;
        case PCL_ALIEN_LEVEL4:
            getTargetPos(self->botMind->goal, &targetPos);
            if(DistanceSquared(self->s.pos.trBase, targetPos) <= Square(TYRANT_RANGE))
                self->botMind->followingRoute = qfalse;
            //use charge to approach more quickly
            if (DistanceSquared( muzzle, targetPos) > Square(LEVEL4_CLAW_RANGE))
                botCmdBuffer->buttons |= BUTTON_ATTACK2;
            break;
        case PCL_HUMAN:
        case PCL_HUMAN_BSUIT:
            if(self->s.weapon == WP_PAIN_SAW) //we ALWAYS want psaw users to fire, otherwise they only fire intermittently
                botFireWeapon(self, botCmdBuffer);
            break;
        default: break;
    }
        
}
/**
 * G_BotDodge
 * Makes the bot dodge :P
 */
void G_BotDodge(gentity_t *self, usercmd_t *botCmdBuffer) {
    if(self->client->time1000 >= 500)
        botCmdBuffer->rightmove = 127;
    else
        botCmdBuffer->rightmove = -127;
    
    if((self->client->time10000 % 2000) < 1000)
        botCmdBuffer->rightmove *= -1;
    
    if((self->client->time1000 % 300) >= 100 && (self->client->time10000 % 3000) > 2000)
        botCmdBuffer->rightmove = 0;
}
/**
 * botGetAimEntityNumber
 * Returns the entity number of the entity that the bot is currently aiming at
 */
//NOTE: This is just used for telling bot repairers if they will repair the buildable when they fire. Thus, this may need updating if the code for ckit repair changes in g_weapon.c
int botGetAimEntityNumber(gentity_t *self) {
    vec3_t forward;
    vec3_t end;
    trace_t trace;
    AngleVectors( self->client->ps.viewangles, forward, NULL,NULL);
    
    
    VectorMA(self->client->ps.origin, 4092, forward, end);
    
    trap_Trace(&trace, self->client->ps.origin, NULL, NULL, end, self->s.number, MASK_SHOT);
    return trace.entityNum;
}
/**botTargetInAttackRange
 * Tells if the bot is in range of attacking a target
 */
qboolean botTargetInAttackRange(gentity_t *self, botTarget_t target) {
    float range,secondaryRange;
    vec3_t forward,right,up;
    vec3_t muzzle, targetPos;
    vec3_t myMaxs, targetMaxs;
    trace_t trace;
    int distance, myMax, targetMax;
    AngleVectors( self->client->ps.viewangles, forward, right, up);
    
    CalcMuzzlePoint( self, forward, right, up , muzzle);
    BG_FindBBoxForClass(self->client->ps.stats[STAT_PCLASS], NULL, myMaxs, NULL, NULL, NULL);
    
    if(targetIsEntity(target) && target.ent->client)
        BG_FindBBoxForClass(target.ent->client->ps.stats[STAT_PCLASS], NULL,targetMaxs, NULL, NULL, NULL);
    else if(targetIsEntity(target) && getTargetType(target) == ET_BUILDABLE)
        BG_FindBBoxForBuildable(target.ent->s.modelindex, NULL, targetMaxs);
    else 
        VectorSet(targetMaxs, 0, 0, 0);
    targetMax = VectorLengthSquared(targetMaxs);
    myMax = VectorLengthSquared(myMaxs);
    
    switch(self->s.weapon) {
        case WP_ABUILD:
            range = 0; //poor granger :(
            secondaryRange = 0;
            break;
        case WP_ABUILD2:
            range = ABUILDER_CLAW_RANGE;
            secondaryRange = 300; //An arbitrary value for the blob launcher, has nothing to do with actual range
            break;
        case WP_ALEVEL0:
            range = LEVEL0_BITE_RANGE;
            secondaryRange = 0;
            break;
        case WP_ALEVEL1:
            range = LEVEL1_CLAW_RANGE;
            secondaryRange = 0;
            break;
        case WP_ALEVEL1_UPG:
            range = LEVEL1_CLAW_RANGE;
            secondaryRange = LEVEL1_PCLOUD_RANGE;
            break;
        case WP_ALEVEL2:
            range = LEVEL2_CLAW_RANGE;
            secondaryRange = 0;
            break;
        case WP_ALEVEL2_UPG:
            range = LEVEL2_CLAW_RANGE;
            secondaryRange = LEVEL2_AREAZAP_RANGE;
            break;
        case WP_ALEVEL3:
            range = LEVEL3_CLAW_RANGE;
            secondaryRange = 900; //An arbitrary value for pounce, has nothing to do with actual range
            break;
        case WP_ALEVEL3_UPG:
            range = LEVEL3_CLAW_RANGE;
            secondaryRange = 900; //An arbitrary value for pounce and barbs, has nothing to do with actual range
            break;
        case WP_ALEVEL4:
            range = LEVEL4_CLAW_RANGE;
            secondaryRange = 0; //Using 0 since tyrant rush is basically just movement, not a ranged attack
            break;
        case WP_HBUILD:
            range = 100;
            secondaryRange = 0;
            break;
        case WP_PAIN_SAW:
            range = PAINSAW_RANGE;
            secondaryRange = 0;
            break;
        case WP_FLAMER:
            range = FLAMER_SPEED;
            secondaryRange = 0;
            break;
        case WP_SHOTGUN:
            range = (100 * 8192)/SHOTGUN_SPREAD; //100 is the maximum radius we want the spread to be
            secondaryRange = 0;
            break;
        case WP_MACHINEGUN:
            range = (100 * 8192)/RIFLE_SPREAD; //100 is the maximum radius we want the spread to be
            secondaryRange = 0;
            break;
        case WP_CHAINGUN:
            range = (100 * 8192)/CHAINGUN_SPREAD; //100 is the maximum radius we want the spread to be
            secondaryRange = 0;
            break;
        default:
            range = 4098 * 4; //large range for guns because guns have large ranges :)
            secondaryRange = 0; //no secondary attack
    }
    getTargetPos(target, &targetPos);
    trap_Trace(&trace,muzzle,NULL,NULL,targetPos,self->s.number,MASK_SHOT);
    distance = DistanceSquared(self->s.pos.trBase, targetPos);
    distance = (int) distance - myMax/2 - targetMax/2;
    
    if((distance <= Square(range) || distance <= Square(secondaryRange))
    &&(trace.entityNum == getTargetEntityNumber(target) || trace.fraction == 1.0f))
        return qtrue;
    else
        return qfalse;
}
/**botFireWeapon
 * Makes the bot attack/fire his weapon
 */
void botFireWeapon(gentity_t *self, usercmd_t *botCmdBuffer) {
    vec3_t forward,right,up;
    vec3_t muzzle, targetPos;
    vec3_t myMaxs,targetMaxs;
    int distance, myMax,targetMax;
    BG_FindBBoxForClass(self->client->ps.stats[STAT_PCLASS], NULL, myMaxs, NULL, NULL, NULL);
    
    if(targetIsEntity(self->botMind->goal) && self->botMind->goal.ent->client)
        BG_FindBBoxForClass(self->botMind->goal.ent->client->ps.stats[STAT_PCLASS], NULL,targetMaxs, NULL, NULL, NULL);
    else if(targetIsEntity(self->botMind->goal) && getTargetType(self->botMind->goal) == ET_BUILDABLE)
        BG_FindBBoxForBuildable(self->botMind->goal.ent->s.modelindex, NULL, targetMaxs);
    else 
        VectorSet(targetMaxs, 0, 0, 0);
    targetMax = VectorLengthSquared(targetMaxs);
    myMax = VectorLengthSquared(myMaxs);
    getTargetPos(self->botMind->goal,&targetPos);
    distance = DistanceSquared(self->s.pos.trBase, targetPos);
    distance = (int) distance - myMax/2 - targetMax/2;
    
    AngleVectors(self->client->ps.viewangles, forward,right,up);
    CalcMuzzlePoint(self,forward,right,up,muzzle);
    if( self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS ) {
        switch(self->client->ps.stats[STAT_PCLASS]) {
            case PCL_ALIEN_BUILDER0:
                botCmdBuffer->buttons |= BUTTON_GESTURE;
                break;
            case PCL_ALIEN_BUILDER0_UPG:
                if(distance < Square(ABUILDER_CLAW_RANGE))
                    botCmdBuffer->buttons |= BUTTON_ATTACK2;
                else
                    botCmdBuffer->buttons |= BUTTON_USE_HOLDABLE;
                break;
            case PCL_ALIEN_LEVEL0:
                break; //nothing, auto hit
            case PCL_ALIEN_LEVEL1:
                botCmdBuffer->buttons |= BUTTON_ATTACK;
                break;
            case PCL_ALIEN_LEVEL1_UPG:
                if(distance <= Square(LEVEL1_CLAW_RANGE))
                    botCmdBuffer->buttons |= BUTTON_ATTACK;
                else
                    botCmdBuffer->buttons |= BUTTON_ATTACK2; //gas
                break;
            case PCL_ALIEN_LEVEL2:
                if(self->client->time1000 % 300 == 0)
                    botCmdBuffer->upmove = 20; //jump
                botCmdBuffer->buttons |= BUTTON_ATTACK;
                break;
            case PCL_ALIEN_LEVEL2_UPG:
                if(self->client->time1000 % 300 == 0)
                    botCmdBuffer->upmove = 20; //jump
                if(distance <= Square(LEVEL2_CLAW_RANGE))
                    botCmdBuffer->buttons |= BUTTON_ATTACK;
                else
                    botCmdBuffer->buttons |= BUTTON_ATTACK2; //zap
                break;
            case PCL_ALIEN_LEVEL3:
                if(distance > Square(2 * LEVEL3_CLAW_RANGE) && 
                    self->client->ps.stats[ STAT_MISC ] < LEVEL3_POUNCE_SPEED) {
                    botCmdBuffer->angles[PITCH] -= calcPounceAimDelta(self, self->botMind->goal) - self->client->ps.delta_angles[PITCH]; //look up a bit more
                    botCmdBuffer->buttons |= BUTTON_ATTACK2; //pounce
                } else
                    botCmdBuffer->buttons |= BUTTON_ATTACK;
                break;
            case PCL_ALIEN_LEVEL3_UPG:
                if(self->client->ps.ammo[WP_ALEVEL3_UPG] > 0 && 
                    distance > Square(2 * LEVEL3_CLAW_RANGE) && getTargetType(self->botMind->goal) == ET_BUILDABLE) {
                    botCmdBuffer->angles[PITCH] -= calcBarbAimDelta(self, self->botMind->goal) - self->client->ps.delta_angles[PITCH]; //look up a bit more
                    botCmdBuffer->buttons |= BUTTON_USE_HOLDABLE; //barb
                    botCmdBuffer->forwardmove = 0; //dont move while sniping
                    botCmdBuffer->rightmove = 0;
                    botCmdBuffer->upmove = 0;
                } else {       
                    if(distance > Square(2 * LEVEL3_CLAW_RANGE) && 
                    self->client->ps.stats[ STAT_MISC ] < LEVEL3_POUNCE_UPG_SPEED) {
                        botCmdBuffer->angles[PITCH] -= calcPounceAimDelta(self, self->botMind->goal) - self->client->ps.delta_angles[PITCH];; //look up a bit more
                        botCmdBuffer->buttons |= BUTTON_ATTACK2; //pounce
                    }else
                        botCmdBuffer->buttons |= BUTTON_ATTACK;
                }
                break;
            case PCL_ALIEN_LEVEL4:
                if (distance > Square(LEVEL4_CLAW_RANGE))
                    botCmdBuffer->buttons |= BUTTON_ATTACK2; //charge
                else
                    botCmdBuffer->buttons |= BUTTON_ATTACK;
                break;
            default: break; //nothing
        }
        
    } else if( self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS ) {
        if(self->client->ps.weapon == WP_FLAMER)
        {
                botCmdBuffer->buttons |= BUTTON_ATTACK;
            
        } else if( self->client->ps.weapon == WP_LUCIFER_CANNON ) {
            
            if( self->client->time10000 % 2000 ) {
                botCmdBuffer->buttons |= BUTTON_ATTACK;
            }
        } else if(self->client->ps.weapon == WP_HBUILD || self->client->ps.weapon == WP_HBUILD2) {
            botCmdBuffer->buttons |= BUTTON_ATTACK2;
        } else
            botCmdBuffer->buttons |= BUTTON_ATTACK; //just fire
            
    }
}
/**
 * Helper functions for managing botTarget_t structures
 * Please use these when you need to access something in a botTarget structure
 * If you use the structure directly, you may make the program crash if the member is undefined
 */
int getTargetEntityNumber(botTarget_t target) {
    if(target.ent) 
        return target.ent->s.number;
    else
        return ENTITYNUM_NONE;
}
void getTargetPos(botTarget_t target, vec3_t *rVec) {
    if(target.ent)
        VectorCopy( target.ent->s.origin, *rVec);
    else
        VectorCopy(target.coord, *rVec);
}
int getTargetTeam( botTarget_t target) {
    if(target.ent) {
        return getEntityTeam(target.ent);
    } else
        return PTE_NONE;
}
int getTargetType( botTarget_t target) {
    if(target.ent)
        return target.ent->s.eType;
    else
        return -1;
}
qboolean targetIsEntity( botTarget_t target) {
    if(target.ent)
        return qtrue;
    else
        return qfalse;
}
/**setGoalEntity
 * Used to set a new goal for the bot
 *The goal is an entity
 */
void setGoalEntity(gentity_t *self, gentity_t *goal ){
    self->botMind->goal.ent = goal;
    findRouteToTarget(self, self->botMind->goal);
    setNewRoute(self);
}

/**setGoalCoordinate
 * Used to set a new goal for the bot 
 * The goal is a coordinate
 */
void setGoalCoordinate(gentity_t *self, vec3_t goal ) {
    VectorCopy(goal,self->botMind->goal.coord);
    self->botMind->goal.ent = NULL;
    findRouteToTarget(self, self->botMind->goal);
    setNewRoute(self);
}
/**setTargetEntity
 * Generic function for setting a botTarget that is not our current goal
 * Note, this function does not compute a route to the specified target
 */
void setTargetEntity(botTarget_t *target, gentity_t *goal ){
    target->ent = goal;
}
/**setTargetCoordinate
 * Generic function for setting a botTarget that is not our current goal
 * Note, this function does not compute a route to the specified target
 */
void setTargetCoordinate(botTarget_t *target, vec3_t goal ) {
    VectorCopy(goal, target->coord);
    target->ent = NULL;
}

//helper function for getting an entity's team
int getEntityTeam(gentity_t *ent) {
    if(ent->client) {
        return ent->client->ps.stats[STAT_PTEAM];
    } else if(ent->s.eType == ET_BUILDABLE) {
        return ent->biteam;
    } else {
        return PTE_NONE;
    }
}
int botFindBuilding(gentity_t *self, int buildingType, int range) {

    // range converted to a vector
    vec3_t vectorRange;
    // Lower bound vector
    vec3_t mins;
    // Upper bound vector
    vec3_t maxs;
    // Indexing field
    int total_entities;
    int entityList[ MAX_GENTITIES ];
    int i;
    float minDistance = -1;
    int closestBuilding = ENTITYNUM_NONE;
    float newDistance;
    gentity_t *target;
    if(range != -1) {
        VectorSet( vectorRange, range, range, range );
        VectorAdd( self->client->ps.origin, vectorRange, maxs );
        VectorSubtract( self->client->ps.origin, vectorRange, mins );
        total_entities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);
        for( i = 0; i < total_entities; ++i ) {
            target = &g_entities[entityList[ i ] ];
            
            if( target->s.eType == ET_BUILDABLE && target->s.modelindex == buildingType && (target->biteam == PTE_ALIENS || (target->powered && target->spawned)) && target->health > 0) {
                newDistance = DistanceSquared( self->s.pos.trBase, target->s.pos.trBase );
                if( newDistance < minDistance|| minDistance == -1) {
                    minDistance = newDistance;
                    closestBuilding = entityList[i];
                }
            }
            
        }
    } else {
        for( i = 0; i < level.num_entities; ++i ) {
            target = &g_entities[i];
            
            if( target->s.eType == ET_BUILDABLE && target->s.modelindex == buildingType && (target->biteam == PTE_ALIENS || (target->powered && target->spawned)) && target->health > 0) {
                newDistance = DistanceSquared(self->s.pos.trBase,target->s.pos.trBase);
                if( newDistance < minDistance|| minDistance == -1) {
                    minDistance = newDistance;
                    closestBuilding = i;
                }
            }
            
        }
    }
    
    return closestBuilding;
}
    
    
void G_BotSpectatorThink( gentity_t *self ) {
    //hacky ping fix
    self->client->ps.ping = rand() % 50 + 50;
    if( self->client->ps.pm_flags & PMF_QUEUED) {
        //we're queued to spawn, all good
        //check for humans in the spawn que
        {
            spawnQueue_t *sq;
            if(self->client->pers.teamSelection == PTE_HUMANS)
                sq = &level.humanSpawnQueue;
            else if(self->client->pers.teamSelection == PTE_ALIENS)
                sq = &level.alienSpawnQueue;
            else
                sq = NULL;
            
            if( sq && G_BotCheckForSpawningPlayers( self )) {
                G_RemoveFromSpawnQueue( sq, self->s.number );
                G_PushSpawnQueue( sq, self->s.number );
            }
        }
        return;
    }
    
    //reset stuff
    self->botMind->followingRoute = qfalse;
    self->botMind->currentModus = IDLE;
    self->botMind->targetNodeID = -1;
    self->botMind->needsNewGoal = qtrue;
    self->botMind->targetNodeID = -1;
    self->botMind->lastNodeID = -1;
    
    if( self->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        int teamnum = self->client->pers.teamSelection;
        int clientNum = self->client->ps.clientNum;

        if( teamnum == PTE_HUMANS ) {
            self->client->pers.classSelection = PCL_HUMAN;
            self->client->ps.stats[STAT_PCLASS] = PCL_HUMAN;

            self->client->pers.humanItemSelection = self->botMind->spawnItem;

            G_PushSpawnQueue( &level.humanSpawnQueue, clientNum );
        } else if( teamnum == PTE_ALIENS) {
            self->client->pers.classSelection = PCL_ALIEN_LEVEL0;
            self->client->ps.stats[STAT_PCLASS] = PCL_ALIEN_LEVEL0;
            G_PushSpawnQueue( &level.alienSpawnQueue, clientNum );
        }
    }
}
qboolean G_BotCheckForSpawningPlayers( gentity_t *self ) 
{
    //this function only checks if there are Humans in the SpawnQueue
    //which are behind the bot
    int i;
    int botPos = 0, lastPlayerPos = 0;
    spawnQueue_t *sq;
    
    if(self->client->pers.teamSelection == PTE_HUMANS)
        sq = &level.humanSpawnQueue;
    
    else if(self->client->pers.teamSelection == PTE_ALIENS)
        sq = &level.alienSpawnQueue;
    else
        return qfalse;
    
    i = sq->front;
    
    if( G_GetSpawnQueueLength( sq ) ) {
        do {
            if( !(g_entities[ sq->clients[ i ] ].r.svFlags & SVF_BOT)) {
                if( i < sq->front )
                    lastPlayerPos = i + MAX_CLIENTS - sq->front;
                else
                    lastPlayerPos = i - sq->front;
            }
            
            if( sq->clients[ i ] == self->s.number ) {
                if( i < sq->front )
                    botPos = i + MAX_CLIENTS - sq->front;
                else
                    botPos = i - sq->front;
            }
            
            i = QUEUE_PLUS1( i );
        } while( i != QUEUE_PLUS1( sq->back ) );
    }
    
    if(botPos < lastPlayerPos)
        return qtrue;
    else
        return qfalse;
}
/*
 * Called when we are in intermission.
 * Just flag that we are ready to proceed.
 */
void G_BotIntermissionThink( gclient_t *client )
{
    client->readyToExit = qtrue;
}
void botGetAimLocation(gentity_t *self, botTarget_t target, vec3_t *aimLocation) {
    vec3_t mins;
    //get the position of the enemy
    getTargetPos(target, aimLocation);
    
    
    if(getTargetType(target) != ET_BUILDABLE && targetIsEntity(target) && getTargetTeam(target) == PTE_HUMANS) {
        
        (*aimLocation)[2] += g_entities[getTargetEntityNumber(target)].r.maxs[2] * 0.85;
        
    } else if(getTargetType(target) == ET_BUILDABLE || getTargetTeam(target) == PTE_ALIENS) {
        VectorCopy( g_entities[getTargetEntityNumber(target)].s.origin, *aimLocation );
        //make lucifer cannons aim ahead based on the target's velocity
        if(self->s.weapon == WP_LUCIFER_CANNON) {
            VectorMA(*aimLocation, Distance(self->s.pos.trBase, *aimLocation) / LCANNON_SPEED, target.ent->s.pos.trDelta, *aimLocation);
        }
    } else { 
        //get rid of 'bobing' motion when aiming at waypoints by making the aimlocation the same height above ground as our viewheight
        //NOTE: the waypoints are about on ground level due to code in g_main.c so we just need to add mins height and viewheight
        BG_FindBBoxForClass(self->client->ps.stats[STAT_PCLASS], mins, NULL, NULL, NULL, NULL);
        (*aimLocation)[2] += -mins[2] + self->client->ps.viewheight;
    }
}

void botAimAtLocation( gentity_t *self, vec3_t target, usercmd_t *rAngles)
{
        vec3_t aimVec, oldAimVec, aimAngles;
        vec3_t refNormal, grapplePoint, xNormal, viewBase;
        //vec3_t highPoint;
        float turnAngle;
        int i;

        if( ! (self && self->client) )
                return;
        VectorCopy( self->s.pos.trBase, viewBase );
        viewBase[2] += self->client->ps.viewheight;
        
        VectorSubtract( target, viewBase, aimVec );
        VectorCopy(aimVec, oldAimVec);

        {
        VectorSet(refNormal, 0.0f, 0.0f, 1.0f);
        VectorCopy( self->client->ps.grapplePoint, grapplePoint);
        //cross the reference normal and the surface normal to get the rotation axis
        CrossProduct( refNormal, grapplePoint, xNormal );
        VectorNormalize( xNormal );
        turnAngle = RAD2DEG( acos( DotProduct( refNormal, grapplePoint ) ) );
        //G_Printf("turn angle: %f\n", turnAngle);
        }

        if(self->client->ps.stats[ STAT_STATE ] & SS_WALLCLIMBINGCEILING ) {
                //NOTE: the grapplePoint is here not an inverted refNormal :(
                RotatePointAroundVector( aimVec, grapplePoint, oldAimVec, -180.0);
        }
        else if( turnAngle != 0.0f)
                RotatePointAroundVector( aimVec, xNormal, oldAimVec, -turnAngle);

        vectoangles( aimVec, aimAngles );

        VectorSet(self->client->ps.delta_angles, 0.0f, 0.0f, 0.0f);

        for( i = 0; i < 3; i++ ) {
                aimAngles[i] = ANGLE2SHORT(aimAngles[i]);
        }

        //save bandwidth
        SnapVector(aimAngles);
        rAngles->angles[0] = aimAngles[0];
        rAngles->angles[1] = aimAngles[1];
        rAngles->angles[2] = aimAngles[2];
}
//blatently ripped from ShotgunPattern() in g_weapon.c :)
void botShakeAim( gentity_t *self, vec3_t *rVec ){
    vec3_t forward, right, up, diffVec;
    int seed;
    float r,u, length, speedAngle;
    AngleVectors(self->client->ps.viewangles, forward, right, up);
    //seed crandom
    seed = (int) rand() & 255;
    VectorSubtract(*rVec,self->s.origin, diffVec);
    length = (float) VectorLength(diffVec)/1000;
    VectorNormalize(diffVec);
    speedAngle=RAD2DEG(acos(DotProduct(forward,diffVec)))/100;
    r = crandom() * self->botMind->botSkill.aimShake * length * speedAngle;
    u = crandom() * self->botMind->botSkill.aimShake * length * speedAngle;
    VectorMA(*rVec, r, right, *rVec);
    VectorMA(*rVec,u,up,*rVec);
}
int botFindClosestEnemy( gentity_t *self, qboolean includeTeam ) {
    // return enemy entity index, or -1
    int vectorRange = MGTURRET_RANGE * 3;
    int i;
    int total_entities;
    int entityList[ MAX_GENTITIES ];
    float minDistance = (float) Square(MGTURRET_RANGE * 3);
    int closestTarget = ENTITYNUM_NONE;
    float newDistance;
    vec3_t range;
    vec3_t mins, maxs;
    gentity_t *target;
    botTarget_t botTarget;
    
    VectorSet( range, vectorRange, vectorRange, vectorRange );
    VectorAdd( self->client->ps.origin, range, maxs );
    VectorSubtract( self->client->ps.origin, range, mins );
    SnapVector(mins);
    SnapVector(maxs);
    total_entities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
    
    for( i = 0; i < total_entities; ++i ) {
        target = &g_entities[entityList[ i ] ];
        setTargetEntity(&botTarget, target);
        //DistanceSquared for performance reasons (doing sqrt constantly is bad and keeping it squared does not change result)
        newDistance = (float) DistanceSquared( self->s.pos.trBase, target->s.pos.trBase );
        //if entity is closer than previous stored one and the target is alive
        if( newDistance < minDistance && target->health > 0) {
            
            //if we can see the entity OR we are on aliens (who dont care about LOS because they have radar)
            if( (self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS ) || botTargetInRange(self, botTarget, MASK_SHOT) ){
                
                //if the entity is a building and we can attack structures and we are not a dretch
                if(target->s.eType == ET_BUILDABLE && g_bot_attackStruct.integer && self->client->ps.stats[STAT_PCLASS] != PCL_ALIEN_LEVEL0) {
                    
                    //if the building is not on our team (unless we can attack teamates)
                    if( target->biteam != self->client->ps.stats[STAT_PTEAM] || includeTeam ) {
                        
                        //store the new distance and the index of the entity
                        minDistance = newDistance;
                        closestTarget = entityList[i];
                    }
                    //if the entity is a player and not us
                } else if( target->client && self != target) {
                    //if we are not on the same team (unless we can attack teamates)
                    if( (target->client->ps.stats[STAT_PTEAM] != self->client->ps.stats[STAT_PTEAM])|| includeTeam ) {
                        
                        //store the new distance and the index of the enemy
                        minDistance = newDistance;
                        closestTarget = entityList[i];
                    }
                }
            }
        }

    }
        return closestTarget;
}

qboolean botTargetInRange( gentity_t *self, botTarget_t target, int mask ) {
    trace_t trace;
    vec3_t  muzzle, targetPos;
    vec3_t  forward, right, up;

    // set aiming directions
    AngleVectors( self->client->ps.viewangles, forward, right, up );

    CalcMuzzlePoint( self, forward, right, up, muzzle );
    getTargetPos(target, &targetPos);
    trap_Trace( &trace, muzzle, NULL, NULL,targetPos, self->s.number, mask);

    if( trace.surfaceFlags & SURF_NOIMPACT )
        return qfalse;
        
    //target is in range
    if( (trace.entityNum == getTargetEntityNumber(target) || trace.fraction == 1.0f) && !trace.startsolid )
        return qtrue;
    return qfalse;
}

int distanceToTargetNode( gentity_t *self ) {
        vec3_t mins;
        vec3_t pos;
        BG_FindBBoxForClass(self->client->ps.stats[STAT_PCLASS],mins, NULL, NULL, NULL, NULL);
        VectorCopy(self->s.pos.trBase, pos);
        pos[2] += mins[2]; //mins should be negative
        return (int) Distance(level.nodes[self->botMind->targetNodeID].coord, pos);
}
void botSlowAim( gentity_t *self, vec3_t target, float slow, vec3_t *rVec) {
        vec3_t viewBase;
        vec3_t aimVec, forward;
        vec3_t skilledVec;
        float slowness;
        
        if( !(self && self->client) )
                return;
        
        //get the point of where the bot is aiming from
        VectorCopy( self->s.pos.trBase, viewBase );
        viewBase[2] += self->client->ps.viewheight;
        
        //random targetVec manipulation (point of enemy)
        
        //get the Vector from the bot to the enemy (aim Vector)
        VectorSubtract( target, viewBase, aimVec );
        //take the current aim Vector
        AngleVectors( self->client->ps.viewangles, forward, NULL, NULL);
        
        //make the aim slow by not going the full difference
        //between the current aim Vector and the new one
        slowness = slow*(25/1000.0);
        if(slowness > 1.0) slowness = 1.0;
        
        VectorLerp( slowness, forward, aimVec, skilledVec);
        
        VectorAdd(viewBase, skilledVec, *rVec);
}
//finds the closest node to the start, but also takes into account position of end when choosing closest
//chooses best match
int findClosestNode( botTarget_t startTarget) {
        trace_t trace;
        int i,k,n = 0;
        long distance = 0;
        long closestNodeDistances[10] = {-1,-1,-1,-1, -1, -1, -1, -1, -1, -1};
        int closestNodes[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
        vec3_t start;
        getTargetPos(startTarget, &start);
        //move viewpoint to ground (used for trace)
        if(targetIsEntity(startTarget)) {
            start[2] += startTarget.ent->r.mins[2];
        }
        for(i = 0; i < level.numNodes; i++) {
            distance = DistanceSquared(start,level.nodes[i].coord);
            //check if new distance is shorter than one of the 4 we have
            for(k=0;k<10;k++) {
                if(distance < closestNodeDistances[k] || closestNodeDistances[k] == -1) {
                    
                    //need to move the other elements up 1 index
                    //loop will not execute if k == 9
                    for(n=9;n>k;n--) {
                        closestNodeDistances[n] = closestNodeDistances[n - 1];
                        closestNodes[n] = closestNodes[n - 1];
                    }
                    closestNodeDistances[k] = distance;
                    closestNodes[k] = i;
                    k=10; //get out of inner loop
                } else {
                    continue;
                }
            }
        }
        n=0;
        //now loop through the closestnodes and find the closest 2 nodes that are in LOS
        //note that they are sorted by distance in the array
        for(i = 0; i < 10; i++) {
            trap_Trace(&trace, start, NULL, NULL, level.nodes[closestNodes[i]].coord, getTargetEntityNumber(startTarget), MASK_DEADSOLID);
            if( trace.fraction == 1.0f && closestNodes[i] != -1) {
                closestNodes[n] = closestNodes[i];
                n++;
            }
        }

        //return the closest nodes that start can see
        //NOTE: if none of the nodes could be seen from the start point, it will return the closest node to the startpoint regardless
        return closestNodes[0];
}


void doLastNodeAction(gentity_t *self, usercmd_t *botCmdBuffer) {
    vec3_t targetPos;
    getTargetPos(self->botMind->targetNode,&targetPos);
    switch(level.nodes[self->botMind->lastNodeID].action)
    {
        case BOT_JUMP:  
            
            if( self->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS && 
                self->client->ps.stats[ STAT_STAMINA ] < 0 )
            {break;}
            if( !BG_ClassHasAbility( self->client->ps.stats[ STAT_PCLASS ], SCA_WALLCLIMBER ) )
                
                botCmdBuffer->upmove = 20;
            break;
            //we should not need this now that wallclimb is always enabled in G_BotGoto
        case BOT_WALLCLIMB: if( BG_ClassHasAbility( self->client->ps.stats[ STAT_PCLASS ], SCA_WALLCLIMBER ) ) {
            botCmdBuffer->upmove = -1;
        }
        break;
        case BOT_KNEEL: if(self->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS)
        {
            botCmdBuffer->upmove = -1;
        }
        break;
        case BOT_POUNCE:if(self->client->ps.stats[STAT_PCLASS] == PCL_ALIEN_LEVEL3 && 
            self->client->ps.stats[ STAT_MISC ] < LEVEL3_POUNCE_SPEED) {
            botCmdBuffer->angles[PITCH] -= calcPounceAimDelta(self, self->botMind->targetNode) - self->client->ps.delta_angles[PITCH];
            botCmdBuffer->buttons |= BUTTON_ATTACK2;
            }else if(self->client->ps.stats[STAT_PCLASS] == PCL_ALIEN_LEVEL3_UPG && 
                self->client->ps.stats[ STAT_MISC ] < LEVEL3_POUNCE_UPG_SPEED) {
            botCmdBuffer->angles[PITCH] -= calcPounceAimDelta(self, self->botMind->targetNode) - self->client->ps.delta_angles[PITCH];
            botCmdBuffer->buttons |= BUTTON_ATTACK2;
                }
                break;
        default: break;
    }
    
}
void setNewRoute(gentity_t *self) { 
    self->botMind->timeFoundNode = level.time;
    self->botMind->targetNodeID = self->botMind->startNodeID;
    self->botMind->followingRoute = qtrue;
}

void findRouteToTarget( gentity_t *self, botTarget_t target ) {
    long shortDist[MAX_NODES];
    short i;
    short k;
    int bestNode;
    int childNode;
    short visited[MAX_NODES];
    short startNum = -1;
    short endNum = -1;
    vec3_t start = {0,0,0}; 
    vec3_t end;
    botTarget_t bot;
    trace_t trace;
    setTargetEntity(&bot, self);
    VectorCopy(self->s.pos.trBase,start);
    //set initial variable values
    for( i=0;i<MAX_NODES;i++) {
        shortDist[i] = INFINITE;
        self->botMind->routeToTarget[i] = -1;
        visited[i] = 0;
    }
    startNum = findClosestNode(bot);
    endNum = findClosestNode(target);
    
    //no closestnode
    if(startNum == -1 || endNum == -1)
        return;
    shortDist[endNum] = 0;
    //NOTE: the algorithm has been reversed so we dont have to go through the final route and reverse it before we use it
    //Dijkstra's Algorithm
    //TODO: Implement A* algorithm
    for (k = 0; k < level.numNodes; ++k) {
        bestNode = -1;
        for (i = 0; i < level.numNodes; ++i) {
            if (!visited[i] && ((bestNode == -1) || (shortDist[i] < shortDist[bestNode])))
                bestNode = i;
        }
        if(bestNode == -1)
            break;      //there are no more nodes with distance < infinite
        if(bestNode == startNum) //we have found a shortest path, no need to continue
            break;
        visited[bestNode] = 1;
        
        for (i = 0; i < MAX_PATH_NODES; ++i) {
            childNode = level.nodes[bestNode].nextid[i];
            if (childNode != -1 && childNode < 1000 ) {
                if (shortDist[bestNode] + level.distNode[bestNode][childNode] < shortDist[childNode]) {
                    shortDist[childNode] = shortDist[bestNode] + level.distNode[bestNode][childNode];
                    self->botMind->routeToTarget[childNode] = bestNode;
                }
            }
        }
    }
        self->botMind->lastRouteSearch = level.time;
        VectorCopy(level.nodes[self->botMind->routeToTarget[startNum]].coord, end);
        trap_Trace(&trace, start, NULL, NULL, end, self->s.number, MASK_SHOT);
        
        
        
        if(trace.fraction == 1.0f)
            self->botMind->startNodeID = self->botMind->routeToTarget[startNum];
        else
            self->botMind->startNodeID = startNum;
        self->botMind->endNodeID = endNum;
}

void setSkill(gentity_t *self, int skill) {
    self->botMind->botSkill.level = skill;
    //different aim for different teams
    if(self->botMind->botTeam == PTE_HUMANS) {
        self->botMind->botSkill.aimSlowness = (float) skill / 60;
        self->botMind->botSkill.aimShake = (int) (10 - skill);
    } else {
        self->botMind->botSkill.aimSlowness = (float) skill / 30;
        self->botMind->botSkill.aimShake = (int) (10 - skill);
    }
}
float calcPounceAimDelta(gentity_t *self, botTarget_t target) {
    vec3_t startPos;
    vec3_t targetPos;
    getTargetPos(target, &targetPos);
    VectorCopy(self->s.pos.trBase, startPos);
    targetPos[2] = 0.0f;
    startPos[2] = 0.0f;
    return Distance(startPos, targetPos) * 5;
}
float calcBarbAimDelta(gentity_t *self, botTarget_t target) {
    vec3_t startPos;
    vec3_t targetPos;
    getTargetPos(target, &targetPos);
    VectorCopy(self->s.pos.trBase, startPos);
    targetPos[2] = 0.0f;
    startPos[2] = 0.0f;
    return Distance(startPos, targetPos) * 3;
}
    
