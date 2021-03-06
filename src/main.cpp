#ifdef _MSC_VER
#pragma comment(lib, "Irrlicht.lib")
#pragma comment(lib, "jthread.lib")
// This would get rid of the console window
//#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define sleep_ms(x) Sleep(x)
#else
#include <unistd.h>
#define sleep_ms(x) usleep(x*1000)
#endif

#include <iostream>
#include <time.h>
#include <jmutexautolock.h>
using namespace jthread;
// JThread 1.3 support
#include "common_irrlicht.h"
#include "map.h"
#include "player.h"
#include "main.h"
#include "environment.h"
#include <string>

const char *g_material_filenames[MATERIALS_COUNT] = { "../data/stone.png",
		"../data/grass.png", "../data/water.png", };

#define FPS_MIN 15
#define FPS_MAX 25

#define VIEWING_RANGE_NODES_MIN MAP_BLOCKSIZE
#define VIEWING_RANGE_NODES_MAX 35

JMutex g_viewing_range_nodes_mutex;
s16 g_viewing_range_nodes = MAP_BLOCKSIZE;

/*
 Random stuff
 */
u16 g_selected_material = 0;

/*
 Debug streams
 - use these to disable or enable outputs of parts of the program
 */


std::ofstream dfile("debug.txt");
std::ostream dout(dfile.rdbuf());
//
//// Connection
//std::ostream dout_con(dfile.rdbuf());
//
//// Server;
//std::ostream dout_server(dfile.rdbuf());
////std::ostream dout_server(dfile_server.rdbuf());
//
//// Client
//std::ostream dout_client(dfile.rdbuf());
//std::ostream dout_client(dfile_client.rdbuf());
Player *player;

class MyEventReceiver: public IEventReceiver {
public:
	// This is the one method that we have to implement
	virtual bool OnEvent(const SEvent& event) {
		// Remember whether each key is down or up
		if (event.EventType == irr::EET_KEY_INPUT_EVENT) {
			keyIsDown[event.KeyInput.Key] = event.KeyInput.PressedDown;
			if (event.KeyInput.Key == KEY_KEY_W
					|| event.KeyInput.Key == KEY_KEY_A
					|| event.KeyInput.Key == KEY_KEY_S
					|| event.KeyInput.Key == KEY_KEY_D) {

				if ((event.KeyInput.PressedDown) && (!walking)) //this will be done once
						{
					walking = true;
					player->animateMove();
				} else if ((!event.KeyInput.PressedDown) && (walking)) //this will be done on key up
						{
					walking = false;
					player->animateStand();
				}

			}
			if ((event.KeyInput.Key == KEY_ESCAPE && event.KeyInput.PressedDown)) {
				if (!isPaused) {
					isPaused = true;
				} else {
					isExit = true;
				}
			}
			if ((event.KeyInput.Key == KEY_KEY_Q && isPaused)) {
				isPaused = false;
			}
		}
		if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
			if (event.MouseInput.Event == EMIE_MOUSE_WHEEL) {
				wheel = event.MouseInput.Wheel;
			}
		}
		return false;
	}
	// This is used to check whether a key is being held down
	virtual bool IsKeyDown(EKEY_CODE keyCode) const {
		return keyIsDown[keyCode];
	}

	MyEventReceiver() {
		for (u32 i = 0; i < KEY_KEY_CODES_COUNT; ++i)
			keyIsDown[i] = false;
		wheel = 0;
		walking = false;
		isPaused = false;
		isExit = false;
	}

	float wheel;
	bool walking;
	bool isPaused;
	bool isExit;
private:
	bool keyIsDown[KEY_KEY_CODES_COUNT];
};

