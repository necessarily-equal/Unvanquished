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

// Cleanly create a juggernaut, this will send messages events and all
static void G_PromoteJuggernaut( gentity_t *new_juggernaut = nullptr )
{
	if (!new_juggernaut)
		new_juggernaut = G_SelectRandomJuggernaut();
	if (!new_juggernaut)
		return;
	Log::Notice("Promoting %p to Juggernaut!", new_juggernaut);

	G_ChangeTeam( new_juggernaut, G_JuggernautTeam() );

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

	//TODO: send messages

	new_juggernaut->client->sess.spectatorState = SPECTATOR_NOT;
	new_juggernaut->client->pers.classSelection = G_JuggernautClass();

	/* bot */
	auto model = BG_ClassModelConfig( new_juggernaut->client->pers.classSelection );
	auto navHandle = model->navMeshClass
	          ? BG_ClassModelConfig( model->navMeshClass )->navHandle
	          : model->navHandle;
	G_BotSetNavMesh( new_juggernaut->s.number, navHandle );

	ClientUserinfoChanged( new_juggernaut->client->ps.clientNum, false );
	ClientSpawn( new_juggernaut, spawn, origin, angles );

	Beacon::Tag( new_juggernaut, G_PreyTeam(), true );
}

// Cleanly removes a juggernaut
static void G_DemoteJuggernaut( gentity_t *old_juggernaut )
{
	Log::Notice("Demoting %p from Juggernaut!", old_juggernaut);
	old_juggernaut->client->pers.classSelection = PCL_NONE;

	G_ChangeTeam( old_juggernaut, G_PreyTeam() );
}

void G_SwitchJuggernaut( gentity_t *new_juggernaut, gentity_t *old_juggernaut )
{
	G_DemoteJuggernaut(  old_juggernaut );
	if (new_juggernaut && new_juggernaut->client)
		G_PromoteJuggernaut(new_juggernaut);
	else
		G_PromoteJuggernaut();

}

// Pick a juggernaut if there is none
void G_CheckAndSpawnJuggernaut()
{
	gentity_t *juggernaut = nullptr;
	for ( gentity_t *ent = nullptr; (ent = G_IterateEntities(ent)); )
	{
		if ( ent && ent->client && G_Team(ent) == G_JuggernautTeam() )
		{
			if (!juggernaut)
			{
				juggernaut = ent;
			}
			else
			{
				// downgrade the extra juggernaut
				Log::Warn("Argh, one juggernaut too many");
				G_ChangeTeam( juggernaut, G_PreyTeam() );
			}
		}
	}

	if (juggernaut)
		return;

	Log::Notice("New Juggernaut!");

	G_PromoteJuggernaut();
}
