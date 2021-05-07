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

#include "shared/parse.h"
#include "sg_bot_parse.h"
#include "sg_bot_util.h"
#include "Entities.h"

void WarnAboutParseError( parseError p )
{
	if ( p.line != 0 )
	{
		Log::Warn( "Parse error %s on line %i", p.message, p.line );
	}
	else
	{
		Log::Warn( "Parse error %s", p.message );
	}
}

static bool expectToken( const char *s, pc_token_list **list, bool next )
{
	const pc_token_list *current = *list;

	if ( !current )
	{
		Log::Warn( "Expected token %s but found end of file", s );
		return false;
	}

	if ( Q_stricmp( current->token.string, s ) != 0 )
	{
		Log::Warn( "Expected token %s but found %s on line %d", s, current->token.string, current->token.line );
		return false;
	}

	if ( next )
	{
		*list = current->next;
	}
	return true;
}

AIValue_t AIBoxToken( const pc_token_stripped_t *token )
{
	if ( token->type == tokenType_t::TT_STRING )
	{
		return AIValue_t( token->string );
	}

	if ( ( float ) token->intvalue != token->floatvalue )
	{
		return AIValue_t( token->floatvalue );
	}
	else
	{
		return AIValue_t( token->intvalue );
	}
}

// functions that are used to provide values to the behavior tree in condition nodes
static AIValue_t buildingIsDamaged( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( self->botMind->closestDamagedBuilding.ent != nullptr );
}

static AIValue_t haveWeapon( gentity_t *self, const std::vector<AIValue_t> &params )
{
	return AIValue_t( BG_InventoryContainsWeapon( (int) params[0], self->client->ps.stats ) );
}

static AIValue_t alertedToEnemy( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( self->botMind->bestEnemy.ent != nullptr );
}

static AIValue_t botTeam( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( self->client->pers.team );
}

static AIValue_t goalTeam( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( Util::ordinal<team_t, int>(
				BotGetTargetTeam( self->botMind->goal ) ) );
}

static AIValue_t goalType( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( Util::ordinal<entityType_t, int>(
				BotGetTargetType( self->botMind->goal ) ) );
}

// TODO: Check if we can just check for HealthComponent.
static AIValue_t goalDead( gentity_t *self, const std::vector<AIValue_t> & )
{
	bool dead = false;
	botTarget_t *goal = &self->botMind->goal;

	if ( !BotTargetIsEntity( *goal ) )
	{
		dead = true;
	}
	else if ( BotGetTargetTeam( *goal ) == TEAM_NONE )
	{
		dead = true;
	}
	else if ( !Entities::IsAlive( self->botMind->goal.ent ) )
	{
		dead = true;
	}
	else if ( goal->ent->client && goal->ent->client->sess.spectatorState != SPECTATOR_NOT )
	{
		dead = true;
	}
	else if ( goal->ent->s.eType == entityType_t::ET_BUILDABLE && goal->ent->buildableTeam == self->client->pers.team && !goal->ent->powered )
	{
		dead = true;
	}

	return AIValue_t( dead );
}

static AIValue_t goalBuildingType( gentity_t *self, const std::vector<AIValue_t> & )
{
	if ( BotGetTargetType( self->botMind->goal ) != entityType_t::ET_BUILDABLE )
	{
		return AIValue_t( Util::ordinal<buildable_t, int>( BA_NONE ) );
	}

	return AIValue_t( self->botMind->goal.ent->s.modelindex );
}

static AIValue_t currentWeapon( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( Util::ordinal<weapon_t, int>(
				BG_GetPlayerWeapon( &self->client->ps ) ) );
}

static AIValue_t haveUpgrade( gentity_t *self, const std::vector<AIValue_t> &params )
{
	int upgrade = (int) params[0];
	return AIValue_t( !BG_UpgradeIsActive( upgrade, self->client->ps.stats ) && BG_InventoryContainsUpgrade( upgrade, self->client->ps.stats ) );
}

static AIValue_t percentAmmo( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( PercentAmmoRemaining( BG_PrimaryWeapon( self->client->ps.stats ), &self->client->ps ) );
}

static AIValue_t teamateHasWeapon( gentity_t *self, const std::vector<AIValue_t> &params )
{
	return AIValue_t( BotTeamateHasWeapon( self, (int) params[0] ) );
}

static AIValue_t distanceTo( gentity_t *self, const std::vector<AIValue_t> &params )
{
	AIEntity_t e = (AIEntity_t) (int) params[0];
	botEntityAndDistance_t ent = AIEntityToGentity( self, e );

	return AIValue_t( ent.distance );
}

