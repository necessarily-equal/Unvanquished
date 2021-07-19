/*
===========================================================================

Unvanquished GPL Source Code
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2021 Antoine Fontaine

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

#include "sg_bot_behavior_tree.h"

/*
======================
sg_bot_behavior_tree.cpp

This file contains the implementation of the different behavior tree nodes

On each frame, the behavior tree for each bot is evaluated starting from the root node.
Each state will be evaluated to either STATUS_SUCCESS, STATUS_RUNNING, or STATUS_FAILURE when they are run.
The return values are used in various sequences and selectors to change the execution of the tree.
======================
*/


/*
======================
Values

This class represents any type of value in the tree. For example literals or
what is returned by a function.
======================
*/

AIValue_t::AIValue_t(bool b)
	: AIValue_t( (int) b )
{
}

AIValue_t::AIValue_t(int i) : valType(VALUE_INT)
{
	l.intValue = i;
}

AIValue_t::AIValue_t(float f) : valType(VALUE_FLOAT)
{
	l.floatValue = f;
}

AIValue_t::AIValue_t(const char *s) : valType(VALUE_STRING)
{
	l.stringValue = BG_strdup(s);
}

AIValue_t::AIValue_t(const AIValue_t& other) : valType(other.valType)
{
	switch ( other.valType )
	{
		case VALUE_INT:
			l.intValue = other.l.intValue;
			break;
		case VALUE_FLOAT:
			l.floatValue = other.l.floatValue;
			break;
		case VALUE_STRING:
			l.stringValue = BG_strdup(other.l.stringValue);
			break;
		default:
			ASSERT_UNREACHABLE();
	}
}

AIValue_t::AIValue_t(AIValue_t&& other) : valType(other.valType)
{
	switch ( other.valType )
	{
		case VALUE_INT:
			l.intValue = other.l.intValue;
			break;
		case VALUE_FLOAT:
			l.floatValue = other.l.floatValue;
			break;
		case VALUE_STRING:
			l.stringValue = other.l.stringValue;
			other.l.stringValue = nullptr;
			break;
		default:
			ASSERT_UNREACHABLE();
	}
}

AIValue_t::operator bool() const
{
	return (float)*this != 0.0f;
}

AIValue_t::operator int() const
{
	switch ( valType )
	{
		case VALUE_FLOAT:
			return ( int ) l.floatValue;
		case VALUE_INT:
			return l.intValue;
		default:
			return 0;
	}
}

AIValue_t::operator float() const
{
	switch ( valType )
	{
		case VALUE_FLOAT:
			return l.floatValue;
		case VALUE_INT:
			return ( float ) l.intValue;
		default:
			return 0.0f;
	}
}

AIValue_t::operator double() const
{
	return (double) (float) *this;
}

AIValue_t::operator const char *() const
{
	const static char empty[] = "";

	switch ( valType )
	{
		case VALUE_FLOAT:
			return va( "%f", l.floatValue );
		case VALUE_INT:
			return va( "%d", l.intValue );
		case VALUE_STRING:
			return l.stringValue;
		default:
			return empty;
	}
}

AIValue_t::~AIValue_t()
{
	switch ( valType )
	{
		case VALUE_STRING:
			BG_Free( l.stringValue );
			break;
		default:
			break;
	}
}


/*
======================
Operators
======================
*/

bool isBinaryOp( AIOpType_t op )
{
	switch ( op )
	{
		case OP_GREATERTHAN:
		case OP_GREATERTHANEQUAL:
		case OP_LESSTHAN:
		case OP_LESSTHANEQUAL:
		case OP_EQUAL:
		case OP_NEQUAL:
		case OP_AND:
		case OP_OR:
			return true;
		default:
			return false;
	}
}

bool isUnaryOp( AIOpType_t op )
{
	return op == OP_NOT;
}

