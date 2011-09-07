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

void G_BotAdd( char *name, int team, int skill ) {
    int i;
    int clientNum;
    char userinfo[MAX_INFO_STRING];
    int reservedSlots = 0;
    gentity_t *bot;
    //char buffer [33];
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

    //clientNum = trap_BotAllocateClient();

    //no slots available
    //if( clientNum == -1 )
    //return;
    //char buffer;
    //buffer = (char) clientNum + 48;
    //iota( clientNum, buffer, 10);
    //trap_Printf(va("clientNum = %s\n",clientNum + ""));
    bot = &g_entities[ clientNum ];
    //bot->client = &level.clients[ clientNum ];
    bot->r.svFlags |= SVF_BOT;
    bot->inuse = qtrue;


    //default bot data
    bot->botCommand = BOT_AUTO;
    bot->botFriend = NULL;
    bot->botEnemy = NULL;
    bot->botFriendLastSeen = 0;
    bot->botEnemyLastSeen = 0;
    bot->botSkillLevel = skill;
    bot->botTeam = team;
    bot->spawnItem = WP_HBUILD;
    bot->state = FINDNEWNODE;
    bot->timeFoundEnemy = 0;
    bot->followingRoute = qfalse;
    
    //different aim for different teams
    if(bot->botTeam == PTE_HUMANS) {
        bot->aimSlowness = (float) bot->botSkillLevel / 80;
        bot->aimShake = (int) 100 * (10 - bot->botSkillLevel)/2 + 0.5;
    } else {
        bot->aimSlowness = (float) bot->botSkillLevel / 40;
        bot->aimShake = (int) (10 - bot->botSkillLevel)/2 + .05;
    }

    // register user information
    userinfo[0] = '\0';
    Info_SetValueForKey( userinfo, "name", name );
    Info_SetValueForKey( userinfo, "rate", "25000" );
    Info_SetValueForKey( userinfo, "snaps", "20" );
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

    bot->botFriend = NULL;
    bot->botEnemy = NULL;
    bot->botFriendLastSeen = 0;
    bot->botEnemyLastSeen = 0;

    if( !Q_stricmp( command, "auto" ) )
        bot->botCommand = BOT_AUTO;
    else if( !Q_stricmp( command, "attack" ) )
        bot->botCommand = BOT_ATTACK;
    else if( !Q_stricmp( command, "idle" ) )
        bot->botCommand = BOT_IDLE;
    else if( !Q_stricmp( command, "defensive" ) ) {
        bot->botCommand = BOT_DEFENSIVE;
	bot->botDefensePoint = bot->s.pos.trBase;
    } else if( !Q_stricmp( command, "followattack" ) ) {
        bot->botCommand = BOT_FOLLOW_FRIEND_ATTACK;
        bot->botFriend = master;
    } else if( !Q_stricmp( command, "followidle" ) ) {
        bot->botCommand = BOT_FOLLOW_FRIEND_IDLE;
        bot->botFriend = master;
    } else if( !Q_stricmp( command, "teamkill" ) )
        bot->botCommand = BOT_TEAM_KILLER;
    else if( !Q_stricmp( command, "repair" ) ) {
        if(BG_InventoryContainsWeapon(WP_HBUILD, bot->client->ps.stats)) {
            bot->botCommand = BOT_REPAIR;
            G_ForceWeaponChange( bot, WP_HBUILD );
        }
    } else if( !Q_stricmp( command, "spawnrifle" ) )
        bot->spawnItem = WP_MACHINEGUN;
    else if( !Q_stricmp( command, "spawnckit" ) )
        bot->spawnItem = WP_HBUILD;
    else {
        bot->botCommand = BOT_AUTO;
        trap_SendServerCommand(-1, "print \"Unknown mode. Reverting to auto.\n\"");
    }
    return;
}


void G_BotMove(gentity_t *self, usercmd_t *botCmdBuffer)
{
    int distanceToEnemy = 999;
    if(self->botEnemy)
    distanceToEnemy = botGetDistanceBetweenPlayer(self,self->botEnemy);
    //prevent human bots from moving toward target while attacking buildables
    //TODO: Refacter this to be much cleaner and less of a hack
    if( self->botEnemy->s.eType != ET_BUILDABLE || 
	self->client->ps.stats[ STAT_PTEAM ] == PTE_ALIENS || 
	!botTargetInRange(self,self->botEnemy) ||
	// !botTargetInRange(self, (self->botEnemy) ? self->botEnemy : self->botTarget)){
	(self->client->ps.weapon == WP_FLAMER && distanceToEnemy > FLAMER_LIFETIME * FLAMER_SPEED / 1000) ||
	(self->client->ps.weapon == WP_PAIN_SAW && distanceToEnemy > PAINSAW_RANGE)) {
    
	//dont go too far if in defend mode
	//if( Distance( self->s.pos.trBase, self->botDefensePoint ) < MGTURRET_RANGE * 3 ||
	    //self->botCommand != BOT_DEFENSIVE)
	botCmdBuffer->forwardmove = 127;

        if(self->state != TARGETNODE) {
            /*
            botCmdBuffer->rightmove = -100;
            if(self->client->time1000 >= 500 && random() < 0.7)
                botCmdBuffer->rightmove = 100;
        
            */
            if(self->client->time1000 >= 500)
                        botCmdBuffer->rightmove = 127;
                else
                        botCmdBuffer->rightmove = -127;

                if((self->client->time10000 % 2000) < 1000)
                        botCmdBuffer->rightmove *= -1;

                if((self->client->time1000 % 300) >= 100 && (self->client->time10000 % 3000) > 2000)
                        botCmdBuffer->rightmove = 0;
        }
        
        //try to get around the block
        if(botPathIsBlocked(self)) {
            
            if((self->client->time10000 % 4000) > 2000)
                botCmdBuffer->rightmove = 100;
            else
                botCmdBuffer->rightmove = -100;
        }
	
	self->client->ps.stats[ STAT_STAMINA ] = MAX_STAMINA;

	if( botPathIsBlocked( self ) ) {
	    botCmdBuffer->upmove = 127;
	}
	
	//need to periodically reset upmove to 0 for jump to work
	if( self->client->time10000 % 1000)
	    botCmdBuffer->upmove = 0;
        
        //stay back as human
        if(self->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS && 
        distanceToEnemy < 300 && botTargetInRange(self, self->botEnemy) &&
        self->client->ps.weapon != WP_PAIN_SAW && 
        self->client->ps.weapon != WP_FLAMER)
        {
            botCmdBuffer->forwardmove = -100;
        }
        
    }
}