static AIValue_t baseRushScore( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( BotGetBaseRushScore( self ) );
}

static AIValue_t healScore( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( BotGetHealScore( self ) );
}

static AIValue_t botClass( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( self->client->ps.stats[ STAT_CLASS ] );
}

static AIValue_t botSkill( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( self->botMind->botSkill.level );
}

static AIValue_t inAttackRange( gentity_t *self, const std::vector<AIValue_t> &params )
{
	botTarget_t target;
	AIEntity_t et = (AIEntity_t) (int) params[0];
	botEntityAndDistance_t e = AIEntityToGentity( self ,et );

	if ( !e.ent )
	{
		return AIValue_t( false );
	}

	BotSetTarget( &target, e.ent, nullptr );

	if ( BotTargetInAttackRange( self, target ) )
	{
		return AIValue_t( true );
	}

	return AIValue_t( false );
}

static AIValue_t isVisible( gentity_t *self, const std::vector<AIValue_t> &params )
{
	botTarget_t target;
	AIEntity_t et = (AIEntity_t) (int) params[0];
	botEntityAndDistance_t e = AIEntityToGentity( self, et );

	if ( !e.ent )
	{
		return AIValue_t( false );
	}

	BotSetTarget( &target, e.ent, nullptr );

	if ( BotTargetIsVisible( self, target, CONTENTS_SOLID ) )
	{
		if ( BotEnemyIsValid( self, e.ent ) )
		{
			self->botMind->enemyLastSeen = level.time;
		}
		return AIValue_t( true );
	}

	return AIValue_t( false );
}

static AIValue_t matchTime( gentity_t*, const std::vector<AIValue_t> & )
{
	return AIValue_t( level.matchTime );
}

static AIValue_t directPathTo( gentity_t *self, const std::vector<AIValue_t> &params )
{
	AIEntity_t e = (AIEntity_t) (int) params[0];
	botEntityAndDistance_t ed = AIEntityToGentity( self, e );

	if ( e == E_GOAL )
	{
		return AIValue_t( self->botMind->nav.directPathToGoal );
	}
	else if ( ed.ent )
	{
		botTarget_t target;
		BotSetTarget( &target, ed.ent, nullptr );
		return AIValue_t( BotPathIsWalkable( self, target ) );
	}

	return AIValue_t( false );
}

static AIValue_t botCanEvolveTo( gentity_t *self, const std::vector<AIValue_t> &params )
{
	class_t c = (class_t) (int) params[ 0 ];

	return AIValue_t( BotCanEvolveToClass( self, c ) );
}

static AIValue_t humanMomentum( gentity_t*, const std::vector<AIValue_t> & )
{
	return AIValue_t( level.team[ TEAM_HUMANS ].momentum );
}

static AIValue_t alienMomentum( gentity_t*, const std::vector<AIValue_t> & )
{
	return AIValue_t( level.team[ TEAM_ALIENS ].momentum );
}

static AIValue_t aliveTime( gentity_t*self, const std::vector<AIValue_t> & )
{
	return AIValue_t( level.time - self->botMind->spawnTime );
}

static AIValue_t randomChance( gentity_t*, const std::vector<AIValue_t> & )
{
	return AIValue_t( random() );
}

static AIValue_t cvarInt( gentity_t*, const std::vector<AIValue_t> &params )
{
	vmCvar_t *c = G_FindCvar( (const char *) params[0] );

	if ( !c )
	{
		return AIValue_t( 0 );
	}

	return AIValue_t( c->integer );
}

static AIValue_t cvarFloat( gentity_t*, const std::vector<AIValue_t> &params )
{
	vmCvar_t *c = G_FindCvar( (const char *) params[0] );

	if ( !c )
	{
		return AIValue_t( 0.0f );
	}

	return AIValue_t( c->value );
}

static AIValue_t percentHealth( gentity_t *self, const std::vector<AIValue_t> &params )
{
	AIEntity_t e = (AIEntity_t) (int) params[0];
	botEntityAndDistance_t et = AIEntityToGentity( self, e );
	float healthFraction;

	if (Entities::HasHealthComponent(et.ent)) {
		healthFraction = Entities::HealthFraction(et.ent);
	} else {
		healthFraction = 0.0f;
	}

	return AIValue_t( healthFraction );
}

static AIValue_t stuckTime( gentity_t *self, const std::vector<AIValue_t> & )
{
	return AIValue_t( level.time - self->botMind->stuckTime );
}

