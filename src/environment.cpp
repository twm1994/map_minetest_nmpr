#include "environment.h"

Environment::Environment(Map *map, std::ostream &dout) :
		m_dout(dout) {
	m_map = map;
	initMap(MAPSIZE);
}

Environment::~Environment() {
	delete m_player;
}

void Environment::initMap(int size) {

	for (int y = 0; y < size; y++) {

		for (int z = 0; z < size; z++) {

			for (int x = 0; x < size; x++) {
				v3s16 p = v3s16(x, y, z);
				m_map->getNode(p);
			} // for(int x=-size;x<size;x++)
		} // for(int z=-size;z<size;z++)
	} // for(int y=-size;y<size;y++)
}

void Environment::saveMap() {

}

void Environment::step(f32 dtime) {
	f32 maximum_player_speed = 0.001; // just some small value
	f32 speed = m_player->speed.getLength();
	if (speed > maximum_player_speed)
		maximum_player_speed = speed;

	// Maximum time increment (for collision detection etc)
	// Allow 0.1 blocks per increment
	// time = distance / speed
	f32 dtime_max_increment = 0.1 * BS / maximum_player_speed;

	// Maximum time increment is 10ms or lower
	if (dtime_max_increment > 0.01)
		dtime_max_increment = 0.01;

	/*
	 Stuff that has a maximum time increment
	 */
	// Don't allow overly too much dtime
	if (dtime > 0.5)
		dtime = 0.5;
	do {
		f32 dtime_part;
		if (dtime > dtime_max_increment)
			dtime_part = dtime_max_increment;
		else
			dtime_part = dtime;
		dtime -= dtime_part;
		m_player->move(dtime_part, *m_map);
	} while (dtime > 0.001);
}

Map & Environment::getMap() {
	return *m_map;
}

void Environment::addPlayer(Player *player) {
	m_player = player;
}

Player * Environment::getPlayer() {
	return m_player;
}

