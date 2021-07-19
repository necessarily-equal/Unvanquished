/*
===========================================================================

Unvanquished GPL Source Code
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of the Unvanquished GPL Source Code (Unvanquished Source Code).

Unvanquished is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Unvanquished is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Unvanquished; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

===========================================================================
*/

#include "sg_bot_ai.h"
#include "sg_bot_behavior_tree.h"
#include "sg_bot_util.h"
#include "Entities.h"

/*
======================
Action Nodes

Action nodes are always the leaves of the behavior tree
They make the bot do a specific thing while leaving decision making
to the rest of the behavior tree
======================
*/

AINodeStatus_t BotActionFireWeapon( gentity_t *self, AIActionNode* )
{
	if ( WeaponIsEmpty( BG_GetPlayerWeapon( &self->client->ps ), &self->client->ps ) && self->client->pers.team == TEAM_HUMANS )
	{
		G_ForceWeaponChange( self, WP_BLASTER );
	}

	if ( BG_GetPlayerWeapon( &self->client->ps ) == WP_HBUILD )
	{
		G_ForceWeaponChange( self, WP_BLASTER );
	}

	BotFireWeaponAI( self );
	return STATUS_SUCCESS;
}

AINodeStatus_t BotActionTeleport( gentity_t *self, AIActionNode *node )
{
	vec3_t pos = { (float) node->params[0], (float) node->params[1], (float) node->params[2] };
	VectorCopy( pos,self->client->ps.origin );
	return STATUS_SUCCESS;
}

AINodeStatus_t BotActionActivateUpgrade( gentity_t *self, AIActionNode *node )
{
	upgrade_t u = (upgrade_t) (int) node->params[ 0 ];

	if ( !BG_UpgradeIsActive( u, self->client->ps.stats ) &&
		BG_InventoryContainsUpgrade( u, self->client->ps.stats ) )
	{
		BG_ActivateUpgrade( u, self->client->ps.stats );
	}
	return STATUS_SUCCESS;
}

AINodeStatus_t BotActionDeactivateUpgrade( gentity_t *self, AIActionNode *node )
{
	upgrade_t u = (upgrade_t) (int) node->params[ 0 ];

	if ( BG_UpgradeIsActive( u, self->client->ps.stats ) &&
		BG_InventoryContainsUpgrade( u, self->client->ps.stats ) )
	{
		BG_DeactivateUpgrade( u, self->client->ps.stats );
	}
	return STATUS_SUCCESS;
}

AINodeStatus_t BotActionAimAtGoal( gentity_t *self, AIActionNode* )
{
	if ( BotGetTargetTeam( self->botMind->goal ) != self->client->pers.team )
	{
		BotAimAtEnemy( self );
	}
	else
	{
		vec3_t pos;
		BotGetTargetPos( self->botMind->goal, pos );
		BotSlowAim( self, pos, 0.5 );
		BotAimAtLocation( self, pos );
	}
	return STATUS_SUCCESS;
}

AINodeStatus_t BotActionMoveToGoal( gentity_t *self, AIActionNode* )
{
	BotMoveToGoal( self );
	return STATUS_RUNNING;
}

AINodeStatus_t BotActionMoveInDir( gentity_t *self, AIActionNode *node )
{
	AIActionNode *a = (AIActionNode *) node;
	int dir = (int) a->params[ 0 ];
	if ( a->params.size() == 2 )
	{
		dir |= (int) a->params[ 1 ];
	}
	BotMoveInDir( self, dir );
	return STATUS_SUCCESS;
}

AINodeStatus_t BotActionStrafeDodge( gentity_t *self, AIActionNode* )
{
	BotStrafeDodge( self );
	return STATUS_SUCCESS;
}

AINodeStatus_t BotActionAlternateStrafe( gentity_t *self, AIActionNode* )
{
	BotAlternateStrafe( self );
	return STATUS_SUCCESS;
}

AINodeStatus_t BotActionClassDodge( gentity_t *self, AIActionNode* )
{
	BotClassMovement( self, BotTargetInAttackRange( self, self->botMind->goal ) );
	return STATUS_SUCCESS;
}