void G_BotThink( gentity_t *self) {
    int tempEntityIndex = -1;
    usercmd_t  botCmdBuffer = self->client->pers.cmd;
    botCmdBuffer.buttons = 0;
    botCmdBuffer.forwardmove = 0;
    botCmdBuffer.rightmove = 0;
    
    G_BotEvolveAI(self, &botCmdBuffer);
    //G_BotBuyAI(self);
    // reset botEnemy if enemy is dead
    if(self->botEnemy && self->botEnemy->health <= 0) {
        self->botEnemy = NULL;
        self->state = FINDNEWNODE;
    }
    // if friend dies, reset status to auto
    if(self->botFriend && self->botFriend->health <= 0) {
        self->botCommand = BOT_AUTO;
        self->botFriend = NULL;
        self->state = FINDNEWNODE;
    }

    // if health < 30, use medkit
    if(self->health <= 30)
        BG_ActivateUpgrade( UP_MEDKIT, self->client->ps.stats );
    
    //every 2 seconds, look for a new closer enemy or when we dont have an enemy
    if(self->timeFoundEnemy + BOT_ENEMYSEARCH_INTERVAL < level.time || !self->botEnemy) {
        tempEntityIndex = botFindClosestEnemy( self, qfalse );
        if( tempEntityIndex != -1 && self->botEnemy != &g_entities[tempEntityIndex]) {
            self->botEnemy = &g_entities[tempEntityIndex];
            self->followingRoute = qfalse;
            
        }
        self->timeFoundEnemy = level.time;
    }
    
    //luci TK prevention
    if( self->client->ps.weapon == WP_LUCIFER_CANNON && !self->botEnemy && self->isFireing)
    {
        G_ForceWeaponChange( self, WP_BLASTER );
        G_ForceWeaponChange( self, WP_LUCIFER_CANNON );
        self->isFireing = qfalse;
    }
    
    switch( self->botCommand ) {
        case BOT_AUTO:
            
            //attack mode
            if(self->botEnemy)
                Attack(self, qfalse, &botCmdBuffer);
            
            //repair mode
            else if(botFindDamagedFriendlyStructure(self) != -1 && 
                BG_InventoryContainsWeapon(WP_HBUILD,self->client->ps.stats))
                Repair(self, &botCmdBuffer);
            //heal mode
            else if((self->health < BOT_LOW_HP || !BG_InventoryContainsUpgrade(UP_MEDKIT, self->client->ps.stats)) &&
                botFindBuilding(self, BA_H_MEDISTAT, BOT_MEDI_RANGE) != -1 && self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS)
                Heal(self, &botCmdBuffer);
            
            //TODO: clean this up and make it better by introducing botCanPurchaseNewItem
            //buy mode
            else if( ( ( (short)self->client->ps.persistant[ PERS_CREDIT ] > 70 && 
                !BG_InventoryContainsUpgrade( UP_LIGHTARMOUR, self->client->ps.stats ) ) ||
                self->client->ps.weapon == WP_BLASTER || 
                botWeaponHasLowAmmo(self) ) && 
                self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS &&
                botFindBuilding(self, BA_H_ARMOURY, BOT_ARM_RANGE) != -1 &&
                g_bot_buy.integer == 1 )
                Buy(self, &botCmdBuffer);
            else {
                //reset stuff to begin roaming the map
                self->botDest.ent = NULL;
                if(self->state == LOST || self->state == TARGETOBJECTIVE)
                    self->state = FINDNEWNODE;
                self->followingRoute = qfalse;
            }
            break;
        case BOT_ATTACK:
            Attack(self, qfalse, &botCmdBuffer);
            break;
        case BOT_TEAM_KILLER:
            Attack(self,qtrue, &botCmdBuffer);
            break;
        case BOT_FOLLOW_FRIEND_IDLE:
            Follow(self, &botCmdBuffer);
        case BOT_FOLLOW_FRIEND_ATTACK:
            if(!self->botEnemy)
                Follow(self, &botCmdBuffer);
            else
                Attack(self, qfalse, &botCmdBuffer);
            break;
        case BOT_REPAIR:
             Repair(self, &botCmdBuffer);
            break;
        case BOT_IDLE:
            break;
        default: break;
    }
    if(self->botCommand != BOT_IDLE)
        pathfinding(self, &botCmdBuffer);
    self->client->pers.cmd = botCmdBuffer;
}
void Attack(gentity_t *self, qboolean attackTeam, usercmd_t *botCmdBuffer) {
    
    int tempEntityIndex = -1;
    vec3_t tmpVec;
    //int clicksToStopChase = 30; //about 5 seconds
    //if out of ammo switch to blaster
    if((BG_WeaponIsEmpty(self->client->ps.weapon, self->client->ps.ammo, self->client->ps.powerups)
        || self->client->ps.weapon == WP_HBUILD) && self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS)
            G_ForceWeaponChange( self, WP_BLASTER );
    // if there is enemy around, rush them and attack.
        if(self->botEnemy) {
                //dont remove the botTargetInRange check, otherwise there will be massive ammounts of lag as everyone
                // needlessly keeps finding new routes whenever the enemy is in range
                //the botEnemyLastSeen check is to help with the erratic aiming when enemy reapeatedly goes in/out of view
            if((self->botDest.ent != self->botEnemy || self->botDest.ent == NULL || !self->followingRoute) &&
            !botTargetInRange(self, self->botEnemy) && (self->botEnemyLastSeen + 500 < level.time || self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS)) {
                //put the assainments in GoToward
                self->botDest.ent = self->botEnemy;
                VectorCopy(self->botEnemy->s.pos.trBase, self->botDest.coord);
                findRouteToTarget(self, self->botDest.coord);
                setNewRoute(self);
                //trap_SendServerCommand(-1,va("print \"New Target Node %d\n\"", self->targetNode));
            }
             
              // enemy!
            self->client->ps.pm_flags &= ~PMF_CROUCH_HELD;
            // gesture
            if(random() <= 0.1) botCmdBuffer->buttons |= BUTTON_GESTURE;
            
                if(self->targetNode == -1 || botTargetInRange(self, self->botEnemy)) {
                    botGetAimLocation(self->botEnemy, &tmpVec);
                    goToward(self, tmpVec, botCmdBuffer);
                    botAttackIfTargetInRange(self,self->botEnemy, botCmdBuffer);
                    //self->timeFoundNode = level.time;
                    #ifdef BOT_DEBUG
                    trap_SendServerCommand(-1,va("print \"Current Target Node %d\n\"", self->targetNode));
                    trap_SendServerCommand(-1,va("print \"botTargetInRange returns %d\n\"",(int)botTargetInRange(self, self->botEnemy)));
                    #endif
                    //reset this to avoid problems with the node timeouts (bots stuck between 2 nodes
                }
            
            // we already have an enemy. See if still in LOS or radar range for aliens
            if((!botTargetInRange(self,self->botEnemy) &&
                self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS) ||
                (Distance(self->s.pos.trBase, self->botEnemy->s.pos.trBase) > ALIENSENSE_RANGE &&
                self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS))
            {
                // if it's been over the maxium enemy chase time since we last saw him in LOS or radar then do nothing, else follow him!
                if(self->botEnemyLastSeen + BOT_ENEMY_CHASETIME < level.time) {
                    // forget him!
                    self->botEnemy = NULL;
                    self->botEnemyLastSeen = 0;
                    self->followingRoute = qfalse;
                } 
                
                
                
            } else {
                self->botEnemyLastSeen = level.time;
                #ifdef BOT_DEBUG
                trap_SendServerCommand(-1,va("print \"BotEnemylastSeen: %d\n\"", self->botEnemyLastSeen));
                #endif
            }
                G_BotMove( self, botCmdBuffer );
        }
        if(!self->botEnemy) {
            // try to find closest enemy
            tempEntityIndex = botFindClosestEnemy(self, attackTeam);
            if(tempEntityIndex >= 0) {
                self->botEnemy = &g_entities[tempEntityIndex];
                self->timeFoundEnemy = level.time;
            }
            self->followingRoute = qfalse;
        }
        
        // enable wallwalk
        if( BG_ClassHasAbility( self->client->ps.stats[STAT_PCLASS], SCA_WALLCLIMBER ) )
            botCmdBuffer->upmove = -1;
        // jetpack
        if( self->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS && BG_UpgradeIsActive(BG_FindUpgradeNumForName((char *)"jetpack"), self->botFriend->client->ps.stats))
            BG_ActivateUpgrade( BG_FindUpgradeNumForName((char *)"jetpack"), self->client->ps.stats );
        else
            BG_DeactivateUpgrade( BG_FindUpgradeNumForName((char *)"jetpack"), self->client->ps.stats );

}
void Follow( gentity_t *self, usercmd_t *botCmdBuffer ) {
    int distance = 0;
    int tooCloseDistance = 100;     // about 1/3 of turret range

    qboolean followFriend = qfalse;
    vec3_t tmpVec;      
    if(self->botFriend) {
        // see if our friend is in LOS or radar range (aliens)
        if((botTargetInRange(self,self->botFriend) &&
        self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS) ||
        (Distance(self->s.pos.trBase, self->botFriend->s.pos.trBase) < ALIENSENSE_RANGE &&
        self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS))
        {
            // go to him!
            followFriend = qtrue;
            self->botFriendLastSeen = level.time;
        } else {
            // if it's been over BOT_FRIEND_CHASETIME since we last saw him then do nothing, else follow him!
            if(self->botFriendLastSeen + BOT_FRIEND_CHASETIME < level.time )
                // forget him!
                followFriend = qfalse;
            else {
                followFriend = qtrue;
            }
        }

        if(followFriend) {
            distance = botGetDistanceBetweenPlayer(self, self->botFriend);
            botGetAimLocation(self->botFriend, &tmpVec);
            botSlowAim(self, tmpVec, self->aimSlowness, &tmpVec );
            botAimAtTarget(self,tmpVec , botCmdBuffer);

            // enable wallwalk
            if( BG_ClassHasAbility( self->client->ps.stats[STAT_PCLASS], SCA_WALLCLIMBER ) )
                botCmdBuffer->upmove = -1;
                                            // jetpack
            if( self->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS && BG_UpgradeIsActive(BG_FindUpgradeNumForName((char *)"jetpack"), self->botFriend->client->ps.stats))
                BG_ActivateUpgrade( BG_FindUpgradeNumForName((char*)"jetpack"), self->client->ps.stats );
            else
                BG_DeactivateUpgrade( BG_FindUpgradeNumForName((char *)"jetpack"), self->client->ps.stats );

            //botShootIfTargetInRange(self,self->botEnemy);
            if(distance>tooCloseDistance)
                G_BotMove( self, botCmdBuffer );
        }
    }
}

