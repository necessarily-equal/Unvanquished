#include "sg_local.h"
#include "sg_bot_local.h"
#include "CBSE.h"
#include "Entities.h"


static inline team_t G_JuggernautTeam()
{
	return static_cast<team_t>( g_juggernautTeam.Get() );
}

static inline team_t G_PreyTeam()
{
	return G_JuggernautTeam() == TEAM_ALIENS ?
			TEAM_HUMANS : TEAM_ALIENS;
}

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

// TODO: make this a cvar
constexpr int MIN_CHALLENGER_COUNT = 3;

// Look at people that might be juggernaut and return a random one
gentity_t *G_SelectRandomJuggernaut()
{
	const team_t preyTeam = G_PreyTeam();
	const int numChallengers = level.team[ preyTeam ].numClients;
	if ( numChallengers >= MIN_CHALLENGER_COUNT )
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
static void G_PromoteJuggernaut( gentity_t *new_juggernaut = nullptr )
{
	if (!new_juggernaut)
		new_juggernaut = G_SelectRandomJuggernaut();
	if (!new_juggernaut)
		return; // give up

	new_juggernaut->client->sess.restartTeam = G_JuggernautTeam();
}

// Cleanly create a juggernaut, this will send messages events and all
static void G_SpawnJuggernaut( gentity_t *new_juggernaut )
{
	// Get the spawn position
	gentity_t *spawn;
	vec3_t origin;
	vec3_t angles;
	if (Entities::IsAlive(new_juggernaut))
	{
		Log::Warn("spawning from an alive juggernaut");
		spawn = new_juggernaut;
		// Eww vector lib
		const float *o = new_juggernaut->s.origin;
		const float *a = new_juggernaut->s.angles;
		VectorCopy( o, origin );
		VectorCopy( a, angles );
	}
	else
	{
		Log::Warn("spawning from a non-spawned juggernaut");
		spawn = G_SelectAlienLockSpawnPoint( origin, angles );
		origin[2] -= 30;
		// TODO: SpotWouldTelefrag?
	}

	G_ChangeTeam( new_juggernaut, G_JuggernautTeam() );

	//TODO: send messages

	/* bot */
	auto model = BG_ClassModelConfig( new_juggernaut->client->pers.classSelection );
	auto navHandle = model->navMeshClass
	          ? BG_ClassModelConfig( model->navMeshClass )->navHandle
	          : model->navHandle;
	G_BotSetNavMesh( new_juggernaut->s.number, navHandle );

	new_juggernaut->client->sess.spectatorState = SPECTATOR_NOT;
	new_juggernaut->client->pers.classSelection = G_JuggernautClass();
	ClientSpawn( new_juggernaut, spawn, origin, angles );
	ClientUserinfoChanged( new_juggernaut->client->ps.clientNum, false );

	// Tell everyone it's here
	Beacon::Tag( new_juggernaut, G_PreyTeam(), true );
}

//// Cleanly removes a juggernaut
//static void G_DemoteJuggernaut( gentity_t *old_juggernaut )
//{
//	Log::Notice("Demoting %p from Juggernaut!", old_juggernaut);
//	old_juggernaut->client->pers.classSelection = PCL_NONE;
//
//	ent->client->sess.restartTeam = G_PreyTeam();
//}

void G_SwitchJuggernaut( gentity_t *new_juggernaut, gentity_t *old_juggernaut )
{
	old_juggernaut->client->sess.restartTeam = G_PreyTeam();
	if (new_juggernaut && new_juggernaut->client)
	{
		G_PromoteJuggernaut(new_juggernaut);
	}
	else
	{
		Log::Warn("No killer, choosing a new juggernaut randomly");
		G_PromoteJuggernaut();
	}
}

// Pick a juggernaut if there is none
void G_CheckAndSpawnJuggernaut()
{
	int juggernaut_count = 0;
	for ( gentity_t *ent = nullptr; (ent = G_IterateEntities(ent)); )
	{
		if ( !ent || !ent->client )
			continue;

		// change teams
		team_t target_team = ent->client->sess.restartTeam;
		if ( target_team != TEAM_NONE )
		{
			G_ChangeTeam(ent, target_team);
			ent->client->sess.restartTeam = TEAM_NONE;
			ASSERT(G_Team(ent) == target_team);

			// clear shit
			G_UnlaggedClear(ent);
			auto flags = ent->client->ps.eFlags;
			memset( &ent->client->ps, 0, sizeof( ent->client->ps ) );
			memset( &ent->client->pmext, 0, sizeof( ent->client->pmext ) );
			ent->client->ps.eFlags = flags;
			if ( target_team == G_JuggernautTeam() )
			{
				G_SpawnJuggernaut(ent);
			}
			else
			{
				ent->client->sess.spectatorState = SPECTATOR_LOCKED; //free spec?
				ent->client->pers.classSelection = PCL_NONE;
				ClientSpawn( ent, nullptr, nullptr, nullptr );
				ClientUserinfoChanged( ent->client->ps.clientNum, false );
				//G_FreeEntity(ent);
			}
			//*ent->entity->Get<HealthComponent>() = HealthComponent(*ent->entity, 100.0f);
		}

		if ( G_Team(ent) == G_JuggernautTeam() )
		{
			juggernaut_count++;
		}
	}

	if (juggernaut_count == 1)
		return; // perfect

	if (juggernaut_count == 0)
		G_PromoteJuggernaut(); // mark one to become juggernaut on next frame
	else
		Log::Warn("Huh oh, we have %i juggernauts", juggernaut_count);
}