AINodeStatus_t BotActionChangeGoal( gentity_t *self, AIActionNode *node )
{
	AIActionNode *a = (AIActionNode *) node;

	if( a->params.size() == 1 )
	{
		AIEntity_t et = (AIEntity_t) (int) a->params[0];
		botEntityAndDistance_t e = AIEntityToGentity( self, et );
		if ( !BotChangeGoalEntity( self, e.ent ) )
		{
			return STATUS_FAILURE;
		}
	}
	else if( a->params.size() == 3 )
	{
		vec3_t pos = { (float) a->params[0], (float) a->params[1], (float) a->params[2] };
		if ( !BotChangeGoalPos( self, pos ) )
		{
			return STATUS_FAILURE;
		}
	}
	else
	{
		return STATUS_FAILURE;
	}

	self->botMind->currentNode = node;
	return STATUS_SUCCESS;
}

AINodeStatus_t BotActionEvolveTo( gentity_t *self, AIActionNode *node )
{
	class_t c = ( class_t ) (int) node->params[ 0 ];

	if ( self->client->ps.stats[ STAT_CLASS ] == c )
	{
		return STATUS_SUCCESS;
	}

	if ( BotEvolveToClass( self, c ) )
	{
		return STATUS_SUCCESS;
	}

	return STATUS_FAILURE;
}

AINodeStatus_t BotActionSay( gentity_t *self, AIActionNode *node )
{
	const char *str = (const char *) node->params[0];
	saymode_t say = (saymode_t) (int) node->params[1];
	G_Say( self, say, str );
	return STATUS_SUCCESS;
}

// TODO: Move decision making out of these actions and into the rest of the behavior tree
AINodeStatus_t BotActionFight( gentity_t *self, AIActionNode *node )
{
	team_t myTeam = ( team_t ) self->client->pers.team;

	if ( self->botMind->currentNode != node )
	{
		if ( !BotChangeGoalEntity( self, self->botMind->bestEnemy.ent ) )
		{
			return STATUS_FAILURE;
		}

		self->botMind->currentNode = node;
		self->botMind->enemyLastSeen = level.time;
		return STATUS_RUNNING;
	}

	if ( !BotTargetIsEntity( self->botMind->goal ) )
	{
		return STATUS_FAILURE;
	}

	if ( !BotEnemyIsValid( self, self->botMind->goal.ent ) )
	{
		return STATUS_SUCCESS;
	}

	if ( !self->botMind->nav.havePath )
	{
		return STATUS_FAILURE;
	}

	if ( WeaponIsEmpty( BG_GetPlayerWeapon( &self->client->ps ), &self->client->ps ) && myTeam == TEAM_HUMANS )
	{
		G_ForceWeaponChange( self, WP_BLASTER );
	}

	if ( BG_GetPlayerWeapon( &self->client->ps ) == WP_HBUILD )
	{
		G_ForceWeaponChange( self, WP_BLASTER );
	}

	//aliens have radar so they will always 'see' the enemy if they are in radar range
	if ( myTeam == TEAM_ALIENS && DistanceToGoalSquared( self ) <= Square( ALIENSENSE_RANGE ) )
	{
		self->botMind->enemyLastSeen = level.time;
	}

	if ( !BotTargetIsVisible( self, self->botMind->goal, CONTENTS_SOLID ) )
	{
		botTarget_t proposedTarget;
		BotSetTarget( &proposedTarget, self->botMind->bestEnemy.ent, nullptr );

		//we can see another enemy (not our target) so switch to it
		if ( self->botMind->bestEnemy.ent && self->botMind->goal.ent != self->botMind->bestEnemy.ent && BotPathIsWalkable( self, proposedTarget ) )
		{
			return STATUS_SUCCESS;
		}
		else if ( level.time - self->botMind->enemyLastSeen >= g_bot_chasetime.integer )
		{
			return STATUS_SUCCESS;
		}
		else
		{
			BotMoveToGoal( self );
			return STATUS_RUNNING;
		}
	}
	else
	{
		bool inAttackRange = BotTargetInAttackRange( self, self->botMind->goal );
		self->botMind->enemyLastSeen = level.time;

		if ( ( inAttackRange && myTeam == TEAM_HUMANS ) || self->botMind->nav.directPathToGoal )
		{
			BotAimAtEnemy( self );

			BotMoveInDir( self, MOVE_FORWARD );

			if ( inAttackRange || self->client->ps.weapon == WP_PAIN_SAW )
			{
				BotFireWeaponAI( self );
			}

			if ( myTeam == TEAM_HUMANS )
			{
				if ( self->botMind->botSkill.level >= 3 && DistanceToGoalSquared( self ) < Square( MAX_HUMAN_DANCE_DIST )
				        && ( DistanceToGoalSquared( self ) > Square( MIN_HUMAN_DANCE_DIST ) || self->botMind->botSkill.level < 5 )
				        && self->client->ps.weapon != WP_PAIN_SAW && self->client->ps.weapon != WP_FLAMER )
				{
					BotMoveInDir( self, MOVE_BACKWARD );
				}
				else if ( DistanceToGoalSquared( self ) <= Square( MIN_HUMAN_DANCE_DIST ) ) //we wont hit this if skill < 5
				{
					//we will be moving toward enemy, strafe too
					//the result: we go around the enemy
					BotAlternateStrafe( self );
				}
				else if ( DistanceToGoalSquared( self ) >= Square( MAX_HUMAN_DANCE_DIST ) && self->client->ps.weapon != WP_PAIN_SAW )
				{
					if ( DistanceToGoalSquared( self ) - Square( MAX_HUMAN_DANCE_DIST ) < 100 )
					{
						BotStandStill( self );
					}
					else
					{
						BotStrafeDodge( self );
					}
				}

				if ( inAttackRange && BotGetTargetType( self->botMind->goal ) == entityType_t::ET_BUILDABLE )
				{
					BotStandStill( self );
				}

				BotSprint( self, true );
			}
			else if ( myTeam == TEAM_ALIENS )
			{
				BotClassMovement( self, inAttackRange );
			}
		}
		else
		{
			BotMoveToGoal( self );
		}
	}
	return STATUS_RUNNING;
}

