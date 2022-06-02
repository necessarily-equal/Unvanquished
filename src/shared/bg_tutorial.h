
enum class tutorialMsg {
	NONE,
	A_BUILD_OVERMIND,
	A_BUILD_EGG,
	//A_BUILD_BOOSTER,
	A_EVOLVE_DRETCH,
	A_EVOLVE_GRANGER,
	//A_EVOLVE_ADVGRANGER,
	A_KILL_HUMANS,
	//A_KILL_REMAINING_HUMANS,
	//A_USE_BARB,
	//A_USE_POUNCE,
	//A_USE_SPIT,

	//H_KILL_ALIENS,
	//H_KILL_REMAINING_ALIENS,
	//H_USE_MEDIKIT,
	CONGRATS,
	COUNT
};

// TODO: move this inside a cpp file
static const char * tutorialShortMsg[] {
	"", // NONE
	"Build the Overmind", // A_BUILD_OVERMIND
	"Place an egg", // A_BUILD_EGG
	//"", // A_BUILD_BOOSTER
	"Evolve to dretch", // A_EVOLVE_DRETCH
	//"Evolve to granger", // A_EVOLVE_GRANGER
	//"Evolve to advanced granger", // A_EVOLVE_ADVGRANGER
	"Kill all humans", // A_KILL_HUMANS
	"Kill the remaining humans", // A_KILL_REMAINING_HUMANS
	//"", // A_USE_BARB
	//"", // A_USE_POUNCE
	//"", // A_USE_SPIT

	//"", // H_KILL_ALIENS
	//"", // H_KILL_REMAINING_ALIENS
	//"", // H_USE_MEDIKIT
	"Success!", // CONGRATS,
};

static const char * tutorialExplainationMsg[] {
	"", // NONE
	"The overmind is the heart of the hivemind. It allows aliens to evolve, and controls other alien buildables", // A_BUILD_OVERMIND
	"Eggs allow you to respawn when you get killed. Loosing all your eggs is a receipe for swift defeat", // A_BUILD_EGG
	//"", // A_BUILD_BOOSTER
	"The dretch is the basic alien attack form. It is fast, sneaky and deadly. Its mastery is critical in difficult fights", // A_EVOLVE_DRETCH
	//"", // A_EVOLVE_GRANGER
	//"", // A_EVOLVE_ADVGRANGER
	"Victory is achieved when there are no humans left, and no humans can teleport back inside the hunting ground", // A_KILL_HUMANS
	"Victory is achieved when there are no humans left, and no humans can teleport back inside the hunting ground", // A_KILL_REMAINING_HUMANS
	//"", // A_USE_BARB
	//"", // A_USE_POUNCE
	//"", // A_USE_SPIT

	//"", // H_KILL_ALIENS
	//"", // H_KILL_REMAINING_ALIENS
	//"", // H_USE_MEDIKIT
	"", // CONGRATS
};