// functions accessible to the behavior tree for use in condition nodes
static const struct AIConditionMap_s
{
	const char    *name;
	AIValueType_t retType;
	AIFunc        func;
	int           nparams;
} conditionFuncs[] =
{
	{ "alertedToEnemy",    VALUE_INT,   alertedToEnemy,    0 },
	{ "alienMomentum",     VALUE_INT,   alienMomentum,     0 },
	{ "aliveTime",         VALUE_INT,   aliveTime,         0 },
	{ "baseRushScore",     VALUE_FLOAT, baseRushScore,     0 },
	{ "buildingIsDamaged", VALUE_INT,   buildingIsDamaged, 0 },
	{ "canEvolveTo",       VALUE_INT,   botCanEvolveTo,    1 },
	{ "class",             VALUE_INT,   botClass,          0 },
	{ "cvarFloat",         VALUE_FLOAT, cvarFloat,         1 },
	{ "cvarInt",           VALUE_INT,   cvarInt,           1 },
	{ "directPathTo",      VALUE_INT,   directPathTo,      1 },
	{ "distanceTo",        VALUE_FLOAT, distanceTo,        1 },
	{ "goalBuildingType",  VALUE_INT,   goalBuildingType,  0 },
	{ "goalIsDead",        VALUE_INT,   goalDead,          0 },
	{ "goalTeam",          VALUE_INT,   goalTeam,          0 },
	{ "goalType",          VALUE_INT,   goalType,          0 },
	{ "haveUpgrade",       VALUE_INT,   haveUpgrade,       1 },
	{ "haveWeapon",        VALUE_INT,   haveWeapon,        1 },
	{ "healScore",         VALUE_FLOAT, healScore,         0 },
	{ "humanMomentum",     VALUE_INT,   humanMomentum,     0 },
	{ "inAttackRange",     VALUE_INT,   inAttackRange,     1 },
	{ "isVisible",         VALUE_INT,   isVisible,         1 },
	{ "matchTime",         VALUE_INT,   matchTime,         0 },
	{ "percentAmmo",       VALUE_FLOAT, percentAmmo,       0 },
	{ "percentHealth",     VALUE_FLOAT, percentHealth,     1 },
	{ "random",            VALUE_FLOAT, randomChance,      0 },
	{ "skill",             VALUE_INT,   botSkill,          0 },
	{ "stuckTime",         VALUE_INT,   stuckTime,         0 },
	{ "team",              VALUE_INT,   botTeam,           0 },
	{ "teamateHasWeapon",  VALUE_INT,   teamateHasWeapon,  1 },
	{ "weapon",            VALUE_INT,   currentWeapon,     0 }
};

static const struct AIOpMap_s
{
	const char            *str;
	int                   tokenSubtype;
	AIOpType_t            opType;
} conditionOps[] =
{
	{ ">=", P_LOGIC_GEQ,     OP_GREATERTHANEQUAL },
	{ ">",  P_LOGIC_GREATER, OP_GREATERTHAN      },
	{ "<=", P_LOGIC_LEQ,     OP_LESSTHANEQUAL    },
	{ "<",  P_LOGIC_LESS,    OP_LESSTHAN         },
	{ "==", P_LOGIC_EQ,      OP_EQUAL            },
	{ "!=", P_LOGIC_UNEQ,    OP_NEQUAL           },
	{ "!",  P_LOGIC_NOT,     OP_NOT              },
	{ "&&", P_LOGIC_AND,     OP_AND              },
	{ "||", P_LOGIC_OR,      OP_OR               }
};

static AIOpType_t opTypeFromToken( pc_token_stripped_t *token )
{
	if ( token->type != tokenType_t::TT_PUNCTUATION )
	{
		return OP_NONE;
	}

	for ( unsigned i = 0; i < ARRAY_LEN( conditionOps ); i++ )
	{
		if ( token->subtype == conditionOps[ i ].tokenSubtype )
		{
			return conditionOps[ i ].opType;
		}
	}
	return OP_NONE;
}

static const char *opTypeToString( AIOpType_t op )
{
	for ( unsigned i = 0; i < ARRAY_LEN( conditionOps ); i++ )
	{
		if ( conditionOps[ i ].opType == op )
		{
			return conditionOps[ i ].str;
		}
	}
	return nullptr;
}

// compare operator precedence
static int opCompare( AIOpType_t op1, AIOpType_t op2 )
{
	if ( op1 < op2 )
	{
		return 1;
	}
	else if ( op1 > op2 )
	{
		return -1;
	}
	return 0;
}

static pc_token_list *findCloseParen( pc_token_list *start, pc_token_list *end )
{
	pc_token_list *list = start;
	int depth = 0;

	while ( list != end )
	{
		if ( list->token.string[ 0 ] == '(' )
		{
			depth++;
		}

		if ( list->token.string[ 0 ] == ')' )
		{
			depth--;
		}

		if ( depth == 0 )
		{
			return list;
		}

		list = list->next;
	}

	return nullptr;
}