AINodeStatus_t BotActionFlee( gentity_t *self, AIActionNode *node )
{
	if ( node != self->botMind->currentNode )
	{
		if ( !BotChangeGoal( self, BotGetRetreatTarget( self ) ) )
		{
			return STATUS_FAILURE;
		}

		self->botMind->currentNode = node;
		return STATUS_RUNNING;
	}

	if ( !BotTargetIsEntity( self->botMind->goal ) )
	{
		return STATUS_FAILURE;
	}

	if ( GoalInRange( self, 70 ) )
	{
		return STATUS_SUCCESS;
	}
	else
	{
		BotMoveToGoal( self );
	}

	return STATUS_RUNNING;
}

AINodeStatus_t BotActionRoamInRadius( gentity_t *self, AIActionNode *node )
{
	AIActionNode *a = (AIActionNode *) node;
	AIEntity_t e = (AIEntity_t) (int) a->params[0];
	float radius = (float) a->params[1];

	if ( node != self->botMind->currentNode )
	{
		vec3_t point;
		botEntityAndDistance_t ent = AIEntityToGentity( self, e );

		if ( !ent.ent )
		{
			return STATUS_FAILURE;
		}

		if ( !trap_BotFindRandomPointInRadius( self->s.number, ent.ent->s.origin, point, radius ) )
		{
			return STATUS_FAILURE;
		}

		if ( !BotChangeGoalPos( self, point ) )
		{
			return STATUS_FAILURE;
		}

		self->botMind->currentNode = node;
		return STATUS_RUNNING;
	}

	if ( self->botMind->nav.directPathToGoal && GoalInRange( self, 70 ) )
	{
		return STATUS_SUCCESS;
	}
	else
	{
		BotMoveToGoal( self );
	}

	return STATUS_RUNNING;
}