void updateViewingRange(f32 frametime) {
#if 1
	static f32 counter = 0;
	if (counter > 0) {
		counter -= frametime;
		return;
	}
	counter = 5.0; //seconds
	g_viewing_range_nodes_mutex.Lock();
	bool changed = false;
	if (frametime > 1.0 / FPS_MIN
			|| g_viewing_range_nodes > VIEWING_RANGE_NODES_MAX) {
		if (g_viewing_range_nodes > VIEWING_RANGE_NODES_MIN) {
			g_viewing_range_nodes -= MAP_BLOCKSIZE / 2;
			changed = true;
		}
	} else if (frametime < 1.0 / FPS_MAX
			|| g_viewing_range_nodes < VIEWING_RANGE_NODES_MIN) {
		if (g_viewing_range_nodes < VIEWING_RANGE_NODES_MAX) {
			g_viewing_range_nodes += MAP_BLOCKSIZE / 2;
			changed = true;
		}
	}
	if (changed) {
		std::cout << "g_viewing_range_nodes = " << g_viewing_range_nodes
				<< std::endl;
	}
	g_viewing_range_nodes_mutex.Lock();
#endif
}
int main() {
	/*
	 Initialization
	 */

	srand(time(0));
	g_viewing_range_nodes_mutex.Init();
	assert(g_viewing_range_nodes_mutex.IsInitialized());
	MyEventReceiver receiver;

	// create device and exit if creation failed
	u16 screenW = 800;
	u16 screenH = 600;
	video::E_DRIVER_TYPE driverType;

#ifdef _WIN32
	//driverType = video::EDT_DIRECT3D9; // Doesn't seem to work
	driverType = video::EDT_OPENGL;
#else
	driverType = video::EDT_OPENGL;
#endif

	IrrlichtDevice *device;
	device = createDevice(driverType, core::dimension2d<u32>(screenW, screenH),
			16, false, false, false, &receiver);

	if (device == 0)
		return 1; // could not create selected driver.
	video::IVideoDriver* driver = device->getVideoDriver();
	scene::ISceneManager* smgr = device->getSceneManager();
	gui::IGUIEnvironment* guienv = device->getGUIEnvironment();

	// -----pause menu-----
	video::ITexture* image = driver->getTexture("../data/pause.png");
	u16 imgWidth = 600;
	u16 imgHeight = 600;
	gui::IGUIImage* pauseOverlay = guienv->addImage(image,
			core::position2d<int>(screenW / 2 - imgWidth / 2,
					screenH / 2 - imgHeight / 2));
	pauseOverlay->setVisible(false);

	gui::IGUISkin* skin = guienv->getSkin();
	gui::IGUIFont* font = guienv->getFont("../data/fontlucida.png");
	if (font)
		skin->setFont(font);
	skin->setColor(gui::EGDC_BUTTON_TEXT, video::SColor(255, 255, 255, 255));
	skin->setColor(gui::EGDC_3D_HIGH_LIGHT, video::SColor(255, 0, 0, 0));
	skin->setColor(gui::EGDC_3D_SHADOW, video::SColor(255, 0, 0, 0));
	const wchar_t *text = L"Loading...";
	core::vector2d<s32> center(screenW / 2, screenH / 2);
	core::dimension2d<u32> textd = font->getDimension(text);
	std::cout << "Text w=" << textd.Width << " h=" << textd.Height << std::endl;
	core::vector2d<s32> textsize(300, textd.Height);
	core::rect<s32> textrect(center - textsize / 2, center + textsize / 2);

	gui::IGUIStaticText *gui_loadingtext = guienv->addStaticText(text, textrect,
			false, false);
	gui_loadingtext->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_UPPERLEFT);

	driver->beginScene(true, true, video::SColor(255, 255, 255, 255));
	guienv->drawAll();
	driver->endScene();

	video::SMaterial materials[MATERIALS_COUNT];
	for (u16 i = 0; i < MATERIALS_COUNT; i++) {
		materials[i].Lighting = false;
		materials[i].BackfaceCulling = false;

		const char *filename = g_material_filenames[i];
		if (filename != NULL) {
			video::ITexture *t = driver->getTexture(filename);
			if (t == NULL) {
				std::cout << "Texture could not be loaded: \"" << filename
						<< "\"" << std::endl;
				return 1;
			}
			materials[i].setTexture(0, driver->getTexture(filename));
		}
		materials[i].setFlag(video::EMF_BILINEAR_FILTER, false);
	}

	// -----TODO: environment will load the whole map-----
	Map *map = new Map(materials, smgr->getRootSceneNode(), smgr, 666);
	Environment *env = new Environment(map, dout);

	// -----TODO: Just local player and a reference player-----
	player = new Player(smgr->getRootSceneNode(), smgr, 111);

