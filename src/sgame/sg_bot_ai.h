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
#include "sg_local.h"
#include "sg_bot_behavior_tree.h"

#ifndef __BOT_AI_HEADER
#define __BOT_AI_HEADER

// decorator nodes
AINodeStatus_t BotDecoratorTimer( gentity_t *self, AIDecoratorNode *node );
AINodeStatus_t BotDecoratorReturn( gentity_t *self, AIDecoratorNode *node );

// action nodes
AINodeStatus_t BotActionChangeGoal( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionMoveToGoal( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionFireWeapon( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionAimAtGoal( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionAlternateStrafe( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionStrafeDodge( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionMoveInDir( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionClassDodge( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionTeleport( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionActivateUpgrade( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionDeactivateUpgrade( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionEvolveTo( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionSay( gentity_t *self, AIActionNode *node );

AINodeStatus_t BotActionFight( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionBuy( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionRepair( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionEvolve ( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionHealH( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionHealA( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionHeal( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionFlee( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionRoam( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionRoamInRadius( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionMoveTo( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionRush( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionSuicide( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionJump( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionResetStuckTime( gentity_t *self, AIActionNode *node );
AINodeStatus_t BotActionGesture( gentity_t *self, AIActionNode* );

#endif
