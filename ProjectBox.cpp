#include "SDLWrapper.h"
#include <cmath>
#include <vector>

void GameMain();

int SDL_main(int argc, char* argv[])
{
	srand(time(NULL));

	InitSDLWrapper();
	GameMain();
	ShutdownSDLWrapper();
	return 0;
}

class Sprite
{
public:
	std::vector<Texture> vec;
	int loop;
	int keepTicks;
	double x, y;
	int size;

	int _idx;
	int _leftTicks;

	Sprite() : loop(-1), keepTicks(60), x(0), y(0), size(32), _idx(-1), _leftTicks(0) {}

	Texture tickTexture()
	{
		if (--_leftTicks <= 0)
		{
			_leftTicks = keepTicks;
			_idx++;
		}

		if (_idx < vec.size())
		{
			return vec[_idx];
		}
		else if (loop < 0)
		{
			_idx = 0;
			return vec[_idx];
		}
		else if (loop)
		{
			_idx = 0;
			--loop;
			return vec[_idx];
		}
		else
		{
			return Texture();
		}
	}
};

class Plane
{
public:
	double x, y;
	int rad;
	double speed;

	int hp;
	int leftAmmo;

	Plane() : x(0), y(0), rad(0), speed(0), hp(100), leftAmmo(50) {}
};

class EnemyPlane
{
public:
	double x, y;
	int rad;
	double speed;
	int hp;

	int ammo;
	int waitTicksFire;
	int waitTicksReload;
	int waitTicksSmoke;

	EnemyPlane() : x(0), y(0), rad(0), speed(0), hp(100), ammo(3), waitTicksFire(0), waitTicksReload(0), waitTicksSmoke(0) {}
};

class Ammo
{
public:
	double x, y;
	double rad;
	double speed;
	int ticksLeft;
	int party;

	Ammo() : x(0), y(0), rad(0), speed(0), ticksLeft(0), party(0) {}
};

class NumberPlate
{
public:
	std::vector<Texture> vec;

	std::vector<Texture> getNumber(int n)
	{
		std::vector<Texture> nvec;

		while (n > 0)
		{
			int lastN = n % 10;
			nvec.push_back(vec[lastN]);

			n = n / 10;
		}

		if (nvec.empty())
		{
			nvec.push_back(vec[0]);
		}
		else
		{
			std::reverse(nvec.begin(), nvec.end());
		}

		return nvec;
	}
};

enum
{
	TILE_NONE = 0,
	TILE_INTERNAL_PATH = 1,
	TILE_BUILDING = 11,
	TILE_TREE = 12,
	TILE_PATH_U = 21,
	TILE_PATH_R,
	TILE_PATH_D,
	TILE_PATH_L,
	TILE_PATH_UR,
	TILE_PATH_RD,
	TILE_PATH_DL,
	TILE_PATH_LU,
	TILE_PATH_UD,
	TILE_PATH_RL,
	TILE_PATH_URD,
	TILE_PATH_RDL,
	TILE_PATH_DLU,
	TILE_PATH_LUR,
	TILE_PATH_URDL,
};

constexpr int MAP_SIZE_MAXX = 1000;
constexpr int MAP_SIZE_MAXY = 1000;

std::vector<EnemyPlane> generate_enemies(int n = 20)
{
	std::vector<EnemyPlane> vec;
	for (int i = 0; i < n; i++)
	{
		EnemyPlane e;
		e.x = rand() % (MAP_SIZE_MAXX * 10);
		e.y = rand() % (MAP_SIZE_MAXY * 10);
		e.speed = 80;

		vec.emplace_back(e);
	}

	return vec;
}