static std::unique_ptr<AIValue_t> newValueLiteral( pc_token_list **list )
{
	pc_token_list *current = *list;
	pc_token_stripped_t *token = &current->token;

	*list = current->next;
	return Util::make_unique<AIValue_t>( std::move( AIBoxToken(token) ) );
}

static std::vector<AIValue_t>
parseFunctionParameters( pc_token_list **list, int minparams, int maxparams )
{
	pc_token_list *current = *list;
	pc_token_list *parenBegin = current->next;
	pc_token_list *parenEnd;
	pc_token_list *parse;
	std::vector<AIValue_t> params;
	int           numParams = 0;

	// functions should always be proceeded by a '(' if they have parameters
	if ( !expectToken( "(", &parenBegin, false ) )
	{
		*list = current;
		return {};
	}

	// find the end parenthesis around the function's args
	parenEnd = findCloseParen( parenBegin, nullptr );

	if ( !parenEnd )
	{
		Log::Warn( "could not find matching ')' for '(' on line %d", parenBegin->token.line );
		*list = parenBegin->next;
		return {};
	}

	// count the number of parameters
	parse = parenBegin->next;

	while ( parse != parenEnd )
	{
		if ( parse->token.type == tokenType_t::TT_NUMBER || parse->token.type == tokenType_t::TT_STRING )
		{
			numParams++;
		}
		else if ( parse->token.string[ 0 ] != ',' && parse->token.string[ 0 ] != '-' )
		{
			Log::Warn( "Invalid token %s in parameter list on line %d", parse->token.string, parse->token.line );
			*list = parenEnd->next; // skip invalid function expression
			return {};
		}
		parse = parse->next;
	}

	// warn if too many or too few parameters
	if ( numParams < minparams )
	{
		Log::Warn( "too few parameters for %s on line %d", current->token.string, current->token.line );
		*list = parenEnd->next;
		return {};
	}

	if ( numParams > maxparams )
	{
		Log::Warn( "too many parameters for %s on line %d", current->token.string, current->token.line );
		*list = parenEnd->next;
		return {};
	}

	if ( numParams )
	{
		// add the parameters
		parse = parenBegin->next;
		while ( parse != parenEnd )
		{
			if ( parse->token.type == tokenType_t::TT_NUMBER || parse->token.type == tokenType_t::TT_STRING )
			{
				params.push_back( AIBoxToken( &parse->token ) );
			}
			parse = parse->next;
		}
	}
	*list = parenEnd->next;
	return params;
}

static std::unique_ptr<AIValueFunc_t> newValueFunc( pc_token_list **list )
{
	pc_token_list *current = *list;
	pc_token_list *parenBegin = nullptr;
	struct AIConditionMap_s *f;

	f = (struct AIConditionMap_s*) bsearch( current->token.string, conditionFuncs, ARRAY_LEN( conditionFuncs ), sizeof( *conditionFuncs ), cmdcmp );

	if ( !f )
	{
		Log::Warn( "Unknown function: %s on line %d", current->token.string, current->token.line );
		*list = current->next;
		return nullptr;
	}

	parenBegin = current->next;

	// if the function has no parameters, allow it to be used without parenthesis
	if ( f->nparams == 0 && parenBegin->token.string[ 0 ] != '(' )
	{
		*list = current->next;
		return Util::make_unique<AIValueFunc_t>( f->retType, f->func, std::vector<AIValue_t>{} );
	}

	auto params = parseFunctionParameters( list, f->nparams, f->nparams );

	if ( params.size() == 0 && f->nparams > 0 )
	{
		return nullptr;
	}

	return Util::make_unique<AIValueFunc_t>( f->retType, f->func, std::move(params) );
}

static std::unique_ptr<AIExpression_t> Primary( pc_token_list **list );
static std::unique_ptr<AIExpression_t> ReadConditionExpression( pc_token_list **list, AIOpType_t op2 )
{
	std::unique_ptr<AIExpression_t> left;
	AIOpType_t op;

	if ( !*list )
	{
		Log::Warn( "Unexpected end of file" );
		return nullptr;
	}

	left = Primary( list );

	if ( !left )
	{
		return nullptr;
	}

	while ( op = opTypeFromToken( &(*list)->token ),
			isBinaryOp( op ) && opCompare( op, op2 ) >= 0 )
	{
		std::unique_ptr<AIExpression_t> right;
		pc_token_list *prev = *list;
		*list = (*list)->next;
		right = ReadConditionExpression( list, op );

		if ( !right )
		{
			Log::Warn( "Missing right operand for %s on line %d", opTypeToString( op ), prev->token.line );
			return nullptr;
		}

		left = Util::make_unique<AIBinaryOp_t>(
				op, std::move(left), std::move(right) );
	}

	return left;
}