void Repair( gentity_t *self, usercmd_t *botCmdBuffer) {
    int tempEntityIndex = botFindDamagedFriendlyStructure(self);
    //if we have ckit and stuff is damaged
    if (BG_InventoryContainsWeapon( WP_HBUILD, self->client->ps.stats)) {
                
        self->botTarget = &g_entities[tempEntityIndex];

        //change to ckit if we havent already
        if (self->client->ps.weapon != WP_HBUILD)
            G_ForceWeaponChange( self, WP_HBUILD );
            
        //use waypoints to try to get to target
        if(self->botDest.ent != self->botTarget || self->botDest.ent == NULL) {
            self->botDest.ent = self->botTarget;
            VectorCopy(self->botTarget->s.pos.trBase, self->botDest.coord);
            findRouteToTarget(self, self->botDest.coord);
            setNewRoute(self);
        }
                
        //have reached end of path, continue towards buildable from here
        if( self->targetNode == -1) {
            goToward(self,self->botDest.coord, botCmdBuffer);
            //in case we get stuck, find a new route to the target and follow it
            if(self->timeFoundNode + 10000 < level.time) {
                findRouteToTarget(self, self->botDest.ent->s.pos.trBase);
                setNewRoute(self);
            }
        }
        if( botGetDistanceBetweenPlayer( self, self->botTarget ) <
            100 && botWillHitTarget(self,self->botTarget)) {
            // If we are within the distance of the structure, than we
            // start directly with repairing
            botCmdBuffer->buttons |= BUTTON_ATTACK2;
        } else {
            G_BotMove( self , botCmdBuffer);
        }
    }
}
void Heal( gentity_t *self, usercmd_t *botCmdBuffer) {
    int tempEntityIndex = botFindBuilding(self, BA_H_MEDISTAT, BOT_MEDI_RANGE);
    self->botTarget = &g_entities[tempEntityIndex];
     //use paths to try to get to target
    if(self->botDest.ent != self->botTarget || self->botDest.ent == NULL) {
            self->botDest.ent = self->botTarget;
            VectorCopy(self->botTarget->s.pos.trBase, self->botDest.coord);
            findRouteToTarget(self, self->botDest.ent->s.pos.trBase);
            setNewRoute(self);
    }
            
    //have reached end of path, continue towards medi until reached
    if( self->targetNode == -1) {
        goToward(self, self->botDest.coord, botCmdBuffer);
        //in case we get stuck, find a new route to the target and follow it
        if(self->timeFoundNode + 10000 < level.time) {
            findRouteToTarget(self, self->botDest.ent->s.pos.trBase);
            setNewRoute(self);
        }
    }
     if(botGetDistanceBetweenPlayer(self,self->botTarget) > 50) {
            G_BotMove(self, botCmdBuffer);
     }
}

