#include "sg_juggernaut.h"
#include "sg_bot_local.h"
#include "sg_bot_util.h"
#include "CBSE.h"
#include "Entities.h"

Log::Logger juggernautLogger = Log::Logger("juggernaut", "", Log::Level::NOTICE).WithoutSuppression();

// TODO: make this an enum cvar, someday
static Cvar::Cvar<int> g_juggernautTeam("g_juggernautTeam",
		"juggernaut team", Cvar::NONE, (int)TEAM_ALIENS);

static Cvar::Range<Cvar::Cvar<int>> g_juggernautMinChallengerCount(
		"g_juggernautMinChallengerCount",
		"minimun number of player to assign the first juggernaut",
		Cvar::NONE, 3, 2, 100);

static inline class_t G_JuggernautClass()
{
	switch (G_JuggernautTeam())
	{
		case TEAM_ALIENS: return PCL_ALIEN_LEVEL3_UPG;
		case TEAM_HUMANS: return PCL_HUMAN_BSUIT;
				  //TODO: add weapon
		default: ASSERT_UNREACHABLE();
	}
}

team_t G_JuggernautTeam()
{
	// wrapper for type safety
	return g_juggernautTeam.Get() == TEAM_ALIENS ?
			TEAM_ALIENS : TEAM_HUMANS;
}

team_t G_PreyTeam()
{
	return g_juggernautTeam.Get() == TEAM_ALIENS ?
			TEAM_HUMANS : TEAM_ALIENS;
}

// Look at people that might be juggernaut and return a random one
gentity_t *G_SelectRandomJuggernaut()
{
	const team_t preyTeam = G_PreyTeam();
	const int numChallengers = level.team[ preyTeam ].numClients;
	if ( numChallengers >= g_juggernautMinChallengerCount.Get() )
	{
		float count = static_cast<float>( numChallengers );

		for ( gentity_t *ent = nullptr; (ent = G_IterateEntities(ent)); )
		{
			if ( ent && ent->client && G_Team(ent) == preyTeam )
			{
				if ( random() <= (1.0f / count--) )
				{
					return ent;
				}
			}
		}
	}
	return nullptr;
}

// Mark a client to become juggernaut
// This function is expected to be called only by G_SwitchJuggernaut
static void G_PromoteJuggernaut( gentity_t *new_juggernaut )
{
	if (!new_juggernaut)
	{
		Log::Warn("No designated juggernaut, choosing a new juggernaut randomly");
		new_juggernaut = G_SelectRandomJuggernaut();
	}
	if (!new_juggernaut)
		return; // give up

	new_juggernaut->client->sess.restartTeam = G_JuggernautTeam();
}

// Cleanly create a juggernaut, this will send messages events and all
static void G_SpawnJuggernaut( gentity_t *new_juggernaut )
{
	// Shout "Le roi est mort, vive le roi!"
	G_BroadcastEvent(EV_NEW_JUGGERNAUT, 0, G_PreyTeam());

	// Get the spawn position
	gentity_t *spawn;
	vec3_t origin;
	vec3_t angles;
	//if (Entities::IsAlive(new_juggernaut))
	//{
	//	Log::Warn("spawning from an alive juggernaut");
	//	spawn = new_juggernaut;
	//	// Eww vector lib
	//	const float *o = new_juggernaut->s.origin;
	//	const float *a = new_juggernaut->s.angles;
	//	VectorCopy( o, origin );
	//	VectorCopy( a, angles );
	//}
	//else
	//{
		spawn = G_PickRandomEntityOfClass("team_alien_spawn");
		const float *o = spawn->s.origin;
		const float *a = spawn->s.angles;
		VectorCopy( o, origin );
		VectorCopy( a, angles );
	//}

	//TODO: send messages

	new_juggernaut->client->sess.spectatorState = SPECTATOR_NOT;
	new_juggernaut->client->pers.classSelection = G_JuggernautClass();
	ClientSpawn( new_juggernaut, spawn, origin, angles );
	ClientUserinfoChanged( new_juggernaut->client->ps.clientNum, false );

	BotSetNavmesh( new_juggernaut, G_JuggernautClass() );

	// Tell everyone it's here
	Beacon::Tag( new_juggernaut, G_PreyTeam(), true );
}