static std::unique_ptr<AIExpression_t> Primary( pc_token_list **list )
{
	// CHECKME: probably one can simplify the control flow
	std::unique_ptr<AIExpression_t> tree = nullptr;
	pc_token_list *current = *list;

	if ( isUnaryOp( opTypeFromToken( &current->token ) ) )
	{
		AIOpType_t opType = opTypeFromToken( &current->token );
		*list = current->next;
		std::unique_ptr<AIExpression_t> t =
			ReadConditionExpression( list, opType );

		if ( !t )
		{
			Log::Warn( "Missing right operand for %s on line %d", opTypeToString( opType ), current->token.line );
			return nullptr;
		}

		return Util::make_unique<AIUnaryOp_t>(
				opType, std::move(t) );
	}
	else if ( current->token.string[0] == '(' )
	{
		*list = current->next;
		tree = ReadConditionExpression( list, OP_NONE );
		if ( !expectToken( ")", list, true ) )
		{
			return nullptr;
		}
	}
	else if ( current->token.type == tokenType_t::TT_NUMBER )
	{
		tree = newValueLiteral( list );
	}
	else if ( current->token.type == tokenType_t::TT_NAME )
	{
		tree = newValueFunc( list );
	}
	else
	{
		Log::Warn( "token %s on line %d is not valid", current->token.string, current->token.line );
	}
	return tree;
}

/*
======================
AIConditionNode constructor

Parses and creates an AIConditionNode from a token list
The token list pointer is modified to point to the beginning of the next node text block

A condition node has the form:
condition [expression] {
	child node
}

or the form:
condition [expression]

[expression] can be any valid set of boolean operations and values
======================
*/

AIConditionNode::AIConditionNode( pc_token_list **tokenlist )
	: AIGenericNode{ CONDITION_NODE, BotConditionNode },
	  child(nullptr), exp(nullptr)
{
	pc_token_list *current = *tokenlist;

	expectToken( "condition", &current, true );

	exp = ReadConditionExpression( &current, OP_NONE );

	if ( !current )
	{
		*tokenlist = current;
		throw parseError("unexpected end of file");
	}

	if ( !exp )
	{
		*tokenlist = current;
		throw parseError("missing expression in condition");
	}

	if ( Q_stricmp( current->token.string, "{" ) )
	{
		// this condition node has no child nodes
		*tokenlist = current;
		return;
	}

	current = current->next;

	child = ReadNode( &current );

	if ( !child )
	{
		*tokenlist = current;
		throw parseError("Parse error: could not parse child node of condition", (*tokenlist)->token.line);
	}

	if ( !expectToken( "}", &current, true ) )
	{
		*tokenlist = current;
		throw parseError("expected } at end of condition body", current->token.line);
	}

	*tokenlist = current;
}

static const struct AIDecoratorMap_s
{
	const char   *name;
	AINodeRunner run;
	int          minparams;
	int          maxparams;
} AIDecorators[] =
{
	{ "return", BotDecoratorReturn, 1, 1 },
	{ "timer", BotDecoratorTimer, 1, 1 }
}; // This list must be sorted

/*
======================
AIDecoratorNode constructor

Parses and creates an DecoratorNode_t from a token list
The token list pointer is modified to point to the beginning of the next node text block

A condition node has the form:
decorator [expression] {
	child node
}

where expression can either be return(RETURN_VALUE) or timer(TIME)
and will trigger when that value is returned, or each TIME milliseconds.
======================
*/

AIDecoratorNode::AIDecoratorNode( pc_token_list **list )
	: AIGenericNode{ DECORATOR_NODE, nullptr }, child(nullptr), data{}
{
	pc_token_list *current = *list;
	pc_token_list           *parenBegin;

	expectToken( "decorator", &current, true );

	if ( !current )
	{
		*list = current;
		throw parseError("unexpected end of file");
	}

	auto dec = (struct AIDecoratorMap_s*) bsearch( current->token.string,
			AIDecorators, ARRAY_LEN( AIDecorators ),
			sizeof( *AIDecorators ), cmdcmp );

	if ( !dec )
	{
		*list = current;
		throw parseError("invalid decorator", current->token.line);
	}

	parenBegin = current->next;

	run = dec->run;

	// allow dropping of parenthesis if we don't require any parameters
	if ( dec->minparams == 0 && parenBegin->token.string[0] != '(' )
	{
		return;
	}

	params = parseFunctionParameters( &current, dec->minparams, dec->maxparams );

	if ( params.size() == 0 && dec->minparams > 0 )
	{
		*list = current;
		throw parseError("could not parse function parameters", current->prev->token.line);
	}

	if ( !expectToken( "{", &current, true ) )
	{
		*list = current;
		throw parseError("missing {", current->prev->token.line);
	}

	child = ReadNode( &current );

	if ( !child )
	{
		*list = current;
		throw parseError("failed to parse child node of decorator", current->token.line );
	}

	expectToken( "}", &current, true );
	*list = current;
}