AINodeStatus_t BotActionRoam( gentity_t *self, AIActionNode *node )
{
	// we are just starting to roam, get a target location
	if ( node != self->botMind->currentNode )
	{
		botTarget_t target = BotGetRoamTarget( self );
		if ( !BotChangeGoal( self, target ) )
		{
			return STATUS_FAILURE;
		}

		self->botMind->currentNode = node;
		return STATUS_RUNNING;
	}

	if ( self->botMind->nav.directPathToGoal && GoalInRange( self, 70 ) )
	{
		return STATUS_SUCCESS;
	}
	else
	{
		BotMoveToGoal( self );
	}
	return STATUS_RUNNING;
}

botTarget_t BotGetMoveToTarget( gentity_t *self, AIEntity_t e )
{
	botTarget_t target;
	botEntityAndDistance_t en = AIEntityToGentity( self, e );
	BotSetTarget( &target, en.ent, nullptr );
	return target;
}

AINodeStatus_t BotActionMoveTo( gentity_t *self, AIActionNode *node )
{
	float radius = 0;
	AIActionNode *moveTo = (AIActionNode *) node;
	AIEntity_t ent = (AIEntity_t) (int) moveTo->params[0];

	if ( moveTo->params.size() > 1 )
	{
		radius = std::max( (float) moveTo->params[1], 0.0f );
	}

	if ( node != self->botMind->currentNode )
	{
		if ( !BotChangeGoal( self, BotGetMoveToTarget( self, ent ) ) )
		{
			return STATUS_FAILURE;
		}

		self->botMind->currentNode = node;
		return STATUS_RUNNING;
	}

	if ( self->botMind->goal.ent )
	{
		// Don't move to dead targets.
		if ( Entities::IsDead( self->botMind->goal.ent ) )
		{
			return STATUS_FAILURE;
		}
	}

	BotMoveToGoal( self );

	if ( radius == 0 )
	{
		radius = BotGetGoalRadius( self );
	}

	if ( DistanceToGoal2DSquared( self ) <= Square( radius ) && self->botMind->nav.directPathToGoal )
	{
		return STATUS_SUCCESS;
	}

	return STATUS_RUNNING;
}

AINodeStatus_t BotActionRush( gentity_t *self, AIActionNode *node )
{
	if ( self->botMind->currentNode != node )
	{
		if ( !BotChangeGoal( self, BotGetRushTarget( self ) ) )
		{
			return STATUS_FAILURE;
		}

		self->botMind->currentNode = node;
		return STATUS_RUNNING;
	}

	if ( !BotTargetIsEntity( self->botMind->goal ) )
	{
		return STATUS_FAILURE;
	}

	// Can only rush living targets.
	if ( !Entities::IsAlive( self->botMind->goal.ent ) )
	{
		return STATUS_FAILURE;
	}

	if ( !GoalInRange( self, 100 ) )
	{
		BotMoveToGoal( self );
	}
	return STATUS_RUNNING;
}

AINodeStatus_t BotActionHeal( gentity_t *self, AIActionNode *node )
{
	if ( self->client->pers.team == TEAM_HUMANS )
	{
		return BotActionHealH( self, node );
	}
	else
	{
		return BotActionHealA( self, node );
	}
}

AINodeStatus_t BotActionSuicide( gentity_t *self, AIActionNode* )
{
	Entities::Kill( self, MOD_SUICIDE );
	return AINodeStatus_t::STATUS_SUCCESS;
}

AINodeStatus_t BotActionJump( gentity_t *self, AIActionNode* )
{
	return BotJump( self ) ? AINodeStatus_t::STATUS_SUCCESS : AINodeStatus_t::STATUS_FAILURE;
}

AINodeStatus_t BotActionResetStuckTime( gentity_t *self, AIActionNode* )
{
	BotResetStuckTime( self );
	return AINodeStatus_t::STATUS_SUCCESS;
}

AINodeStatus_t BotActionGesture( gentity_t *self, AIActionNode* )
{
	usercmd_t *botCmdBuffer = &self->botMind->cmdBuffer;
	usercmdPressButton( botCmdBuffer->buttons, BUTTON_GESTURE );
	return AINodeStatus_t::STATUS_SUCCESS;
}