// Cleanly removes a juggernaut
// This function is expected to be called only by G_SwitchJuggernaut
static void G_DemoteJuggernaut( gentity_t *old_juggernaut )
{
	old_juggernaut->client->pers.classSelection = PCL_NONE;
	old_juggernaut->client->sess.restartTeam = G_PreyTeam();
}

void G_SwitchJuggernaut( gentity_t *new_juggernaut )
{
	const team_t juggernautTeam = G_JuggernautTeam();
	for ( gentity_t *ent = nullptr; (ent = G_IterateEntities(ent)); )
	{
		if ( ent && ent->client && G_Team(ent) == juggernautTeam )
		{
			G_DemoteJuggernaut(ent);
		}
	}

	G_PromoteJuggernaut(new_juggernaut);
}

static void G_AssignTeam(gentity_t *ent)
{
	team_t target_team = ent->client->sess.restartTeam;
	Log::Notice("Assigning to team %i", target_team);

	ent->client->sess.restartTeam = TEAM_NONE;
	//ASSERT(target_team > TEAM_NONE && target_team < TEAM_ALL);
	//if (target_team <= TEAM_NONE || target_team >= TEAM_ALL)
	//	return;

	// clear shit
	G_UnlaggedClear(ent);
	auto flags = ent->client->ps.eFlags;
	memset( &ent->client->ps, 0, sizeof ent->client->ps );
	memset( &ent->client->pmext, 0, sizeof ent->client->pmext );
	ent->client->ps.eFlags = flags;

	G_ChangeTeam(ent, target_team);
	ASSERT(G_Team(ent) == target_team);

	if ( target_team == G_JuggernautTeam() )
	{
		G_SpawnJuggernaut(ent);
	}
	else
	{
		// assign to the other team
		ent->client->sess.spectatorState = SPECTATOR_LOCKED; //free spec?
		ent->client->pers.classSelection = PCL_NONE;
		ClientSpawn( ent, nullptr, nullptr, nullptr );
		ClientUserinfoChanged( ent->client->ps.clientNum, false );
	}
}

// Check that there is the correct number of juggernaut playing, if not,
// schedule some.
static void G_EnsureJuggernaut()
{
	int juggernaut_count = 0;

	for ( gentity_t *ent = nullptr; (ent = G_IterateEntities(ent)); )
	{
		if ( !ent || !ent->client )
			continue;

		if ( G_Team(ent) == G_JuggernautTeam() && Entities::IsAlive(ent) )
		{
			juggernaut_count++;
		}
	}

	if (juggernaut_count > 1)
	{
		Log::Warn("Huh oh, we have %i juggernauts", juggernaut_count);
	}

	if (juggernaut_count != 1)
	{
		G_SwitchJuggernaut(); // stabilise the situation and mark one to become juggernaut on next frame
	}
}

// The rest of the code uses G_SwitchJuggernaut which calls
// G_{Promote,Demote}Juggernaut to mark the next turn's jugg. This will
// actually commit and execute this decision.
//
// We do so in a predefined place in the code, because game logic tends to be
// fragile and we want things to be as reliable as possible.
static void G_CreateJuggernaut()
{
	for ( gentity_t *ent = nullptr; (ent = G_IterateEntities(ent)); )
	{
		if ( !ent || !ent->client )
			continue;

		if ( ent->client->sess.restartTeam != TEAM_NONE )
		{
			G_AssignTeam( ent );
		}
	}
}

// Pick a juggernaut if there is none
void G_CheckAndSpawnJuggernaut()
{
	G_CreateJuggernaut(); // apply changes that were already scheduled
	G_EnsureJuggernaut();
	G_CreateJuggernaut(); // and apply our new changes immediately
}
