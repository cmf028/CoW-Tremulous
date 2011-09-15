/*
   ===========================================================================
   Copyright (C) 2007 Amine Haddad

 ** This file is modified by Thomas Rinsma, I take no copyright at all

   This file is part of Tremulous.

   The original works of vcxzet (lamebot3) were used a guide to create TremBot.

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

int G_BotBuyUpgrade ( gentity_t *ent, int upgrade )
{
    //already got this?
    if( BG_InventoryContainsUpgrade( upgrade, ent->client->ps.stats ) )
        return 0;

    //can afford this?
    if( BG_FindPriceForUpgrade( upgrade ) > (short)ent->client->ps.persistant[ PERS_CREDIT ] )
        return 0;

    //have space to carry this?
    if( BG_FindSlotsForUpgrade( upgrade ) & ent->client->ps.stats[ STAT_SLOTS ] )
        return 0;

    //are we /allowed/ to buy this?
    if( !BG_FindPurchasableForUpgrade( upgrade ) )
        return 0;

    //are we /allowed/ to buy this?
    if( !BG_FindStagesForUpgrade( upgrade, g_humanStage.integer ) || !BG_UpgradeIsAllowed( upgrade ) )
        return 0;

    if( upgrade == UP_AMMO )
        G_GiveClientMaxAmmo( ent, qfalse );
    else
        //add to inventory
        BG_AddUpgradeToInventory( upgrade, ent->client->ps.stats );

    if( upgrade == UP_BATTPACK )
        G_GiveClientMaxAmmo( ent, qtrue );

    //subtract from funds
    G_AddCreditToClient( ent->client, -(short)BG_FindPriceForUpgrade( upgrade ), qfalse );

    return 1;
}
int G_BotBuy ( gentity_t *ent, int weapon )
{
    int maxAmmo, maxClips;

    //already got this?
    if( BG_InventoryContainsWeapon( weapon, ent->client->ps.stats ) )
        return 0;

    //can afford this?
    if( BG_FindPriceForWeapon( weapon ) > (short)ent->client->ps.persistant[ PERS_CREDIT ] )
        return 0;

    //have space to carry this?
    if( BG_FindSlotsForWeapon( weapon ) & ent->client->ps.stats[ STAT_SLOTS ] )
        return 0;

    //are we /allowed/ to buy this?
    if( !BG_FindPurchasableForWeapon( weapon ) )
        return 0;

    //are we /allowed/ to buy this?
    if( !BG_FindStagesForWeapon( weapon, g_humanStage.integer ) || !BG_WeaponIsAllowed( weapon ) )
        return 0;
    
    //does server allow us to buy this?
    switch(weapon) {
        case WP_MACHINEGUN:
            if(g_bot_rifle.integer == 0)
                return 0;
            break;
        case WP_PAIN_SAW:
            if(g_bot_psaw.integer == 0)
                return 0;
            break;
        case WP_SHOTGUN:
            if(g_bot_shotgun.integer == 0)
                return 0;
            break;
        case WP_LAS_GUN:
            if(g_bot_las.integer == 0)
                return 0;
            break;
        case WP_MASS_DRIVER:
            if(g_bot_mass.integer == 0)
                return 0;
            break;
        case WP_CHAINGUN:
            if(g_bot_chain.integer == 0)
                return 0;
            break;
        case WP_PULSE_RIFLE:
            if(g_bot_pulse.integer == 0)
                return 0;
            break;
        case WP_FLAMER:
            if(g_bot_flamer.integer == 0)
                return 0;
            break;
        case WP_LUCIFER_CANNON:
            if(g_bot_luci.integer == 0)
                return 0;
            break;
        default: break;
    }
            

    //add to inventory
    BG_AddWeaponToInventory( weapon, ent->client->ps.stats );
    BG_FindAmmoForWeapon( weapon, &maxAmmo, &maxClips );

    BG_PackAmmoArray( weapon, ent->client->ps.ammo, ent->client->ps.powerups,
                      maxAmmo, maxClips );

    G_ForceWeaponChange( ent, weapon );

    //set build delay/pounce etc to 0
    ent->client->ps.stats[ STAT_MISC ] = 0;

    //subtract from funds
    G_AddCreditToClient( ent->client, -(short)BG_FindPriceForWeapon( weapon ), qfalse );


    return 1;

}
void Buy( gentity_t *self, usercmd_t *botCmdBuffer )
{
    int i;
    // if bot buying is enabled
    if(g_bot_buy.integer > 0) {
        // armoury in range
        if(G_BuildableRange( self->client->ps.origin, 100, BA_H_ARMOURY) && 
            self->client->ps.weapon != WP_HBUILD && 
            self->client->ps.stats[STAT_PTEAM] == PTE_HUMANS &&
            self->client->time10000 % 2000 == 0 ) {

            if((short)self->client->ps.persistant[ PERS_CREDIT ] > 0 || self->client->ps.weapon == WP_BLASTER) {
                
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
                
                
                // buy the stuff the friend has
                if(self->botMind->botFriend) {
                    if( BG_InventoryContainsUpgrade( UP_JETPACK, self->botMind->botFriend->client->ps.stats ))
                        G_BotBuyUpgrade( self, UP_JETPACK );
                    
                    else if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, self->botMind->botFriend->client->ps.stats ))
                        G_BotBuyUpgrade( self, UP_BATTLESUIT );
                    
                    else if( BG_InventoryContainsUpgrade( UP_LIGHTARMOUR, self->botMind->botFriend->client->ps.stats ) && random() <= 0.2)
                        G_BotBuyUpgrade( self, UP_LIGHTARMOUR);
                    
                    else if( BG_InventoryContainsUpgrade( UP_HELMET, self->botMind->botFriend->client->ps.stats ))
                        G_BotBuyUpgrade( self, UP_HELMET);
                    
                }
                G_BotBuyUpgrade( self, UP_HELMET);
                G_BotBuyUpgrade( self, UP_LIGHTARMOUR);
                
                // buy most expensive first, then one cheaper, etc, dirty but working way
                if( !G_BotBuy( self, WP_LUCIFER_CANNON ) )
                    if( !G_BotBuy( self, WP_FLAMER ) )
                        if( !G_BotBuy( self, WP_PULSE_RIFLE ) )
                            if( !G_BotBuy( self, WP_CHAINGUN ) )
                                if( !G_BotBuy( self, WP_MASS_DRIVER ) )
                                    if( !G_BotBuy( self, WP_LAS_GUN ) )
                                        if( !G_BotBuy( self, WP_SHOTGUN ) )
                                            if( !G_BotBuy( self, WP_PAIN_SAW ) )
                                                G_BotBuy( self, WP_MACHINEGUN );
            }
            
            
            if( BG_FindUsesEnergyForWeapon( self->client->ps.weapon )) {
                G_BotBuyUpgrade( self, UP_BATTPACK );
            }else {
                G_BotBuyUpgrade( self, UP_AMMO );
            }
            
        } else if( botFindBuilding(self, BA_H_ARMOURY, BOT_ARM_RANGE) != -1 ) {
            self->botMind->botTarget = &g_entities[botFindBuilding(self, BA_H_ARMOURY, BOT_ARM_RANGE)];
            //use paths to try to get to target
            if(self->botMind->botDest.ent != self->botMind->botTarget || self->botMind->botDest.ent == NULL) {
                self->botMind->botDest.ent = self->botMind->botTarget;
                VectorCopy(self->botMind->botTarget->s.pos.trBase, self->botMind->botDest.coord);
                findRouteToTarget(self, self->botMind->botDest.coord);
                setNewRoute(self);
            }
            
            //have reached end of path, continue towards arm until reached
            if( self->botMind->targetNode == -1) {
                goToward(self, self->botMind->botDest.coord, botCmdBuffer);
                
                //find and follow a new path if we get stuck
                if(self->botMind->timeFoundNode + 10000 < level.time) {
                    findRouteToTarget(self, self->botMind->botDest.ent->s.pos.trBase);
                    setNewRoute(self);
                }
            }
            if(botGetDistanceBetweenPlayer(self,self->botMind->botTarget) > 100) {
                G_BotMove(self, botCmdBuffer);
            }
            if(self->client->ps.weapon == WP_HBUILD)
                G_ForceWeaponChange(self, WP_BLASTER);
        }
    }
}
int botFindArmoury(gentity_t *self) {
    // The range of our scanning field.
    int vectorRange = MGTURRET_RANGE * 5;
    // vectorRange converted to a vector
    vec3_t range;
    // Lower bound vector
    vec3_t mins;
    // Upper bound vector
    vec3_t maxs;
    // Indexing field
    int total_entities;
    int entityList[ MAX_GENTITIES ];
    int i;
    int min_distance = MGTURRET_RANGE * 5;
    int closest_arm = -1;
    int new_distance;
    gentity_t *target;
    VectorSet( range, vectorRange, vectorRange, vectorRange );
    VectorAdd( self->client->ps.origin, range, maxs );
    VectorSubtract( self->client->ps.origin, range, mins );
    total_entities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);
    for( i = 0; i < total_entities; ++i )
    {
        target = &g_entities[entityList[ i ] ];

        if( target->s.eType == ET_BUILDABLE && target->s.modelindex == BA_H_ARMOURY && target->powered) {
            new_distance = botGetDistanceBetweenPlayer( self, target );
            if( new_distance < min_distance ) {
                min_distance = new_distance;
                closest_arm = entityList[i];
            }
        }

    }
        return closest_arm;
}
int botFindMedistat(gentity_t *self) {
    // The range of our scanning field.
    int vectorRange = MGTURRET_RANGE * 5;
    // vectorRange converted to a vector
    vec3_t range;
    // Lower bound vector
    vec3_t mins;
    // Upper bound vector
    vec3_t maxs;
    // Indexing field
    int total_entities;
    int entityList[ MAX_GENTITIES ];
    int i;
    int min_distance = MGTURRET_RANGE * 5;
    int new_distance;
    int closest_medi = -1;
    gentity_t *target;
    VectorSet( range, vectorRange, vectorRange, vectorRange );
    VectorAdd( self->client->ps.origin, range, maxs );
    VectorSubtract( self->client->ps.origin, range, mins );
    total_entities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);
    for( i = 0; i < total_entities; ++i )
    {
        target = &g_entities[entityList[ i ] ];

        if( target->s.eType == ET_BUILDABLE && target->s.modelindex == BA_H_MEDISTAT && target->powered) {
            new_distance = botGetDistanceBetweenPlayer( self, target );
            if( new_distance < min_distance ) {
                min_distance = new_distance;
                closest_medi = entityList[i];
            }
        }

    }
        return closest_medi;
}
int botFindDamagedFriendlyStructure( gentity_t *self )
{
    // The range of our scanning field.
    int vectorRange = MGTURRET_RANGE * 5;
    // vectorRange converted to a vector
    vec3_t range;
    // Lower bound vector
    vec3_t mins;
    // Upper bound vector
    vec3_t maxs;
    // Indexing field
    int i;
    // Entities located in scanning field
    int total_entities;
    
    int new_distance;
    // Array which contains the located entities
    int entityList[ MAX_GENTITIES ];
    int min_distance = MGTURRET_RANGE * 5;
    int nearest_dmged_building = -1;
    // Temporary entitiy
    gentity_t *target;
    // Temporary buildable
    buildable_t inspectedBuilding;

    VectorSet( range, vectorRange, vectorRange, vectorRange );
    VectorAdd( self->client->ps.origin, range, maxs );
    VectorSubtract( self->client->ps.origin, range, mins );

    // Fetch all entities in the bounding box and iterate over them
    // to locate the structures that belong to the team of the bot and that
    // are not at full health.
    total_entities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);
    for( i = 0; i < total_entities; ++i )
    {
        target = &g_entities[ entityList[ i ] ];
        inspectedBuilding = BG_FindBuildNumForEntityName( target->classname );
        if(target->s.eType == ET_BUILDABLE &&
           target->biteam == self->client->ps.stats[ STAT_PTEAM ] &&
           target->health !=  BG_FindHealthForBuildable( inspectedBuilding ) &&
           target->health > 0 ) {
            new_distance = botGetDistanceBetweenPlayer( self, target );
            if( new_distance < min_distance ) {
                min_distance = new_distance;
                nearest_dmged_building = entityList[ i ];
            }
        }
    }
    return nearest_dmged_building;
}

qboolean botWeaponHasLowAmmo(gentity_t *self) {
    return BG_FindPercentAmmo(self->client->ps.weapon, self->client->ps.stats,self->client->ps.ammo,self->client->ps.powerups) < BOT_LOW_AMMO / 100;
}