static const struct AIActionMap_s
{
	const char   *name;
	AINodeRunner run;
	int          minparams;
	int          maxparams;
} AIActions[] =
{
	{ "activateUpgrade",   BotActionActivateUpgrade,   1, 1 },
	{ "aimAtGoal",         BotActionAimAtGoal,         0, 0 },
	{ "alternateStrafe",   BotActionAlternateStrafe,   0, 0 },
	{ "buy",               BotActionBuy,               1, 4 },
	{ "changeGoal",        BotActionChangeGoal,        1, 3 },
	{ "classDodge",        BotActionClassDodge,        0, 0 },
	{ "deactivateUpgrade", BotActionDeactivateUpgrade, 1, 1 },
	{ "equip",             BotActionBuy,               0, 0 },
	{ "evolve",            BotActionEvolve,            0, 0 },
	{ "evolveTo",          BotActionEvolveTo,          1, 1 },
	{ "fight",             BotActionFight,             0, 0 },
	{ "fireWeapon",        BotActionFireWeapon,        0, 0 },
	{ "flee",              BotActionFlee,              0, 0 },
	{ "gesture",           BotActionGesture,           0, 0 },
	{ "heal",              BotActionHeal,              0, 0 },
	{ "jump",              BotActionJump,              0, 0 },
	{ "moveInDir",         BotActionMoveInDir,         1, 2 },
	{ "moveTo",            BotActionMoveTo,            1, 2 },
	{ "moveToGoal",        BotActionMoveToGoal,        0, 0 },
	{ "repair",            BotActionRepair,            0, 0 },
	{ "resetStuckTime",    BotActionResetStuckTime,    0, 0 },
	{ "roam",              BotActionRoam,              0, 0 },
	{ "roamInRadius",      BotActionRoamInRadius,      2, 2 },
	{ "rush",              BotActionRush,              0, 0 },
	{ "say",               BotActionSay,               2, 2 },
	{ "strafeDodge",       BotActionStrafeDodge,       0, 0 },
	{ "suicide",           BotActionSuicide,           0, 0 },
	{ "teleport",          BotActionTeleport,          3, 3 },
};

/*
======================
AIActionNode constructor

Parses and creates an AIGenericNode with the type ACTION_NODE from a token list
The token list pointer is modified to point to the beginning of the next node text block after reading

An action node has the form:
action name( p1, p2, ... )

Where name defines the action to execute, and the parameters are surrounded by parenthesis
======================
*/

AIActionNode::AIActionNode( pc_token_list **tokenlist )
	: AIGenericNode{ ACTION_NODE, nullptr }, params()
{
	pc_token_list *current = *tokenlist;
	pc_token_list *parenBegin;
	struct AIActionMap_s *action = nullptr;

	expectToken( "action", &current, true );

	if ( !current )
	{
		throw parseError( "unexpected end of file after line", (*tokenlist)->token.line );
	}

	action = (struct AIActionMap_s*) bsearch( current->token.string, AIActions, ARRAY_LEN( AIActions ), sizeof( *AIActions ), cmdcmp );

	if ( !action )
	{
		*tokenlist = current;
		throw parseError( "invalid action" + current->token.line );
	}

	parenBegin = current->next;

	run = action->run;

	// allow dropping of parenthesis if we don't require any parameters
	if ( action->minparams == 0 && parenBegin->token.string[0] != '(' )
	{
		*tokenlist = parenBegin;
		return;
	}

	params = parseFunctionParameters( &current, action->minparams, action->maxparams );

	if ( params.size() == 0 && action->minparams > 0 )
	{
		throw parseError("could not parse function parameters", current->prev->token.line);
	}

	*tokenlist = current;
}

/*
======================
AINodeList constructor

Parses and creates an AINodeList from a token list
The token list pointer is modified to point to the beginning of the next node text block after reading
======================
*/