std::vector<std::vector<int>> generate_random_map()
{
	std::vector<std::vector<int>> m(MAP_SIZE_MAXY);
	for (int i = 0; i < MAP_SIZE_MAXY; i++)
	{
		m[i].resize(MAP_SIZE_MAXX);
	}

	std::vector<std::pair<int, int>> vec;
	for (int i = 0; i < 10000; i++)
	{
		int x = rand() % MAP_SIZE_MAXX;
		int y = rand() % MAP_SIZE_MAXY;

		m[y][x] = TILE_BUILDING;
		vec.emplace_back(x, y);
	}

	for (int i = 0; i < MAP_SIZE_MAXY; i++)
	{
		for (int j = 0; j < MAP_SIZE_MAXX; j++)
		{
			if (m[i][j] == TILE_BUILDING)
			{
				for (int y = i - 5; y < i + 5; y++)
				{
					if (y < 0 || y >= MAP_SIZE_MAXY) continue;

					for (int x = j - 5; x < j + 5; x++)
					{
						if (x < 0 || x >= MAP_SIZE_MAXX) continue;
						if (y == i && x == j) continue;

						m[y][x] == TILE_TREE;
					}
				}
			}
		}
	}

	return m;
}

bool isEqual(double a, double b)
{
	return (a > b ? fabs(a - b) : fabs(b - a)) < 1e-6;
}