AIValue_t AIBinaryOp_t::eval( gentity_t *bot ) const
{
	bool success = false;
	switch ( opType )
	{
		case OP_LESSTHAN:
			success = (double) exp1->eval( bot ) <  (double) exp2->eval( bot );
			break;
		case OP_LESSTHANEQUAL:
			success = (double) exp1->eval( bot ) <= (double) exp2->eval( bot );
			break;
		case OP_GREATERTHAN:
			success = (double) exp1->eval( bot ) >  (double) exp2->eval( bot );
			break;
		case OP_GREATERTHANEQUAL:
			success = (double) exp1->eval( bot ) >= (double) exp2->eval( bot );
			break;
		case OP_EQUAL:
			success = (double) exp1->eval( bot ) == (double) exp2->eval( bot );
			break;
		case OP_NEQUAL:
			success = (double) exp1->eval( bot ) != (double) exp2->eval( bot );
			break;
		case OP_AND:
			success = (bool) exp1->eval( bot ) && (bool) exp2->eval( bot );
			break;
		case OP_OR:
			success = (bool) exp1->eval( bot ) || (bool) exp2->eval( bot );
			break;
		default:
			success = false;
			ASSERT_UNREACHABLE();
			break;
	}
	return AIValue_t( success );
}

AIValue_t AIUnaryOp_t::eval( gentity_t *bot ) const
{
	ASSERT_EQ(opType, OP_NOT);
	return AIValue_t( !(bool)exp->eval( bot ) );
}

/*
======================
Generic Node
======================
*/

bool AIGenericNode::isRunning(gentity_t *bot, BTMemory &mem) const
{
	return runningStates[bot->s.number];
}

void AIGenericNode::setRunning(gentity_t *bot, BTMemory &runningNodes, bool running)
{
	bool previously_running = runningStates[bot->s.number];
	runningStates[bot->s.number] = running;

	printf("\n## running: %i, previously_running: %i\n\n", running, previously_running);//FIXME

	// reset the current node if it finishes
	// we do this so we can re-pathfind on the next entrance
	if ( !running && bot->botMind->currentNode == this )
	{
		bot->botMind->currentNode = nullptr;
	}

	if ( previously_running && !running )
	{
		//ASSERT(runningNodes.size() > 0);
		for (auto i = runningNodes.begin(); i != runningNodes.end(); ++i)
		{
			if (*i == this)
			{
				runningNodes.erase(i);
				break;
			}
		}
	}
	else if ( !previously_running && running )
	{
		ASSERT(runningNodes.size() < 200);
		runningNodes.push_back(this);
	}
/*TODO: delete this comment
       // reset running information on node success so sequences and selectors reset their state
       if ( NodeIsRunning( self, node ) && status == STATUS_SUCCESS )
       {
               memset( self->botMind->runningNodes, 0, sizeof( self->botMind->runningNodes ) );
               self->botMind->numRunningNodes = 0;
       }

       // store running information for sequence nodes and selector nodes
       if ( status == STATUS_RUNNING )
       {
               // clear out previous running list when we hit a running leaf node
               // this insures that only 1 node in a sequence or selector has the running state
               if ( node->type == ACTION_NODE )
               {
                       memset( self->botMind->runningNodes, 0, sizeof( self->botMind->runningNodes ) );
                       self->botMind->numRunningNodes = 0;
               }

               if ( !NodeIsRunning( self, node ) )
               {
                       self->botMind->runningNodes[ self->botMind->numRunningNodes++ ] = node;
               }
       }
*/
}

/*
======================
AIBehaviorTree::eval

This is the interface that should be used from outside the behavior tree.
======================
*/
void AIBehaviorTree::eval(gentity_t *bot)
{
	this->run( bot, memory );
}

/*
======================
AIBehaviorTree::run

Generic node running routine that properly handles
running information for sequences and selectors
======================
*/
AINodeStatus_t AIGenericNode::run(gentity_t *bot, std::vector<AIGenericNode*> &running_nodes)
{
	AINodeStatus_t status = this->exec( bot, running_nodes );
	setRunning(bot, running_nodes, status == STATUS_RUNNING);
	return status;
}

/*
======================
Sequences and Selectors

A sequence or selector contains a list of child nodes which are evaluated
based on a combination of the child node return values and the internal logic
of the sequence or selector

A selector evaluates its child nodes like an if ( ) else if ( ) loop
It starts at the first child node, and if the node did not fail, it returns its status
if the node failed, it evaluates the next child node in the list
A selector will fail if all of its child nodes fail

A sequence evaluates its child nodes like a series of statements
It starts at the first previously running child node, and if the node does not succeed, it returns its status
If the node succeeded, it evaluates the next child node in the list
A sequence will succeed if all of its child nodes succeed

A concurrent node will always evaluate all of its child nodes unless one fails
if one fails, the concurrent node will stop executing nodes and return failure
A concurrent node succeeds if none of its child nodes fail
======================
*/