AINodeList::AINodeList( AINodeRunner _run, pc_token_list **tokenlist )
	: AIGenericNode{ SELECTOR_NODE, _run }, list()
{
	pc_token_list *current = (*tokenlist)->next; // TODO: check throw works

	if ( !expectToken( "{", &current, true ) )
	{
		throw parseError( "missing opening {", current->token.line );
	}

	while ( Q_stricmp( current->token.string, "}" ) )
	{
		std::shared_ptr<AIGenericNode> node = ReadNode( &current );
		if ( node )
		{
			list.push_back( node );
		}
		else
		{
			*tokenlist = current;
			throw parseError( "could not read a child of a node list", current->token.line );
		}

		if ( current == nullptr )
		{
			*tokenlist = nullptr;
			return;
		}
	}

	*tokenlist = current->next;
}

static AITreeList *currentList = nullptr;

std::shared_ptr<AIBehaviorTree> ReadBehaviorTree(const char *name, AITreeList *list)
{
	std::string behavior_name = name;
	currentList = list;

	// check if this behavior tree has already been loaded
	for ( const std::shared_ptr<AIBehaviorTree>& behavior : *currentList )
	{
		if ( behavior->name == behavior_name )
		{
			return behavior;
		}
	}

	auto behavior = std::make_shared<AIBehaviorTree>( std::move(behavior_name) );

	if ( behavior )
	{
		currentList->push_back( behavior );
	}
	return behavior;
}

static std::shared_ptr<AIGenericNode> ReadBehaviorTreeInclude( pc_token_list **tokenlist )
{
	pc_token_list *first = *tokenlist;
	pc_token_list *current = first;

	expectToken( "behavior", &current, true );

	if ( !current )
	{
		*tokenlist = current;
		throw parseError("unexpected end of file while parsing external file");
	}

	std::shared_ptr<AIBehaviorTree> behavior =
		ReadBehaviorTree( current->token.string, currentList );

	if ( !behavior->root )
	{
		*tokenlist = current->next;
		throw parseError( "recursive behavior", current->prev->token.line );
	}

	*tokenlist = current->next;
	return behavior->root;
}

/*
======================
ReadNode

Parses and creates an AIGenericNode from a token list
The token list pointer is modified to point to the next node text block after reading

This function delegates the reading to the sub functions
ReadNodeList, ReadActionNode, and ReadConditionNode depending on the first token in the list
======================
*/

std::shared_ptr<AIGenericNode> ReadNode( pc_token_list **tokenlist )
{
	pc_token_list *current = *tokenlist;
	std::shared_ptr<AIGenericNode> node;

	if ( !Q_stricmp( current->token.string, "selector" ) )
	{
		node = (std::shared_ptr<AIGenericNode>) std::make_shared<AINodeList>( BotSelectorNode, &current );
	}
	else if ( !Q_stricmp( current->token.string, "sequence" ) )
	{
		node = (std::shared_ptr<AIGenericNode>) std::make_shared<AINodeList>( BotSequenceNode, &current );
	}
	else if ( !Q_stricmp( current->token.string, "concurrent" ) )
	{
		node = (std::shared_ptr<AIGenericNode>) std::make_shared<AINodeList>( BotConcurrentNode, &current );
	}
	else if ( !Q_stricmp( current->token.string, "action" ) )
	{
		node = (std::shared_ptr<AIGenericNode>) std::make_shared<AIActionNode>( &current );
	}
	else if ( !Q_stricmp( current->token.string, "condition" ) )
	{
		node = (std::shared_ptr<AIGenericNode>) std::make_shared<AIConditionNode>( &current );
	}
	else if ( !Q_stricmp( current->token.string, "decorator" ) )
	{
		node = (std::shared_ptr<AIGenericNode>) std::make_shared<AIDecoratorNode>( &current );
	}
	else if ( !Q_stricmp( current->token.string, "behavior" ) )
	{
		node = ReadBehaviorTreeInclude( &current );
	}
	else
	{
		throw parseError( "invalid token found", current->token.line );
	}

	*tokenlist = current;
	return node;
}

/*
======================
AIBehaviorTree constructor

Load a behavior tree of the given name from a file
======================
*/