void GameMain()
{
	Window wnd = Window("Fighters", 1024, 768);
	Renderer rnd = wnd.createRenderer();

	Font font = Font("resources/SourceHanSansCN-Normal.otf", 18);
	Texture tPlane = rnd.load("resources/GER_bf110.png");
	Texture tEnemyPlane = rnd.load("resources/GER_bf109.png");
	Texture tHouse = rnd.load("resources/tile_0072.png");
	Texture tGrass = rnd.load("resources/tile_0110.png");
	Texture tTree = rnd.load("resources/tile_0060.png");
	Texture tAmmoFlag = rnd.load("resources/tile_0013.png");
	Texture tHpFlag = rnd.load("resources/tile_0024.png");
	Texture tAmmo = rnd.load("resources/tile_0000.png");
	Texture tEnemyAmmo = rnd.load("resources/tile_0000.png");
	tEnemyAmmo.setColorMod(Color(64, 64, 255, 0));

	Texture tRoad = rnd.load("resources/tile_0056.png");

	Sound sfxPlaneFlying = Sound("resources/propeller-plane-flying-steady-01.wav");
	sfxPlaneFlying.setVolume(32);
	Sound sfxPlaneIdle = Sound("resources/propeller-plane-idle-01.wav");
	sfxPlaneIdle.setVolume(32);
	Sound sfxPlaneTakeOff = Sound("resources/propeller-plane-take-off-01.wav");
	Sound sfxExplosion = Sound("resources/explosion.wav");
	Sound sfxFire = Sound("resources/fire.wav");
	sfxFire.setVolume(32);

	std::vector<Sound> vecSfxHit;
	vecSfxHit.push_back(Sound("resources/hit1.wav"));
	vecSfxHit.push_back(Sound("resources/hit2.wav"));
	vecSfxHit.push_back(Sound("resources/hit3.wav"));
	for (auto& sfx : vecSfxHit)
	{
		sfx.setVolume(64);
	}

	Sprite spSmoke;
	spSmoke.loop = 0;
	spSmoke.keepTicks = 15;
	spSmoke.vec.push_back(rnd.load("resources/FX001_01.png"));
	spSmoke.vec.push_back(rnd.load("resources/FX001_02.png"));
	spSmoke.vec.push_back(rnd.load("resources/FX001_03.png"));
	spSmoke.vec.push_back(rnd.load("resources/FX001_04.png"));
	spSmoke.vec.push_back(rnd.load("resources/FX001_05.png"));

	Sprite spEnemySmoke;
	spEnemySmoke.loop = 0;
	spEnemySmoke.keepTicks = 15;
	spEnemySmoke.vec.push_back(rnd.load("resources/FX001_01.png"));
	spEnemySmoke.vec.push_back(rnd.load("resources/FX001_02.png"));
	spEnemySmoke.vec.push_back(rnd.load("resources/FX001_03.png"));
	spEnemySmoke.vec.push_back(rnd.load("resources/FX001_04.png"));
	spEnemySmoke.vec.push_back(rnd.load("resources/FX001_05.png"));
	for (auto& t : spEnemySmoke.vec)
	{
		t.setColorMod(Color(128, 128, 128, 0));
	}

	Sprite spExplode;
	spExplode.loop = 0;
	spExplode.keepTicks = 3;
	spExplode.size = 16;
	spExplode.vec.push_back(rnd.load("resources/tile_0003.png"));
	spExplode.vec.push_back(rnd.load("resources/tile_0004.png"));
	spExplode.vec.push_back(rnd.load("resources/tile_0005.png"));
	spExplode.vec.push_back(rnd.load("resources/tile_0006.png"));
	spExplode.vec.push_back(rnd.load("resources/tile_0007.png"));
	spExplode.vec.push_back(rnd.load("resources/tile_0008.png"));

	NumberPlate numPlate;
	numPlate.vec.push_back(rnd.load("resources/tile_0019.png"));
	numPlate.vec.push_back(rnd.load("resources/tile_0020.png"));
	numPlate.vec.push_back(rnd.load("resources/tile_0021.png"));
	numPlate.vec.push_back(rnd.load("resources/tile_0022.png"));
	numPlate.vec.push_back(rnd.load("resources/tile_0023.png"));
	numPlate.vec.push_back(rnd.load("resources/tile_0031.png"));
	numPlate.vec.push_back(rnd.load("resources/tile_0032.png"));
	numPlate.vec.push_back(rnd.load("resources/tile_0033.png"));
	numPlate.vec.push_back(rnd.load("resources/tile_0034.png"));
	numPlate.vec.push_back(rnd.load("resources/tile_0035.png"));

	const int fpsLimit = 60;
	const int frameTimeLimit = 1000 / fpsLimit;
	const double planeSpeedLimit = 150;
	const int chanSfxIdle = 2;
	const int chanSfxFlying = 3;
	const int chanSfxTakeOff = 4;
	const int chanSfxExplosion = 5;
	const int chanSfxFire = 6;
	const int chanSfxHitBegin = 8;
	const int chanSfxHitEnd = 14;

	auto nextTick = SDL_GetTicks() + frameTimeLimit;
	bool running = true;

	Plane player;
	player.x = MAP_SIZE_MAXX * 10 / 2;
	player.y = MAP_SIZE_MAXY * 10 / 2;

	std::vector<Sprite> sprites;
	std::vector<Ammo> ammos;
	auto gameMap = generate_random_map();
	auto enemies = generate_enemies();

	double offsetx = 0;
	double offsety = 0;
	int waitTicksNextSmoke = 0;
	int waitTicksNextFire = 0;
	int waitTicksNextReload = 0;
	int planeSfxState = 0;  // 0 None 2 Idle 4 TakeOff 3 Flying
	int hitSfxNextChan = chanSfxHitBegin;

	wnd.show();

	while (running)
	{
		Event e;

		int deltaRad = 0;
		double deltaAcc = -0.5;
		int nextPlaneSfxState = 0;
		bool isFired = false;

		while (running && SDL_GetTicks() < nextTick)
		{
			if (!PollEvent(e)) continue;

			switch (e.type)
			{
			case SDL_QUIT:
				running = false;
				break;
				break;
			}
		}

		// holding key detection
		auto keyboardState = SDL_GetKeyboardState(NULL);
		if (keyboardState[SDL_SCANCODE_W] || keyboardState[SDL_SCANCODE_UP]) deltaAcc = 25;
		if (keyboardState[SDL_SCANCODE_S] || keyboardState[SDL_SCANCODE_DOWN]) deltaAcc = -20;
		if (keyboardState[SDL_SCANCODE_A] || keyboardState[SDL_SCANCODE_LEFT]) deltaRad = -1;
		if (keyboardState[SDL_SCANCODE_D] || keyboardState[SDL_SCANCODE_RIGHT]) deltaRad = 1;
		if (keyboardState[SDL_SCANCODE_SPACE]) isFired = true;

		// game update per tick, 60 ticks per second
		player.rad += deltaRad;
		if (player.rad < 0) player.rad += 360;
		else if (player.rad >= 360) player.rad -= 360;

		double tickAcc = deltaAcc;
		if (tickAcc > 0)
		{
			tickAcc = tickAcc * (1 - player.speed / planeSpeedLimit);

			// Add a new smoke sprite
			if (!waitTicksNextSmoke)
			{
				waitTicksNextSmoke = 10;
				Sprite smoke = spSmoke;
				smoke.x = player.x;
				smoke.y = player.y;
				sprites.push_back(smoke);
			}
		}

		if (player.speed < 10)
		{
			nextPlaneSfxState = 2;
		}
		else
		{
			nextPlaneSfxState = 3;
		}

		if (nextPlaneSfxState != planeSfxState)
		{
			switch (planeSfxState)
			{
			case 2:
				MusicPlayer::stop(chanSfxIdle);
				break;
			case 3:
				MusicPlayer::fadeOut(chanSfxFlying, 100);
				break;
			case 4:
				MusicPlayer::stop(chanSfxTakeOff);
				break;
			}

			switch (nextPlaneSfxState)
			{
			case 2:
				MusicPlayer::play(sfxPlaneIdle, chanSfxIdle, -1);
				break;
			case 3:
				MusicPlayer::play(sfxPlaneFlying, chanSfxFlying, -1);
				break;
			case 4:
				MusicPlayer::play(sfxPlaneTakeOff, chanSfxTakeOff);
				break;
			}

			planeSfxState = nextPlaneSfxState;
		}

		player.speed += tickAcc / fpsLimit;
		player.speed = std::max(std::min(player.speed, planeSpeedLimit), 0.0);

		player.x += player.speed * sin(player.rad * M_PI / 180) / fpsLimit;
		player.y -= player.speed * cos(player.rad * M_PI / 180) / fpsLimit;

		for (auto& m : ammos)
		{
			m.x += m.speed * sin(m.rad * M_PI / 180) / fpsLimit;
			m.y -= m.speed * cos(m.rad * M_PI / 180) / fpsLimit;
		}
		if (isFired && player.leftAmmo && !waitTicksNextFire)
		{
			Ammo m;
			m.x = player.x;
			m.y = player.y;
			m.rad = player.rad;
			m.speed = 300 + player.speed;
			m.ticksLeft = 600;

			ammos.push_back(m);
			waitTicksNextFire = 6;

			if (!--player.leftAmmo)
			{
				waitTicksNextReload = 300;
			}

			if (!MusicPlayer::isPlaying(chanSfxFire))
			{
				MusicPlayer::play(sfxFire, chanSfxFire, 0);
			}
		}

		if (!player.leftAmmo && !waitTicksNextReload)
		{
			player.leftAmmo = 50;
		}

		offsetx = player.x - 512;
		offsety = player.y - 384;
		waitTicksNextSmoke = std::max(waitTicksNextSmoke - 1, 0);
		waitTicksNextFire = std::max(waitTicksNextFire - 1, 0);
		waitTicksNextReload = std::max(waitTicksNextReload - 1, 0);

		// enemy tick
		for (auto iter = enemies.begin(); iter != enemies.end(); )
		{
			auto& ene = *iter;
			if (ene.hp <= 0)
			{
				MusicPlayer::play(sfxExplosion, chanSfxExplosion, 0);

				iter = enemies.erase(iter);
				continue;
			}

			if (ene.hp < 50 && !ene.waitTicksSmoke)
			{
				Sprite s = spEnemySmoke;
				s.x = ene.x;
				s.y = ene.y;

				sprites.push_back(s);

				if (ene.hp <= 30)
				{
					ene.speed = 60;
					ene.waitTicksSmoke = 10;
				}
				else
				{
					ene.speed = 70;
					ene.waitTicksSmoke = 20;
				}
			}

			double distance = sqrt((ene.x - player.x) * (ene.x - player.x) + (ene.y - player.y) * (ene.y - player.y));
			int targetRad = 90 + atan2(player.y - ene.y, player.x - ene.x) * 180 / M_PI;
			if (distance >= ene.speed * 8)
			{
				ene.rad = targetRad;
			}

			if (distance < ene.speed * 16 && abs(targetRad - ene.rad) < 30 && ene.ammo && !ene.waitTicksFire)
			{
				Ammo m;
				m.x = ene.x;
				m.y = ene.y;
				m.rad = ene.rad;
				m.speed = 300;
				m.ticksLeft = 600;
				m.party = 1;

				ammos.push_back(m);

				ene.waitTicksFire = 5 + rand() % 5;

				if (!--ene.ammo) ene.waitTicksReload = 180;
			}

			if (ene.waitTicksFire) --ene.waitTicksFire;
			if (ene.waitTicksSmoke) --ene.waitTicksSmoke;
			if (ene.waitTicksReload)
			{
				if (!--ene.waitTicksReload) ene.ammo = 3;
			}

			ene.x += ene.speed * sin(ene.rad * M_PI / 180) / fpsLimit;
			ene.y -= ene.speed * cos(ene.rad * M_PI / 180) / fpsLimit;

			++iter;
		}

		// ammo tick & collision detection
		for (auto iter = ammos.begin(); iter != ammos.end();)
		{
			bool hit = false;
			double hitDistance = 0;
			bool gotHit = false;

			if (iter->party)
			{
				double distance = sqrt((iter->x - player.x) * (iter->x - player.x) + (iter->y - player.y) * (iter->y - player.y));
				if (distance < 8)
				{
					hit = true;
					gotHit = true;
					player.hp -= 5;
				}
			}
			else
			{
				for (auto& ene : enemies)
				{
					double distance = sqrt((iter->x - ene.x) * (iter->x - ene.x) + (iter->y - ene.y) * (iter->y - ene.y));
					if (distance < 8)
					{
						hit = true;
						hitDistance = sqrt((ene.x - player.x) * (ene.x - player.x) + (ene.y - player.y) * (ene.y - player.y));
						if (hitDistance < 150) hitDistance = 0;
						else hitDistance = std::max(192, int(hitDistance - 150) / 2);
						ene.hp -= 5;
					}
				}
			}

			if (hit)
			{
				if (gotHit)
				{
					MusicPlayer::setPosition(hitSfxNextChan, 0, 0);
				}
				else
				{
					MusicPlayer::setPosition(hitSfxNextChan, 0, hitDistance);
				}
				MusicPlayer::play(vecSfxHit[rand() % vecSfxHit.size()], hitSfxNextChan, 0);
				if (++hitSfxNextChan >= chanSfxHitEnd) hitSfxNextChan = chanSfxHitBegin;
			}

			if (--iter->ticksLeft <= 0 || hit)
			{
				Sprite s = spExplode;
				s.x = iter->x;
				s.y = iter->y;
				sprites.push_back(s);

				iter = ammos.erase(iter);
				continue;
			}

			++iter;
		}

		// ---------- GAME TICK FINISHED, START RENDERING ----------

		rnd.clear();
		// draw building
		int mapOffX = offsetx / 10;
		int mapOffY = offsety / 10;
		for (int i = 0; i < 80; i++)
		{
			for (int j = 0; j < 110; j++)
			{
				if (i + mapOffY >= MAP_SIZE_MAXY || j + mapOffX >= MAP_SIZE_MAXX || i + mapOffY < 0 || j + mapOffX < 0) continue;
				Rect tbox = Rect((j + mapOffX) * 10 - offsetx, (i + mapOffY) * 10 - offsety, 10, 10);

				switch (gameMap[mapOffY + i][mapOffX + j])
				{
				case TILE_NONE:
					rnd.copyTo(tGrass, tbox);
					break;
				case TILE_BUILDING:
					rnd.copyTo(tHouse, tbox);
					break;
				case TILE_TREE:
					rnd.copyTo(tTree, tbox);
					break;
				}
			}
		}

		// draw sprites
		for (auto iter = sprites.begin(); iter != sprites.end();)
		{
			Texture t = iter->tickTexture();
			if (!t)
			{
				iter = sprites.erase(iter);
				continue;
			}

			rnd.copyTo(t, Rect(iter->x - offsetx - 16, iter->y - offsety - 16, iter->size, iter->size));
			++iter;
		}

		// draw ammos
		for (auto iter = ammos.begin(); iter != ammos.end();)
		{
			rnd.copyTo(iter->party ? tEnemyAmmo : tAmmo, Rect(iter->x - offsetx - 5, iter->y - offsety - 5, 10, 10), iter->rad);
			if (--iter->ticksLeft < 0)
			{
				Sprite s = spExplode;
				s.x = iter->x;
				s.y = iter->y;
				sprites.push_back(s);

				iter = ammos.erase(iter);
				continue;
			}

			++iter;
		}

		// draw enemies
		for (const auto& ene : enemies)
		{
			auto [tw, th] = tEnemyPlane.getSize();
			rnd.copyTo(tEnemyPlane, Rect(ene.x - offsetx - tw / 2, ene.y - offsety - th / 2, tw, th), ene.rad);

			char buffer[1024] = { 0 };
			snprintf(buffer, sizeof(buffer), "%d", ene.hp);
			rnd.copyTo(font.blendUTF8(rnd, buffer, Color::White), Point(ene.x - offsetx, ene.y - offsety));
		}

		// draw player
		auto [tw, th] = tPlane.getSize();
		rnd.copyTo(tPlane, Rect((1024 - tw) / 2, (768 - th) / 2, tw, th), player.rad);

		// draw hud
		char buffer[1024] = { 0 };
		snprintf(buffer, sizeof(buffer), "x: %d y: %d speed: %.2lf rad: %d offx: %d offy: %d mapOffX: %d mapOffY: %d tickAcc: %.2lf Ammos: %d Sprites: %d", int(player.x), int(player.y), player.speed, player.rad, int(offsetx), int(offsety), mapOffX, mapOffY, tickAcc, ammos.size(), sprites.size());
		rnd.copyTo(font.blendUTF8(rnd, buffer, Color::White), Point(0, 0));

		// draw number plate
		auto speedPlate = numPlate.getNumber(player.speed);
		int speedPlateX = 20;
		for (auto& tNum : speedPlate)
		{
			auto [tw, th] = tNum.getSize();
			rnd.copyTo(tNum, Rect(speedPlateX, 708, tw, th));
			speedPlateX += tw + 5;
		}
		std::tie(tw, th) = tAmmoFlag.getSize();
		rnd.copyTo(tAmmoFlag, Rect(speedPlateX, 708, tw, th));
		speedPlateX += tw + 5;
		speedPlate = numPlate.getNumber(player.leftAmmo);
		for (auto& tNum : speedPlate)
		{
			auto [tw, th] = tNum.getSize();
			rnd.copyTo(tNum, Rect(speedPlateX, 708, tw, th));
			speedPlateX += tw + 5;
		}
		std::tie(tw, th) = tHpFlag.getSize();
		rnd.copyTo(tHpFlag, Rect(speedPlateX, 708, tw, th));
		speedPlateX += tw + 5;
		speedPlate = numPlate.getNumber(player.hp);
		for (auto& tNum : speedPlate)
		{
			auto [tw, th] = tNum.getSize();
			rnd.copyTo(tNum, Rect(speedPlateX, 708, tw, th));
			speedPlateX += tw + 5;
		}

		rnd.present();

		// ---------- POST Render ----------
		// fps update
		nextTick = SDL_GetTicks() + frameTimeLimit;

		if (enemies.empty())
		{
			printf("Game finished, you win!\n");
			running = false;
		}
		else if (player.hp <= 0)
		{
			printf("Game finished, you lost!\n");
			running = false;
		}
	}
}