/*
	alien specific actions
*/
AINodeStatus_t BotActionEvolve ( gentity_t *self, AIActionNode* )
{
	AINodeStatus_t status = STATUS_FAILURE;
	if ( !g_bot_evolve.integer )
	{
		return status;
	}

	if ( BotCanEvolveToClass( self, PCL_ALIEN_LEVEL4 ) && g_bot_level4.integer )
	{
		if ( BotEvolveToClass( self, PCL_ALIEN_LEVEL4 ) )
		{
			status = STATUS_SUCCESS;
		}
	}
	else if ( BotCanEvolveToClass( self, PCL_ALIEN_LEVEL3_UPG ) && g_bot_level3upg.integer )
	{
		if ( BotEvolveToClass( self, PCL_ALIEN_LEVEL3_UPG ) )
		{
			status = STATUS_SUCCESS;
		}
	}
	else if ( BotCanEvolveToClass( self, PCL_ALIEN_LEVEL3 ) &&
	          ( !BG_ClassUnlocked( PCL_ALIEN_LEVEL3_UPG ) ||!g_bot_level2upg.integer ||
	            !g_bot_level3upg.integer ) && g_bot_level3.integer )
	{
		if ( BotEvolveToClass( self, PCL_ALIEN_LEVEL3 ) )
		{
			status = STATUS_SUCCESS;
		}
	}
	else if ( BotCanEvolveToClass( self, PCL_ALIEN_LEVEL2_UPG ) && g_bot_level2upg.integer )
	{
		if ( BotEvolveToClass( self, PCL_ALIEN_LEVEL2_UPG ) )
		{
			status = STATUS_SUCCESS;
		}
	}
	else if ( BotCanEvolveToClass( self, PCL_ALIEN_LEVEL2 ) && g_bot_level2.integer )
	{
		if ( BotEvolveToClass( self, PCL_ALIEN_LEVEL2 ) )
		{
			status = STATUS_SUCCESS;
		}
	}
	else if ( BotCanEvolveToClass( self, PCL_ALIEN_LEVEL1 ) && g_bot_level1.integer )
	{
		if ( BotEvolveToClass( self, PCL_ALIEN_LEVEL1 ) )
		{
			status = STATUS_SUCCESS;
		}
	}
	else if ( BotCanEvolveToClass( self, PCL_ALIEN_LEVEL0 ) )
	{
		if ( BotEvolveToClass( self, PCL_ALIEN_LEVEL0 ) )
		{
			status = STATUS_SUCCESS;
		}
	}

	return status;
}

AINodeStatus_t BotActionHealA( gentity_t *self, AIActionNode *node )
{
	gentity_t *healTarget = nullptr;

	if ( self->botMind->closestBuildings[BA_A_BOOSTER].ent )
	{
		healTarget = self->botMind->closestBuildings[BA_A_BOOSTER].ent;
	}
	else if ( self->botMind->closestBuildings[BA_A_OVERMIND].ent )
	{
		healTarget = self->botMind->closestBuildings[BA_A_OVERMIND].ent;
	}
	else if ( self->botMind->closestBuildings[BA_A_SPAWN].ent )
	{
		healTarget = self->botMind->closestBuildings[BA_A_SPAWN].ent;
	}

	if ( !healTarget )
	{
		return STATUS_FAILURE;
	}

	if ( self->client->pers.team != TEAM_ALIENS )
	{
		return STATUS_FAILURE;
	}

	if ( self->botMind->currentNode != node )
	{
		// already fully healed
		if ( Entities::HasFullHealth(self) )
		{
			return STATUS_FAILURE;
		}

		if ( !BotChangeGoalEntity( self, healTarget ) )
		{
			return STATUS_FAILURE;
		}

		self->botMind->currentNode = node;
		return STATUS_RUNNING;
	}

	//we are fully healed now
	if ( Entities::HasFullHealth(self) )
	{
		return STATUS_SUCCESS;
	}

	if ( !BotTargetIsEntity( self->botMind->goal ) )
	{
		return STATUS_FAILURE;
	}

	// Can't heal at dead targets.
	if ( Entities::IsDead( self->botMind->goal.ent ) )
	{
		return STATUS_FAILURE;
	}

	if ( !GoalInRange( self, 100 ) )
	{
		BotMoveToGoal( self );
	}
	return STATUS_RUNNING;
}