//        Player dummyPlayer= new Player(null,smgr,0);
//        dummyPlayer.setPosition(v3f(20,200,0));

	env->addPlayer(player);
	scene::ICameraSceneNode* camera = smgr->addCameraSceneNode(0, // Camera parent
			v3f(BS * 100, BS * 2, BS * 100), // Look from
			v3f(BS * 100 + 1, BS * 2, BS * 100), // Look to
			-1 // Camera ID
			);
	if (camera == NULL)
		return 1;

	camera->setFOV(FOV_ANGLE);
	// Just so big a value that everything rendered is visible
	camera->setFarValue(BS * 1000);
#define ZOOM_MAX 1.5
#define ZOOM_MIN (-5.0*BS)
#define ZOOM_SPEED (0.02*BS)
#define ROTATE_SPEED 1
	f32 camera_yaw = 0; // "right/left"
	f32 camera_pitch = 0; // "up/down"
	// camera zoom distance control
	f32 zoom_max = ZOOM_MAX;
	f32 zoom_min = ZOOM_MIN;
	f32 zoom_speed = ZOOM_SPEED;
	f32 camera_zoom = 0;
	// camera rotate control
	f64 rotate_speed = ROTATE_SPEED;
	f64 camera_rotate = 0;

	// Random constants
#define WALK_ACCELERATION (4.0 * BS)
#define WALKSPEED_MAX (4.0 * BS)