int botFindBuilding(gentity_t *self, int buildingType, int range) {
    // The range of our scanning field.
    //int vectorRange = MGTURRET_RANGE * 5;
    // vectorRange converted to a vector
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
    int closestBuilding = -1;
    float newDistance;
    gentity_t *target;
    if(range != -1) {
        VectorSet( vectorRange, range, range, range );
        VectorAdd( self->client->ps.origin, vectorRange, maxs );
        VectorSubtract( self->client->ps.origin, vectorRange, mins );
        total_entities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);
        for( i = 0; i < total_entities; ++i )
        {
            target = &g_entities[entityList[ i ] ];
            
            if( target->s.eType == ET_BUILDABLE && target->s.modelindex == buildingType && (target->biteam == PTE_ALIENS || target->powered)) {
                newDistance = DistanceSquared( self->s.pos.trBase, target->s.pos.trBase );
                if( newDistance < minDistance|| minDistance == -1) {
                    minDistance = newDistance;
                    closestBuilding = entityList[i];
                }
            }
            
        }
    } else {
        for( i = 0; i < MAX_GENTITIES; ++i )
        {
            target = &g_entities[i];
            
            if( target->s.eType == ET_BUILDABLE && target->s.modelindex == buildingType && (target->biteam == PTE_ALIENS || target->powered)) {
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
            
            if( sq && G_BotCheckForSpawningPlayers( self ))
            {
                G_RemoveFromSpawnQueue( sq, self->s.number );
                G_PushSpawnQueue( sq, self->s.number );
            }
        }
        return;
    }
    //reset state to FINDNEWPATH
    self->state = FINDNEWNODE;
    
    //reset botEnemy to NULL if we can roam (for fallback reasons to old behavior)
    if(g_bot_roam.integer == 1)
        self->botEnemy = NULL;
    
    self->followingRoute = qfalse;
   // self->targetNode = -1;
    self->botDest.ent = NULL;
    
    if( self->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        int teamnum = self->client->pers.teamSelection;
        int clientNum = self->client->ps.clientNum;

        if( teamnum == PTE_HUMANS ) {
            self->client->pers.classSelection = PCL_HUMAN;
            self->client->ps.stats[STAT_PCLASS] = PCL_HUMAN;

            self->client->pers.humanItemSelection = self->spawnItem;


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
    
    if( G_GetSpawnQueueLength( sq ) )
    {
        do
        {
            if( !(g_entities[ sq->clients[ i ] ].r.svFlags & SVF_BOT))
            {
                if( i < sq->front )
                    lastPlayerPos = i + MAX_CLIENTS - sq->front;
                else
                    lastPlayerPos = i - sq->front;
            }
            
            if( sq->clients[ i ] == self->s.number )
            {
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
void botGetAimLocation( gentity_t *target, vec3_t *aimLocation) {
    //vec3_t aimLocation;
    //get the position of the enemy
    VectorCopy( target->s.pos.trBase, *aimLocation);
    
    if(target->client && target->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS)
        (*aimLocation)[2] += target->r.maxs[2] * 0.85;
   // return aimLocation;
}

void botAimAtTarget( gentity_t *self, vec3_t target, usercmd_t *rAngles)
{
        vec3_t aimVec, oldAimVec, aimAngles;
        vec3_t refNormal, grapplePoint, xNormal, viewBase;
        //vec3_t highPoint;
        float turnAngle;
        int i;
        vec3_t forward;

        if( ! (self && self->client) )
                return;
        
       //botSlowAim( self, target, (float) self->botSkillLevel / 20, &tmpVec);
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

        if(self->client->ps.stats[ STAT_STATE ] & SS_WALLCLIMBINGCEILING )
        {
                //NOTE: the grapplePoint is here not an inverted refNormal :(
                RotatePointAroundVector( aimVec, grapplePoint, oldAimVec, -180.0);
        }
        else if( turnAngle != 0.0f)
                RotatePointAroundVector( aimVec, xNormal, oldAimVec, -turnAngle);

        vectoangles( aimVec, aimAngles );

        VectorSet(self->client->ps.delta_angles, 0.0f, 0.0f, 0.0f);

        for( i = 0; i < 3; i++ )
        {
                aimAngles[i] = ANGLE2SHORT(aimAngles[i]);
        }

        AngleVectors( self->client->ps.viewangles, forward, NULL, NULL);
        //save bandwidth
        SnapVector(aimAngles);
        rAngles->angles[0] = aimAngles[0];
        rAngles->angles[1] = aimAngles[1];
        rAngles->angles[2] = aimAngles[2];
}
void botShakeAim( gentity_t *self, vec3_t target, vec3_t *rVec )
{
    vec3_t aim;
    vec3_t forward, right, up;
    float len, speedAngle;
    
    AngleVectors( self->client->ps.viewangles, forward, right, up);
    VectorSubtract(target, self->s.origin, aim);
    len = VectorLength(aim)/1000;
    VectorNormalize(aim);
    speedAngle = RAD2DEG( acos( DotProduct( forward, aim ) ) )/100;
    VectorScale(up, speedAngle * len * crandom() * self->aimShake, up);
    VectorScale(right, speedAngle * len * crandom() * self->aimShake, right);
    VectorAdd(target, up, *rVec);
    VectorAdd(*rVec, up, *rVec);
}
int botFindClosestEnemy( gentity_t *self, qboolean includeTeam ) {
    // return enemy entity index, or -1
    int vectorRange = MGTURRET_RANGE * 3;
    int i;
    int total_entities;
    int entityList[ MAX_GENTITIES ];
    float minDistance = (float) Square(MGTURRET_RANGE * 3);
    int closestTarget = -1;
    float newDistance;
    vec3_t range;
    vec3_t mins, maxs;
    gentity_t *target;
    VectorSet( range, vectorRange, vectorRange, vectorRange );
    VectorAdd( self->client->ps.origin, range, maxs );
    VectorSubtract( self->client->ps.origin, range, mins );
    SnapVector(mins);
    SnapVector(maxs);
    total_entities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
    
    for( i = 0; i < total_entities; ++i )
    {
        target = &g_entities[entityList[ i ] ];
        //DistanceSquared for performance reasons (doing sqrt constantly is bad and keeping it squared does not change result)
        newDistance = (float) DistanceSquared( self->s.pos.trBase, target->s.pos.trBase );
        //if entity is closer than previous stored one and the target is alive
	if( newDistance < minDistance && target->health > 0) {
	    
	    //if we can see the entity OR we are on aliens (who dont care about LOS because they have radar)
	    if( (self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS ) || botTargetInRange(self, target) ){
		
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
		    if( target->client->ps.stats[STAT_PTEAM] != self->client->ps.stats[STAT_PTEAM] || includeTeam ) {
			
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

// really an int? what if it's too long?
int botGetDistanceBetweenPlayer( gentity_t *self, gentity_t *player ) {
    return Distance( self->s.pos.trBase, player->s.pos.trBase );
}
qboolean botPathIsBlocked( gentity_t *self ) {
    trace_t trace;
    gentity_t *traceEnt;
    gentity_t *target;
    vec3_t  forward, right, up;
    vec3_t end;
    vec3_t mins, maxs;
    vec3_t muzzle;
    
    if(self->botEnemy)
        target = self->botEnemy;
    else
        target = self->botTarget;
    
    BG_FindBBoxForClass( self->client->ps.stats[ STAT_PCLASS ],
                       mins, maxs, NULL, NULL, NULL );
    AngleVectors( self->client->ps.viewangles, forward, right, up );
    CalcMuzzlePoint( self, forward, right, up, muzzle );
    VectorMA( muzzle, 10, forward, end );
    //save bandwidth
    VectorScale(mins, 0.8, mins);
    VectorScale(maxs,0.8, maxs);
    SnapVector(end);
    SnapVector(mins);
    SnapVector(maxs);
    trap_Trace( &trace, muzzle, mins, maxs, end, self->s.number, MASK_SHOT );
    traceEnt = &g_entities[ trace.entityNum ];
    
    if(trace.fraction < 1.0 && trace.entityNum != -1)
    {return qtrue;}
        else
    {return qfalse;}
}
//TODO: refactoring
//use self->s.weapons ??? has both aliens and humans so no need to check for teams
qboolean botAttackIfTargetInRange( gentity_t *self, gentity_t *target, usercmd_t *botCmdBuffer ) {
    if(botTargetInRange(self,target)) {
        if( self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS )
        {
            switch(self->client->ps.stats[STAT_PCLASS]) {
                case PCL_ALIEN_BUILDER0:
                    botCmdBuffer->buttons |= BUTTON_GESTURE;
                    break;
                case PCL_ALIEN_BUILDER0_UPG:
                    if (random() > 0.3)
                        botCmdBuffer->buttons |= BUTTON_ATTACK2;
                    else
                        botCmdBuffer->buttons |= BUTTON_USE_HOLDABLE;
                    break;
                case PCL_ALIEN_LEVEL0:
                    break; //nothing, auto hit
                case PCL_ALIEN_LEVEL1:
                    if(botWillHitTarget(self, target))
                        botCmdBuffer->buttons |= BUTTON_ATTACK;
                    break;
                case PCL_ALIEN_LEVEL1_UPG:
                    if (random() > 0.5)
                        botCmdBuffer->buttons |= BUTTON_ATTACK2; //gas
                    else if(botWillHitTarget( self, target ))
                        botCmdBuffer->buttons |= BUTTON_ATTACK;
                    break;
                case PCL_ALIEN_LEVEL2:
                    if(botWillHitTarget(self, target))
                        botCmdBuffer->buttons |= BUTTON_ATTACK;
                    break;
                case PCL_ALIEN_LEVEL2_UPG:
                    if (Distance( self->s.pos.trBase, target->s.pos.trBase ) > LEVEL2_CLAW_RANGE)
                        botCmdBuffer->buttons |= BUTTON_ATTACK2; //zap
                    else if(botWillHitTarget( self, target ))
                        botCmdBuffer->buttons |= BUTTON_ATTACK;
                    break;
                case PCL_ALIEN_LEVEL3:
                    if(Distance( self->s.pos.trBase, target->s.pos.trBase ) > 150 && 
                        self->client->ps.stats[ STAT_MISC ] < LEVEL3_POUNCE_SPEED)
                        botCmdBuffer->buttons |= BUTTON_ATTACK2; //pounce
                    else if(botWillHitTarget( self, target))
                        botCmdBuffer->buttons |= BUTTON_ATTACK;
                    break;
                case PCL_ALIEN_LEVEL3_UPG:
                    if(self->client->ps.ammo[WP_ALEVEL3_UPG] > 0 && 
                        Distance( self->s.pos.trBase, target->s.pos.trBase ) > 150 )
                        botCmdBuffer->buttons |= BUTTON_USE_HOLDABLE; //barb
                    else
                    {       
                        if(Distance( self->s.pos.trBase, target->s.pos.trBase ) > 150 && 
                            self->client->ps.stats[ STAT_MISC ] < LEVEL3_POUNCE_UPG_SPEED)
                            botCmdBuffer->buttons |= BUTTON_ATTACK2; //pounce
                        else if(botWillHitTarget(self, target))
                            botCmdBuffer->buttons |= BUTTON_ATTACK;
                    }
                    break;
                case PCL_ALIEN_LEVEL4:
                    if (Distance( self->s.pos.trBase, target->s.pos.trBase ) > LEVEL4_CLAW_RANGE)
                        botCmdBuffer->buttons |= BUTTON_ATTACK2; //charge
                    else if(botWillHitTarget(self, target))
                        botCmdBuffer->buttons |= BUTTON_ATTACK;
                    break;
                default: break; //nothing
            }
                    
        } else if( self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS ) {
            if(self->client->ps.weapon == WP_FLAMER)
            {
                //only fire in range
                if(Distance( self->s.pos.trBase, target->s.pos.trBase ) < 200)
                    botCmdBuffer->buttons |= BUTTON_ATTACK;
                
            } else if( self->client->ps.weapon == WP_LUCIFER_CANNON ) {
                
                if( self->client->time10000 % 2000 ) {
                    botCmdBuffer->buttons |= BUTTON_ATTACK;
                    self->isFireing = qtrue;
                }
                    
                    
            } else
            botCmdBuffer->buttons |= BUTTON_ATTACK; //just fire
            
        }

        return qtrue;
    }
    return qfalse;
}

qboolean botTargetInRange( gentity_t *self, gentity_t *target ) {
    trace_t trace;
    gentity_t *traceEnt;
    float width;
    float range;
    vec3_t    mins, maxs;
    vec3_t  muzzle;
    vec3_t  forward, right, up;
    vec3_t targetBase;
    if( !self || !target )
        return qfalse;

    if( !self->client || !target->client )
        if( target->s.eType != ET_BUILDABLE )
            return qfalse;

    if( target->client->ps.stats[ STAT_STATE ] & SS_HOVELING )
        return qfalse;

    if( target->health <= 0 )
        return qfalse;
    // set aiming directions
    AngleVectors( self->client->ps.viewangles, forward, right, up );

    CalcMuzzlePoint( self, forward, right, up, muzzle );
    getWeaponAttributes( self, &range, &width);
    VectorCopy(target->s.pos.trBase, targetBase);
    SnapVector(targetBase);
    
    if(width > 0) {
            VectorSet( mins, -width, -width, -width );
            VectorSet( maxs, width, width, width );
            trap_Trace( &trace, muzzle, mins, maxs, targetBase, self->s.number, MASK_SHOT );
    } else
        trap_Trace( &trace, muzzle, NULL, NULL, targetBase, self->s.number, MASK_SHOT );

    

    // check if we hit a wall
    if( trace.surfaceFlags & SURF_NOIMPACT )
        return qfalse;

    traceEnt = &g_entities[ trace.entityNum ];
        
    //target is in range
    if( traceEnt == target )
        return qtrue;
    return qfalse;
}

qboolean botWillHitTarget( gentity_t *self, gentity_t *target ) {
    trace_t trace;
    gentity_t *traceEnt;
    float width;
    float range;
    vec3_t    end;
    vec3_t    mins, maxs;
    vec3_t  muzzle;
    vec3_t  forward, right, up;
    if( !self || !target )
        return qfalse;

    if( !self->client || !target->client )
        if( target->s.eType != ET_BUILDABLE )
            return qfalse;

    if( target->client->ps.stats[ STAT_STATE ] & SS_HOVELING )
        return qfalse;

    if( target->health <= 0 )
        return qfalse;
        // set aiming directions
        AngleVectors( self->client->ps.viewangles, forward, right, up );

        CalcMuzzlePoint( self, forward, right, up, muzzle );

        getWeaponAttributes(self,&range,&width);
        VectorMA( muzzle, range, forward, end );
        //save bandwidth
        SnapVector(end);
        G_UnlaggedOn( self, muzzle, range );
        if(width > 0) {
            VectorSet( mins, -width, -width, -width );
            VectorSet( maxs, width, width, width );
            SnapVector(maxs);
            SnapVector(mins);
            trap_Trace( &trace, muzzle, mins, maxs, end, self->s.number, MASK_SHOT );
        } else
            trap_Trace( &trace, muzzle, NULL, NULL, end, self->s.number, MASK_SHOT );
        G_UnlaggedOff( );

        // check if we hit a wall
        if( trace.surfaceFlags & SURF_NOIMPACT )
            return qfalse;

        traceEnt = &g_entities[ trace.entityNum ];
        
        //target is in range
        if( traceEnt == target )
            return qtrue;
        return qfalse;
}
//TODO: use self->s.weapon instead (has both aliens and humans no need to check for a team)
void getWeaponAttributes( gentity_t *self, float *range, float *width)
{
    
    if(self->client->ps.stats[STAT_PTEAM] == PTE_ALIENS) {
        
        //find the range and width of alien attack
        switch(self->client->ps.stats[STAT_PCLASS]) {
            case PCL_ALIEN_BUILDER0:
                *range = 0;
                *width = 0;
                break;
            case PCL_ALIEN_BUILDER0_UPG:
                *range =   ABUILDER_CLAW_RANGE;
                *width = ABUILDER_CLAW_WIDTH;
                break;
            case PCL_ALIEN_LEVEL0:
                *range = LEVEL0_BITE_RANGE;
                *width = LEVEL0_BITE_WIDTH;
                break;
            case PCL_ALIEN_LEVEL1:
                *range = LEVEL1_CLAW_RANGE;
                *width = LEVEL1_CLAW_WIDTH;
                break;
            case PCL_ALIEN_LEVEL1_UPG:
                *range = LEVEL1_CLAW_RANGE;
                *width = LEVEL1_CLAW_WIDTH;
                break;
            case PCL_ALIEN_LEVEL2:
                *range = LEVEL2_CLAW_RANGE;
                *width = LEVEL2_CLAW_WIDTH;
                break;
            case PCL_ALIEN_LEVEL2_UPG:
                *range = LEVEL2_CLAW_RANGE;
                *width = LEVEL2_CLAW_WIDTH;
                break;
            case PCL_ALIEN_LEVEL3:
                *range = LEVEL3_CLAW_RANGE;
                *width = LEVEL3_CLAW_WIDTH;
                break;
            case PCL_ALIEN_LEVEL3_UPG:
                *range = LEVEL3_CLAW_RANGE;
                *width = LEVEL3_CLAW_WIDTH;
                break;
            case PCL_ALIEN_LEVEL4:
                *range = LEVEL4_CLAW_RANGE;
                *width = LEVEL4_CLAW_WIDTH;
                break;
            default: break;
        }
        
    } else if(self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS) {
        switch(self->client->ps.weapon) {
          case WP_PAIN_SAW:
            *range = PAINSAW_RANGE;
          case WP_FLAMER:
            *range = FLAMER_LIFETIME * FLAMER_SPEED / 1000;
          default: 
            *range = 8192 * 16;
        }
        *width = 0;
    }
}
//Begin node/waypoint/path functions
int distanceToTargetNode( gentity_t *self )
{
        return (int) Distance(level.nodes[self->targetNode].coord, self->s.pos.trBase);
}
void botSlowAim( gentity_t *self, vec3_t target, float slow, vec3_t *rVec)
{
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
        slowness = slow*(FRAMETIME/1000.0);
        if(slowness > 1.0) slowness = 1.0;
        
        VectorLerp( slowness, forward, aimVec, skilledVec);
        
        VectorAdd(viewBase, skilledVec, *rVec);
}

int findClosestNode( vec3_t start )
{
        trace_t trace;
        int i = 0;
        float distance = 0;
        int closestNode = 0;
        float closestNodeDistance = Square(2000);
        qboolean nodeFound = qfalse;
        for(i = 0; i < level.numNodes; i++) //find a nearby path that wasn't used before
        {
                trap_Trace( &trace, start, NULL, NULL, level.nodes[i].coord, ENTITYNUM_NONE, MASK_DEADSOLID );
                if( trace.fraction < 1.0 )
                {continue;}
                //using distanceSquared for performance reasons (sqrt is expensive and this gives same result here)
                distance = (float) DistanceSquared(level.nodes[i].coord,start);
                if(distance < Square(5000))
                {
                        if(closestNodeDistance > distance)
                        {
                                closestNode = i;
                                closestNodeDistance = distance;
                                nodeFound = qtrue;
                        }
                }
        }
        if(nodeFound == qtrue)
        {
                return closestNode;
        }
        else
        {
                
                return -1;
        }
}
void findNewNode( gentity_t *self, usercmd_t *botCmdBuffer) {
    
    
    int closestNode = findClosestNode(self->s.pos.trBase);
    self->lastNodeID = -1;
    if(closestNode != -1) {
    self->targetNode = closestNode;
    self->timeFoundNode = level.time;
    self->state = TARGETNODE;
    } else {
        self->state = LOST;
        botCmdBuffer->forwardmove = 0;
        botCmdBuffer->upmove = -1;
        botCmdBuffer->rightmove = 0;
        botCmdBuffer->buttons = 0;
        botCmdBuffer->buttons |= BUTTON_GESTURE;
    }
}
    
void findNextNode( gentity_t *self )
{
        int randnum = 0;
        int i,nextNode = 0;
        int possibleNextNode = 0;
        int possibleNodes[5];
        int lasttarget = self->targetNode;
        possibleNodes[0] = possibleNodes[1] = possibleNodes[2] = possibleNodes[3] = possibleNodes[4] = 0;
        if(!self->followingRoute) {
            for(i = 0; i < 5; i++)
            {
                    if(level.nodes[self->targetNode].nextid[i] < level.numNodes &&
                            level.nodes[self->targetNode].nextid[i] >= 0)
                    {
                            if(self->lastNodeID >= 0)
                            {
                                    if(self->lastNodeID == level.nodes[self->targetNode].nextid[i])
                                    {
                                            continue;
                                    }
                            }
                            possibleNodes[possibleNextNode] = level.nodes[self->targetNode].nextid[i];
                            possibleNextNode++;
                    }
            }
            if(possibleNextNode == 0)
            {       
                    self->state = FINDNEWNODE;
                    return;
            }
            else
            {
                    self->state = TARGETNODE;
                    if(level.nodes[self->targetNode].random < 0)
                    {
                            nextNode = 0;
                    }
                    else
                    {
                            srand( trap_Milliseconds( ) );
                            randnum = (int)(( (double)rand() / ((double)(RAND_MAX)+(double)(1)) ) * possibleNextNode);
                            nextNode = randnum;
                            //if(nextpath == possiblenextpath)
                            //{nextpath = possiblenextpath - 1;}
                    }
                    self->lastNodeID = self->targetNode;
                    self->targetNode = possibleNodes[nextNode];
                    for(i = 0;i < 5;i++)
                    {
                            if(level.nodes[self->targetNode].nextid[i] == lasttarget)
                            {
                                    i = 5;
                            }
                    }

                    self->timeFoundNode = level.time;
                    return;
            }
        } else {
            self->targetNode = self->routeToTarget[self->targetNode];
            self->timeFoundNode = level.time;
            self->state = TARGETNODE;
        }
            
        return;
}
void pathfinding( gentity_t *self, usercmd_t *botCmdBuffer )
{
    vec3_t tmpVec;
    //bot path stuff
    //if( !botTargetInRange(self,self->botEnemy)) {
        switch(self->state)
        {
                case FINDNEWNODE: findNewNode(self, botCmdBuffer); break;
                case FINDNEXTNODE: findNextNode(self); break;
                case TARGETNODE:break; //done in G_FrameThink
                case LOST: findNewNode(self, botCmdBuffer);break; //LOL :(
                case TARGETOBJECTIVE: break;
                default: break;
        }
   //}
    if(self->state == TARGETNODE && (self->followingRoute || g_bot_roam.integer == 1))
        {
                #ifdef BOT_DEBUG
                trap_SendServerCommand(-1,va("print \"Now Targeting Node %d\n\"", self->targetNode));
                #endif
                botSlowAim(self, level.nodes[self->targetNode].coord, 0.5, &tmpVec);
                botAimAtTarget(self, tmpVec, botCmdBuffer);
                G_BotMove( self, botCmdBuffer );
                if(self->lastNodeID >= 0 )
                {
                        switch(level.nodes[self->lastNodeID].action)
                        {
                                case BOT_JUMP:  
                                    
                                    if( self->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS && 
                                        self->client->ps.stats[ STAT_STAMINA ] < 0 )
                                    {break;}
                                    if( !BG_ClassHasAbility( self->client->ps.stats[ STAT_PCLASS ], SCA_WALLCLIMBER ) )
                                        
                                        botCmdBuffer->upmove = 20;
                                    
                                    
                                    break;
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
                                    self->client->ps.stats[ STAT_MISC ] < LEVEL3_POUNCE_SPEED)
                                    botCmdBuffer->buttons |= BUTTON_ATTACK2;
                                    else if(self->client->ps.stats[STAT_PCLASS] == PCL_ALIEN_LEVEL3_UPG && 
                                        self->client->ps.stats[ STAT_MISC ] < LEVEL3_POUNCE_UPG_SPEED)
                                        botCmdBuffer->buttons |= BUTTON_ATTACK2;
                                    break;
                                default: break;
                        }
                        if(level.time - self->timeFoundNode > level.nodes[self->lastNodeID].timeout)
                        {
                                self->state = FINDNEWNODE;
                                self->timeFoundNode = level.time;
                        }
                }
                else if( level.time - self->timeFoundNode > 10000 )
                {
                        self->state = FINDNEWNODE;
                        self->timeFoundNode = level.time;
                }
                if(distanceToTargetNode(self) < 70)
                {
                        self->state = FINDNEXTNODE;
                        self->timeFoundNode = level.time;
                }
                //if we hvae reached the end of the route
                if(self->targetNode == -1 && self->followingRoute) {
                    self->followingRoute = qfalse; //say we are no longer following the route
                }

        }
}
void goToward(gentity_t *self, vec3_t target, usercmd_t *botCmdBuffer) {
    vec3_t tmpVec;
    self->state = TARGETOBJECTIVE;
    //botGetAimLocation(target, &tmpVec);
    botShakeAim(self,target,&tmpVec);
    botSlowAim(self, tmpVec, self->aimSlowness, &tmpVec );
    botAimAtTarget(self,tmpVec , botCmdBuffer);
    self->followingRoute = qfalse; //we are not following a route, we are going straight for the target
    
    
}
void setNewRoute(gentity_t *self) { 
    //findRouteToTarget(self, self->botDest.coord);
    self->timeFoundNode = level.time;
    self->lastNodeID = self->targetNode;
    self->targetNode = self->startNode;
    self->state = TARGETNODE;
    self->followingRoute = qtrue;
}
void findRouteToTarget( gentity_t *self, vec3_t dest ) {
    long shortdist[MAX_NODES];
    short i;
    short k;
    int mini;
    short visited[MAX_NODES];
    
    //make startNum -1 so if there is no node close to us, we will not use the path
    short startNum = -1;
    short endNum = -1;
    vec3_t start = {0,0,0}; 
    vec3_t end = {0,0,0};
    vec3_t  forward, right, up;
    vec3_t mins,maxs;
    trace_t trace;
    
    AngleVectors( self->client->ps.viewangles, forward, right, up );
    CalcMuzzlePoint( self, forward, right, up, start);
    
    VectorCopy( dest, end);

    for( i=0;i<MAX_NODES;i++) {
        shortdist[i] = 99999999;
        self->routeToTarget[i] = -1;
        visited[i] = 0;
    }
    startNum = findClosestNode(start);
    endNum = findClosestNode(end);
    if( endNum == -1)
    {
        endNum = 0; //so we dont crash
        startNum = -1; //so we dont pathfind
    }
    shortdist[endNum] = 0;
    for (k = 0; k <= MAX_NODES; ++k) {
        mini = -1;
        for (i = 0; i <= MAX_NODES; ++i) {
            if (!visited[i] && ((mini == -1) || (shortdist[i] < shortdist[mini])))
                mini = i;
        }

        visited[mini] = 1;

        for (i = 0; i <= MAX_NODES; ++i) {
            if (level.distNode[mini][i]) {
                if (shortdist[mini] + level.distNode[mini][i] < shortdist[i]) {
                    shortdist[i] = shortdist[mini] + level.distNode[mini][i];
                    self->routeToTarget[i] = mini;
                }
            }
        }
    }
        self->lastRouteSearch = level.time;
        //check for -1 to keep from crashing
        if(self->routeToTarget[startNum] == -1) {
            self->startNode = startNum;
        } else {
            BG_FindBBoxForClass( self->client->ps.stats[ STAT_PCLASS ],
                                mins, maxs, NULL, NULL, NULL );
            VectorScale(maxs,.5,maxs);
            VectorScale(mins,.5,mins);
            
            //do a trace to the second node in the route to see if we can skip the first (and thus not backtrack if we were going to)
            trap_Trace( &trace, start, mins, maxs, level.nodes[self->routeToTarget[startNum]].coord, self->s.number, MASK_SHOT );
            
            //we can get to that node
            if(trace.fraction == 1) 
                self->startNode = self->routeToTarget[startNum];
            else
                self->startNode = startNum;
        }
        
}
    