AINodeList::AINodeList(
		std::vector<std::shared_ptr<AIGenericNode>> _list )
	: list( std::move(_list) )
{
}

AISelectorNode::AISelectorNode(
		std::vector<std::shared_ptr<AIGenericNode>> list )
	: AINodeList( std::move(list) )
{
}

AINodeStatus_t AISelectorNode::exec( gentity_t *bot, BTMemory &mem )
{
	for ( const std::shared_ptr<AIGenericNode>& node : list )
	{
		AINodeStatus_t status = node->run( bot, mem );
		if ( status == STATUS_FAILURE )
		{
			continue;
		}
		else if ( status == STATUS_SUCCESS )
		{
			// cleanup
			for ( auto node : list )
			{
				node->setRunning(bot, mem, false);
			}
		}
		return status;
	}

	return STATUS_FAILURE;
}

AISequenceNode::AISequenceNode(
		std::vector<std::shared_ptr<AIGenericNode>> list )
	: AINodeList( std::move(list) )
{
}

AINodeStatus_t AISequenceNode::exec( gentity_t *bot, BTMemory &mem )
{
	size_t i;

	// find a previously running node and start there
	for ( i = list.size() - 1; i > 0; i-- )
	{
		if ( list[ i ]->isRunning( bot, mem ) )
		{
			break;
		}
	}

	for ( ; i < list.size(); i++ )
	{
		AINodeStatus_t status = list[ i ]->run( bot, mem );
		if ( status == STATUS_FAILURE )
		{
			return STATUS_FAILURE;
		}

		if ( status == STATUS_RUNNING )
		{
			return STATUS_RUNNING;
		}
	}

	// cleanup
	for ( auto node : list )
	{
		node->setRunning(bot, mem, false);
	}
	return STATUS_SUCCESS;
}

AIConcurrentNode::AIConcurrentNode(
		std::vector<std::shared_ptr<AIGenericNode>> list )
	: AINodeList( std::move(list) )
{
}

AINodeStatus_t AIConcurrentNode::exec( gentity_t *bot, BTMemory &mem )
{
	for ( const std::shared_ptr<AIGenericNode>& node : list )
	{
		AINodeStatus_t status = node->run( bot, mem );

		if ( status == STATUS_FAILURE )
		{
			return STATUS_FAILURE;
		}
	}
	return STATUS_SUCCESS;
}


/*
======================
Decorators

Decorators are used to add functionality to the child node
======================
*/

AIDecoratorNode::AIDecoratorNode( AIDecoratorRunner _f,
		std::shared_ptr<AIGenericNode> _child,
		std::vector<AIValue_t> _params )
	: f(_f), child(std::move(_child)), params(std::move(_params))
{
}

AINodeStatus_t AIDecoratorNode::exec( gentity_t *bot, BTMemory &mem )
{
	return f( bot, mem, this );
}

/*
======================
Actions

Actions are the leaves of the tree, they allow the tree to do something
======================
*/
AINodeStatus_t AIActionNode::exec( gentity_t *bot, BTMemory &mem )
{
	return f( bot, this );
}

AIActionNode::AIActionNode( AIActionRunner _f, std::vector<AIValue_t> _params)
	: f(_f), params(std::move(_params))
{
}


/*
======================
BehaviorTree
======================
*/

AIBehaviorTree::AIBehaviorTree(std::string _name,
		std::shared_ptr<AIGenericNode> _root)
	: root(std::move(_root)), name(std::move(_name))
{
}


/*
======================
AIConditionNode

Runs the child node if the condition expression is true
If there is no child node, returns success if the conditon expression is true
returns failure otherwise
======================
*/

AIConditionNode::AIConditionNode( std::unique_ptr<AIExpression_t> _exp, std::shared_ptr<AIGenericNode> _child )
	: child(std::move(_child)), exp(std::move(_exp))
{
}

AINodeStatus_t AIConditionNode::exec( gentity_t *bot, BTMemory &mem )
{
	bool success = (bool) exp->eval( bot );
	if ( success )
	{
		if ( child )
		{
			return child->run( bot, mem );
		}
		else
		{
			return STATUS_SUCCESS;
		}
	}

	return STATUS_FAILURE;
}
