#ifndef ENVIRONMENT_HEADER
#define ENVIRONMENT_HEADER

/*
	This class is the game's environment.
	It contains:
	- The map
	- Players
	- Other objects
	- The current time in the game, etc.
*/

#include <list>
#include "common_irrlicht.h"
#include "player.h"
#include "map.h"
#include <ostream>
#define MAPSIZE 256
class Environment
{
public:
	// Environment will delete the map passed to the constructor
    Environment(std::ostream &dout);
	Environment(Map *map, std::ostream &dout);
	~Environment();
	/*
		This can do anything to the environment, such as removing
		timed-out players.
	*/
	void step(f32 dtime);

	// -----TODO: initialize a Map of MAPSIZE*2*MAPSIZE*2*MAPSIZE*2 when starting-----
	void initMap(int size);

	// -----TODO: some data structure to save the map-----
	void saveMap();

	// -----warper for Map::updateCamera(v3f pos, v3f dir)-----
	void updateCamera(v3f pos, v3f dir){
	    m_map.updateCamera(pos, dir);
	}

	Map & getMap();
	/*
		Environment deallocates players after use.
	*/
	void addPlayer(Player *player);
	Player * getPlayer();
private:
	Map *m_map;
    Player * m_player;

	// Debug output goes here
	std::ostream &m_dout;

	int m_step_counter = 0;
	int m_step_max = 50; // max game step to reset walking direction
};

#endif