AIBehaviorTree::AIBehaviorTree( std::string _name )
	: AIGenericNode{ BEHAVIOR_NODE, BotBehaviorNode },
	  name(std::move(_name)), root(nullptr)
{
	char treefilename[ MAX_QPATH ];
	int handle;
	pc_token_list *tokenlist;

	// add preprocessor defines for use in the behavior tree
	// add upgrades
	D( UP_LIGHTARMOUR );
	D( UP_MEDIUMARMOUR );
	D( UP_BATTLESUIT );
	D( UP_RADAR );
	D( UP_JETPACK );
	D( UP_GRENADE );
	D( UP_MEDKIT );

	// add weapons
	D( WP_MACHINEGUN );
	D( WP_PAIN_SAW );
	D( WP_SHOTGUN );
	D( WP_LAS_GUN );
	D( WP_MASS_DRIVER );
	D( WP_CHAINGUN );
	D( WP_FLAMER );
	D( WP_PULSE_RIFLE );
	D( WP_LUCIFER_CANNON );
	D( WP_HBUILD );

	// add teams
	D( TEAM_ALIENS );
	D( TEAM_HUMANS );
	D( TEAM_NONE );

	// add AIEntitys
	D( E_NONE );
	D( E_A_SPAWN );
	D( E_A_OVERMIND );
	D( E_A_BARRICADE );
	D( E_A_ACIDTUBE );
	D( E_A_TRAPPER );
	D( E_A_BOOSTER );
	D( E_A_HIVE );
	D( E_A_LEECH );
	D( E_H_SPAWN );
	D( E_H_MGTURRET );
	D( E_H_ROCKETPOD );
	D( E_H_ARMOURY );
	D( E_H_MEDISTAT );
	D( E_H_DRILL );
	D( E_H_REACTOR );
	D( E_GOAL );
	D( E_ENEMY );
	D( E_DAMAGEDBUILDING );
	D( E_SELF );

	// add player classes
	D( PCL_NONE );
	D( PCL_ALIEN_BUILDER0 );
	D( PCL_ALIEN_BUILDER0_UPG );
	D( PCL_ALIEN_LEVEL0 );
	D( PCL_ALIEN_LEVEL1 );
	D( PCL_ALIEN_LEVEL2 );
	D( PCL_ALIEN_LEVEL2_UPG );
	D( PCL_ALIEN_LEVEL3 );
	D( PCL_ALIEN_LEVEL3_UPG );
	D( PCL_ALIEN_LEVEL4 );
	D( PCL_HUMAN_NAKED );
	D( PCL_HUMAN_LIGHT );
	D( PCL_HUMAN_MEDIUM );
	D( PCL_HUMAN_BSUIT );

	D( MOVE_FORWARD );
	D( MOVE_BACKWARD );
	D( MOVE_RIGHT );
	D( MOVE_LEFT );

	D2( ET_BUILDABLE, Util::ordinal(entityType_t::ET_BUILDABLE) );

	// node return status
	D( STATUS_RUNNING );
	D( STATUS_SUCCESS );
	D( STATUS_FAILURE );

	D( SAY_ALL );
	D( SAY_TEAM );
	D( SAY_AREA );
	D( SAY_AREA_TEAM );

	Q_strncpyz( treefilename, va( "bots/%s.bt", name.c_str() ), sizeof( treefilename ) );

	handle = Parse_LoadSourceHandle( treefilename );
	if ( !handle )
	{
		throw "cannot load behavior tree: File not found";
	}

	tokenlist = CreateTokenList( handle );
	Parse_FreeSourceHandle( handle );

	root = ReadNode( &tokenlist );
	FreeTokenList( tokenlist );
	if ( !root ) {
		throw "could not parse behavior tree root node";
	}
}

pc_token_list *CreateTokenList( int handle )
{
	pc_token_t token;
	char filename[ MAX_QPATH ];
	pc_token_list *current = nullptr;
	pc_token_list *root = nullptr;

	while ( Parse_ReadTokenHandle( handle, &token ) )
	{
		pc_token_list *list = ( pc_token_list * ) BG_Alloc( sizeof( pc_token_list ) );

		if ( current )
		{
			list->prev = current;
			current->next = list;
		}
		else
		{
			list->prev = list;
			root = list;
		}

		current = list;
		current->next = nullptr;

		current->token.floatvalue = token.floatvalue;
		current->token.intvalue = token.intvalue;
		current->token.subtype = token.subtype;
		current->token.type = token.type;
		current->token.string = BG_strdup( token.string );
		Parse_SourceFileAndLine( handle, filename, &current->token.line );
	}

	return root;
}

void FreeTokenList( pc_token_list *list )
{
	pc_token_list *current = list;
	while( current )
	{
		pc_token_list *node = current;
		current = current->next;

		BG_Free( node->token.string );
		BG_Free( node );
	}
}

void RemoveTreeFromList( AIBehaviorTree *tree, AITreeList *list )
{
	for ( auto t = list->begin(); t != list->end(); ++t )
	{
		if ( t->get()->name == tree->name )
		{
			list->erase(t);
			return;
		}
	}
}