//#define WALKSPEED_MAX (20.0 * BS)
	f32 walk_acceleration = WALK_ACCELERATION;
	f32 walkspeed_max = WALKSPEED_MAX;

	/*
	 The mouse cursor needs not be visible, so we hide it via the irr::IrrlichtDevice::ICursorControl.
	 */
	device->getCursorControl()->setVisible(false);

	gui_loadingtext->remove();

	gui::IGUIStaticText *guitext = guienv->addStaticText(L"Minetest-c55",
			core::rect<s32>(5, 5, 5 + 300, 5 + textsize.Y), false, false);
	/*
	 Main loop
	 */

	bool first_loop_after_window_activation = true;

	s32 lastFPS = -1;

	// Time is in milliseconds
	u32 lasttime = device->getTimer()->getTime();
	while (device->run()) {
		// game pause check
		if (receiver.isPaused) {
			if (!pauseOverlay->isVisible()) {
				device->getTimer()->stop();
				pauseOverlay->setVisible(true);
				device->getCursorControl()->setVisible(true);
			}
			if (device->isWindowActive()) {
				driver->beginScene(true, true, video::SColor(0, 200, 200, 200));

				guienv->drawAll();

				driver->endScene();
			}
		} else {
			if (pauseOverlay->isVisible()) {
				pauseOverlay->setVisible(false);
				device->getCursorControl()->setVisible(false);
				device->getTimer()->start();
			}

			/*
			 Time difference calculation
			 */
			u32 time = device->getTimer()->getTime();
			f32 dtime; // step interval, in seconds
			if (time > lasttime)
				dtime = (time - lasttime) / 1000.0;
			else
				dtime = 0;
			lasttime = time;
			updateViewingRange(dtime);
			// Collected during the loop and displayed
			core::list<core::aabbox3d<f32> > hilightboxes;

			/*
			 Special keys
			 */

			v3f zoom_direction = v3f(0, 0, 1);
			zoom_direction.rotateXZBy(camera_yaw);
			/*Camera zoom*/
			if (receiver.IsKeyDown(irr::KEY_UP)) {
				camera_zoom += zoom_speed;
			}

			if (receiver.IsKeyDown(irr::KEY_DOWN)) {
				camera_zoom -= zoom_speed;
			}
			if (receiver.wheel != 0) {
				camera_zoom += receiver.wheel;
				receiver.wheel = 0;
			}
			if (camera_zoom < zoom_min) {
				camera_zoom = zoom_min;
			} else if (camera_zoom > zoom_max) {
				camera_zoom = zoom_max;
			}

			/*Camera rotate*/
			if (receiver.IsKeyDown(irr::KEY_LEFT)) {
				camera_rotate -= rotate_speed;
			}

			if (receiver.IsKeyDown(irr::KEY_RIGHT)) {
				camera_rotate += rotate_speed;
			}

			/*
			 Player speed control
			 */
			// get movement direction
			// default direction, facing ahead
			v3f move_direction = v3f(0, 0, 1);
			move_direction.rotateXZBy(camera_yaw);
			v3f speed = v3f(0, 0, 0);
			// determine movement direction
			if (receiver.IsKeyDown(irr::KEY_KEY_W)) {
				speed += move_direction;
			}
			if (receiver.IsKeyDown(irr::KEY_KEY_S)) {
				speed -= move_direction;
			}
			if (receiver.IsKeyDown(irr::KEY_KEY_A)) {
				// counter-clockwise rotation against the plane formed by move_direction and Y direction
				speed += move_direction.crossProduct(v3f(0, 1, 0));
			}
			if (receiver.IsKeyDown(irr::KEY_KEY_D)) {
				// clockwise rotation against the plane: move_direction + Y direction
				speed += move_direction.crossProduct(v3f(0, -1, 0));
			}
			if (receiver.IsKeyDown(irr::KEY_SPACE)) {
				if (player->touching_ground) {
					player->speed.Y = 6.5 * BS;
				}
			}
			// Calculate the maximal movement speed of the player (Y is ignored): direction * max_speed
			speed = speed.normalize() * walkspeed_max;
			// speed value change per loop
			f32 inc = walk_acceleration * BS * dtime;
			// new player speed calculation, limited by the max_speed
			// x-wise
			if (player->speed.X < speed.X - inc)
				player->speed.X += inc; // positive new direction, new speed not exceeding the max_speed
			else if (player->speed.X > speed.X + inc)
				player->speed.X -= inc; // negative new direction, new speed not exceeding the max_speed
			else if (player->speed.X < speed.X)
				player->speed.X = speed.X; // positive new direction, new speed exceeding the max_speed
			else if (player->speed.X > speed.X)
				player->speed.X = speed.X; // negative new direction, new speed exceeding the max_speed
			// z-wise
			if (player->speed.Z < speed.Z - inc)
				player->speed.Z += inc;
			else if (player->speed.Z > speed.Z + inc)
				player->speed.Z -= inc;
			else if (player->speed.Z < speed.Z)
				player->speed.Z = speed.Z;
			else if (player->speed.Z > speed.Z)
				player->speed.Z = speed.Z;

			/*
			 Process environment: simulation logical step increment
			 */
			// -----TODO: Just environment step time. Create environment somewhere-----
			env->step(dtime);
			/*
			 Mouse and camera control
			 */
			if (device->isWindowActive()) {
				// bi-while loop change
				if (first_loop_after_window_activation) {
					first_loop_after_window_activation = false;
				} else {
					// calculate the range of pitch and yaw
					s32 dx = device->getCursorControl()->getPosition().X
							- screenW / 2;
					s32 dy = device->getCursorControl()->getPosition().Y
							- screenH / 2;
					// convert to angle value
					camera_yaw -= dx * 0.2;
					camera_pitch += dy * 0.2;
					if (camera_pitch < -89.9)
						camera_pitch = -89.9;
					if (camera_pitch > 89.9)
						camera_pitch = 89.9;
				}
				// reset cursor (i.e., crosshair) to the center of the screen
				device->getCursorControl()->setPosition(screenW / 2,
						screenH / 2);
			} else {
				first_loop_after_window_activation = true;
			}
			// default direction
			v3f camera_direction = v3f(0, 0, 1);
			// change the horizontal direction
			camera_direction.rotateYZBy(camera_pitch);
			// change the vertical direction
			camera_direction.rotateXZBy(camera_yaw);

			v3f p = player->getPosition();
			// adjust camera if pressing left/right arrow
			zoom_direction.rotateXZBy(camera_rotate);
			camera_direction.rotateXZBy(camera_rotate);
			player->setRotation(v3f(0, -1 * camera_yaw, 0));
			// BS*1.7 is the value of PLAYER_HEIGHT in player.cpp
			v3f camera_position = p + v3f(0, BS * 1.65, zoom_max)
					+ zoom_direction * camera_zoom;

			// update camera position and look-at target
			camera->setPosition(camera_position);
			// look-at target: unit vector (representing the direction) from the current player position
			camera->setTarget(camera_position + camera_direction);

			// -----for Map-----
			env->getMap().updateCamera(camera_position, camera_direction);
			/*
			 Calculate which block the crosshair is pointing to:
			 by drawing a line between the player and the point d*BS units
			 distant from the player along with the camera direction
			 */
			//u32 t1 = device->getTimer()->getTime();
			f32 d = 4; // max. distance: 5 BS
			core::line3d<f32> shootline(camera_position,
					camera_position + camera_direction * BS * (d + 1));
			bool nodefound = false;
			// position of the surface node intersecting the shootline
			v3s16 nodepos;
			v3s16 neighbourpos;
			// the bounding box of the found block face
			core::aabbox3d<f32> nodefacebox;
			f32 mindistance = BS * 1001;
			v3s16 pos_i = Map::floatToInt(player->getPosition());

			s16 a = d;

			s16 ystart = pos_i.Y + 0 - (camera_direction.Y < 0 ? a : 1);
			s16 zstart = pos_i.Z - (camera_direction.Z < 0 ? a : 1);
			s16 xstart = pos_i.X - (camera_direction.X < 0 ? a : 1);
			s16 yend = pos_i.Y + 1 + (camera_direction.Y > 0 ? a : 1);
			s16 zend = pos_i.Z + (camera_direction.Z > 0 ? a : 1);
			s16 xend = pos_i.X + (camera_direction.X > 0 ? a : 1);

			for (s16 y = ystart; y <= yend; y++) {
				for (s16 z = zstart; z <= zend; z++) {
					for (s16 x = xstart; x <= xend; x++) {
						try {
							if (env->getMap().getNode(v3s16(x, y, z)).d
									== MATERIAL_AIR) {
								continue;
							}
						} catch (InvalidPositionException &e) {
							continue;
						}

						// node position
						v3s16 np(x, y, z);
						v3f npf = Map::intToFloat(np);

						f32 d = 0.01;

						// facets
						v3s16 directions[6] = { v3s16(0, 0, 1), // back
						v3s16(0, 1, 0), // top
						v3s16(1, 0, 0), // right
						v3s16(0, 0, -1), v3s16(0, -1, 0), v3s16(-1, 0, 0), };

						for (u16 i = 0; i < 6; i++) {
							//{u16 i=3;
							v3f dir_f = v3f(directions[i].X, directions[i].Y,
									directions[i].Z);
							v3f centerpoint = npf + dir_f * BS / 2; // center point of the face
							f32 distance =
									(centerpoint - camera_position).getLength(); // distance to the camera

							// find the closest block
							if (distance < mindistance) {
								// TODO: ???
								core::CMatrix4<f32> m;
								m.buildRotateFromTo(v3f(0, 0, 1), dir_f);
								// This is the back face
								v3f corners[2] = { v3f(BS / 2, BS / 2,
								BS / 2), v3f(-BS / 2, -BS / 2,
								BS / 2 + d) };
								for (u16 j = 0; j < 2; j++) {
									m.rotateVect(corners[j]);
									corners[j] += npf;
									//std::cout<<"box corners["<<j<<"]: ("<<corners[j].X<<","<<corners[j].Y<<","<<corners[j].Z<<")"<<std::endl;
								}
								//core::aabbox3d<f32> facebox(corners[0],corners[1]);
								core::aabbox3d<f32> facebox(corners[0]);
								facebox.addInternalPoint(corners[1]);
								// find the face of the block
								if (facebox.intersectsWithLine(shootline)) {
									nodefound = true;
									nodepos = np;
									neighbourpos = np + directions[i];
									// find the closest block
									mindistance = distance;
									nodefacebox = facebox;
								}
							}
						} // for (u16 i = 0; i < 6; i++)
					} // for (s16 x = xstart; x <= xend; x++)
				} // for (s16 z = zstart; z <= zend; z++)

			} // for (s16 y = ystart; y <= yend; y++)
			  // highlight the selected surface and add/remove block
			if (nodefound) {
				static v3s16 nodepos_old(-1, -1, -1);
				if (nodepos != nodepos_old) {
					std::cout << "Pointing at (" << nodepos.X << ","
							<< nodepos.Y << "," << nodepos.Z << ")"
							<< std::endl;
					nodepos_old = nodepos;
				}
			}
			/*
			 Update GUI stuff: display the selected material
			 */
			static u8 old_selected_material = MATERIAL_AIR;
			if (g_selected_material != old_selected_material) {
				old_selected_material = g_selected_material;
				wchar_t temptext[50];
				swprintf(temptext, 50, L"Minetest-c55 (F: material=%i)",
						g_selected_material);
				guitext->setText(temptext);
			}

			/*
			 Drawing begins
			 */
			video::SColor bgcolor = video::SColor(255, 90, 140, 200);

			driver->beginScene(true, true, bgcolor);

			smgr->drawAll();

			// draw the crosshair
			core::vector2d<s32> displaycenter(screenW / 2, screenH / 2);
			driver->draw2DLine(displaycenter - core::vector2d<s32>(10, 0),
					displaycenter + core::vector2d<s32>(10, 0),
					video::SColor(255, 255, 255, 255));
			driver->draw2DLine(displaycenter - core::vector2d<s32>(0, 10),
					displaycenter + core::vector2d<s32>(0, 10),
					video::SColor(255, 255, 255, 255));

			/*
			 * draw a box for the highlighted face
			 * currently, there is only one box, since the hilightboxes is always re-initialized in each loop
			 */
			video::SMaterial m;
			m.Thickness = 10;
			m.Lighting = false;
			driver->setMaterial(m);
			for (core::list<core::aabbox3d<f32> >::Iterator i =
					hilightboxes.begin(); i != hilightboxes.end(); i++) {
				driver->draw3DBox(*i, video::SColor(255, 0, 0, 0));
			}

			guienv->drawAll();

			driver->endScene();

			/*
			 Drawing ends
			 */

			// display the current FPS on GUI
			u16 fps = driver->getFPS();
			if (lastFPS != fps) {
				core::stringw str = L"Minetest [";
				str += driver->getName();
				str += "] FPS:";
				str += fps;
				device->setWindowCaption(str.c_str());
				lastFPS = fps;
			}

		} // if (receiver.isPaused) {} else
	} // while (device->run())
	/*
	 In the end, delete the Irrlicht device.
	 */
	device->drop();

	return 0;
} // main()

//END