/*
	human specific actions
*/
AINodeStatus_t BotActionHealH( gentity_t *self, AIActionNode *node )
{
	vec3_t targetPos;
	vec3_t myPos;
	bool fullyHealed = Entities::HasFullHealth(self) &&
	                   BG_InventoryContainsUpgrade( UP_MEDKIT, self->client->ps.stats );

	if ( self->client->pers.team != TEAM_HUMANS )
	{
		return STATUS_FAILURE;
	}

	if ( self->botMind->currentNode != node )
	{
		if ( fullyHealed )
		{
			return STATUS_FAILURE;
		}

		if ( !BotChangeGoalEntity( self, self->botMind->closestBuildings[ BA_H_MEDISTAT ].ent ) )
		{
			return STATUS_FAILURE;
		}

		self->botMind->currentNode = node;
		return STATUS_RUNNING;
	}

	if ( fullyHealed )
	{
		return STATUS_SUCCESS;
	}

	//safety check
	if ( !BotTargetIsEntity( self->botMind->goal ) )
	{
		return STATUS_FAILURE;
	}

	// Can't heal at dead targets.
	if ( Entities::IsDead( self->botMind->goal.ent ) )
	{
		return STATUS_FAILURE;
	}

	//this medi is no longer powered so signal that the goal is unusable
	if ( !self->botMind->goal.ent->powered )
	{
		return STATUS_FAILURE;
	}

	BotGetTargetPos( self->botMind->goal, targetPos );
	VectorCopy( self->s.origin, myPos );
	targetPos[2] += BG_BuildableModelConfig( BA_H_MEDISTAT )->maxs[2];
	myPos[2] += self->r.mins[2]; //mins is negative

	//keep moving to the medi until we are on top of it
	if ( DistanceSquared( myPos, targetPos ) > Square( BG_BuildableModelConfig( BA_H_MEDISTAT )->mins[1] ) )
	{
		BotMoveToGoal( self );
	}
	return STATUS_RUNNING;
}
AINodeStatus_t BotActionRepair( gentity_t *self, AIActionNode *node )
{
	vec3_t forward;
	vec3_t targetPos;
	vec3_t selfPos;

	if ( node != self->botMind->currentNode )
	{
		if ( !BotChangeGoalEntity( self, self->botMind->closestDamagedBuilding.ent ) )
		{
			return STATUS_FAILURE;
		}

		self->botMind->currentNode = node;
		return STATUS_RUNNING;
	}

	if ( !BotTargetIsEntity( self->botMind->goal ) )
	{
		return STATUS_FAILURE;
	}

	// Can only repair alive targets.
	if ( !Entities::IsAlive( self->botMind->goal.ent ) )
	{
		return STATUS_FAILURE;
	}

	if ( Entities::HasFullHealth(self->botMind->goal.ent) )
	{
		return STATUS_SUCCESS;
	}

	if ( BG_GetPlayerWeapon( &self->client->ps ) != WP_HBUILD )
	{
		G_ForceWeaponChange( self, WP_HBUILD );
	}

	AngleVectors( self->client->ps.viewangles, forward, nullptr, nullptr );
	BotGetTargetPos( self->botMind->goal, targetPos );
	VectorMA( self->s.origin, self->r.maxs[1], forward, selfPos );

	//move to the damaged building until we are in range
	if ( !BotTargetIsVisible( self, self->botMind->goal, MASK_SHOT ) || DistanceToGoalSquared( self ) > Square( 100 ) )
	{
		BotMoveToGoal( self );
	}
	else
	{
		//aim at the buildable
		BotSlowAim( self, targetPos, 0.5 );
		BotAimAtLocation( self, targetPos );
		// we automatically heal a building if close enough and aiming at it
	}
	return STATUS_RUNNING;
}
AINodeStatus_t BotActionBuy( gentity_t *self, AIActionNode *node )
{
	weapon_t  weapon;
	upgrade_t upgrades[4];
	int numUpgrades;

	if ( node->params.size() == 0 )
	{
		// equip action
		BotGetDesiredBuy( self, &weapon, upgrades, &numUpgrades );
	}
	else
	{
		// first parameter should always be a weapon
		weapon = ( weapon_t ) (int) node->params[ 0 ];

		if ( weapon < WP_NONE || weapon >= WP_NUM_WEAPONS )
		{
			Log::Warn("parameter 1 to action buy out of range" );
			weapon = WP_NONE;
		}

		numUpgrades = 0;

		// other parameters are always upgrades
		for ( size_t i = 1; i < node->params.size(); i++ )
		{
			upgrades[ numUpgrades ] = (upgrade_t) (int) node->params[ i ];

			if ( upgrades[ numUpgrades ] <= UP_NONE || upgrades[ numUpgrades ] >= UP_NUM_UPGRADES )
			{
				Log::Warn("parameter %zu to action buy out of range", i + 1 );
				continue;
			}

			numUpgrades++;
		}
	}

	if ( !g_bot_buy.integer )
	{
		return STATUS_FAILURE;
	}

	if ( BotGetEntityTeam( self ) != TEAM_HUMANS )
	{
		return STATUS_FAILURE;
	}

	//check if we already have everything
	if ( BG_InventoryContainsWeapon( weapon, self->client->ps.stats ) || weapon == WP_NONE )
	{
		int numContain = 0;

		for ( int i = 0; i < numUpgrades; i++ )
		{
			if ( BG_InventoryContainsUpgrade( upgrades[i], self->client->ps.stats ) )
			{
				numContain++;
			}
		}

		//we have every upgrade we want to buy
		if ( numContain == numUpgrades )
		{
			return STATUS_FAILURE;
		}
	}

	if ( self->botMind->currentNode != node )
	{
		botEntityAndDistance_t *ngoal;

		ngoal = &self->botMind->closestBuildings[ BA_H_ARMOURY ];

		if ( !ngoal->ent )
		{
			return STATUS_FAILURE; // no suitable goal found
		}

		if ( !BotChangeGoalEntity( self, ngoal->ent ) )
		{
			return STATUS_FAILURE;
		}

		self->botMind->currentNode = node;
		return STATUS_RUNNING;
	}

	if ( !BotTargetIsEntity( self->botMind->goal ) )
	{
		return STATUS_FAILURE;
	}

	// Can't buy at dead targets.
	if ( Entities::IsDead( self->botMind->goal.ent ) )
	{
		return STATUS_FAILURE;
	}

	if ( !self->botMind->goal.ent->powered )
	{
		return STATUS_FAILURE;
	}

	if ( GoalInRange( self, ENTITY_BUY_RANGE ) )
	{
		if ( numUpgrades )
		{
			BotSellAll( self );
		}
		else if ( weapon != WP_NONE )
		{
			BotSellWeapons( self );
		}

		if ( weapon != WP_NONE )
		{
			BotBuyWeapon( self, weapon );
		}

		for ( int i = 0; i < numUpgrades; i++ )
		{
			BotBuyUpgrade( self, upgrades[i] );
		}

		// make sure that we're not using the blaster
		if ( weapon != WP_NONE )
		{
			G_ForceWeaponChange( self, weapon );
		}

		return STATUS_SUCCESS;
	}

	BotMoveToGoal( self );
	return STATUS_RUNNING;
}

/*
======================
Decorators

Decorators are used to add functionality to the child node
======================
*/
AINodeStatus_t BotDecoratorTimer( gentity_t *self, BTMemory &mem, AIDecoratorNode *node )
{
	if ( level.time > node->data[ self->s.number ] )
	{
		AINodeStatus_t status = node->child->run( self, mem );

		if ( status == STATUS_FAILURE )
		{
			node->data[ self->s.number ] = level.time + (int)node->params[ 0 ];
		}

		return status;
	}

	return STATUS_FAILURE;
}

AINodeStatus_t BotDecoratorReturn( gentity_t *self, BTMemory &mem, AIDecoratorNode *node )
{
	node->child->run( self, mem );

	// force return status
	AINodeStatus_t status = (AINodeStatus_t) (int) node->params[ 0 ];
	return status;
}
