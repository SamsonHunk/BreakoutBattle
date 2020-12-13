//includes
#define _CRT_SECURE_NO_WARNINGS //to be able to use fopen()
#define MAXPLAYERS 2
#define MAXBALLS 2

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <string>
#include "InputManager.h"
#include <thread>
#include <mutex>
#include "PosPacket.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"

///////////////////////////////////////////////
//networking objects
sf::IpAddress otherIp;
unsigned short otherPort;
unsigned short port;
short int playerNum = 0;

PosPacket lastPacket;
PosPacket outPacket;

bool newPacketFlag = false;
bool foundPlayer = false;

std::mutex networkLock;
////////////////////////////////////////////////

///////////////////////////////////////////
//game objects
struct Player
{
	Player()
	{
		playerShape.setSize(sf::Vector2f(60, 10));
	}

	sf::RectangleShape playerShape;
	sf::Vector2f playerPos;
	int playerScore = 0;
	float playerSpeed = 300;
};

struct Ball
{
	Ball()
	{
		ballShape.setSize(sf::Vector2f(10, 10));
		ballShape.setFillColor(sf::Color::Red);
	}

	sf::RectangleShape ballShape;
	sf::Vector2f ballPos;
	sf::Vector2f ballDir = sf::Vector2f(1, 0);
	float ballSpeed = 300;
	int lastHitBy = 0;
	bool isShot = false;
	int ballNumber = 0;

	bool checkCollision(sf::RectangleShape* shape, float dt)
	{
		//check if the ball has collided with the given shape
		if (shape->getGlobalBounds().intersects(ballShape.getGlobalBounds()))
		{
			//there is a collision; figure out which edge the ball bounced on
			//this is done by simulating the ball one frame ahead to determine if the ball needs to bounce on the x or y axis
			sf::FloatRect nextPos = sf::FloatRect((ballPos + sf::Vector2f(ballDir.x, 0) * ballSpeed * dt), ballShape.getSize());
			if (shape->getGlobalBounds().intersects(nextPos))
			{
				//bounce the ball on the y direction
				ballDir = sf::Vector2f(ballDir.x, ballDir.y * -1.f);
			}
			else //else bounce it on the x direction
			{
				ballDir = sf::Vector2f(ballDir.x * -1.f, ballDir.y);
			}

			ballPos = ballPos + (ballDir * ballSpeed * dt);
			return true;
		}
		else
		{
			//no collision return false
			return false;
		}
	};
};

struct Block
{
	Block(int health, sf::Vector2f pos)
	{
		blockHealth = health;
		blockShape.setSize(sf::Vector2f(80.f, 40.f));
		blockShape.setPosition(pos);
		switch (health)
		{
		case 1:
			blockShape.setFillColor(sf::Color::Yellow);
			break;
		case 2:
			blockShape.setFillColor(sf::Color(255,165,0));
			break;
		case 3:
			blockShape.setFillColor(sf::Color::Red);
			break;
		default:
			break;
		}

		if (pos.y < 400.f)
		{
			areaFlag = true;
		}
		else
		{
			areaFlag = false;
		}
	}

	sf::RectangleShape blockShape;
	int blockHealth = 0;
	bool areaFlag = false;
	bool blockIsDead = false;
};

struct GameState
{
	Player players[2];
	Ball balls[2];
	std::vector<Block> levelLayout;
};
/////////////////////////////////////////////////////////////

//function forward declarations
void update(float dt, GameState* gameState, InputManager* inputManager, sf::RenderWindow* window);
void render(float dt, GameState* gameState, InputManager* inputManager, sf::RenderTexture* renderTex, sf::RenderWindow* window);
void checkPackets(sf::UdpSocket* socket, GameState* gameState, sf::RenderWindow* renderWindow);
void sendPackets(sf::UdpSocket* socket, GameState* gameState);

///////////////////////////////////////////////////////////////////////  vector maths
float Length(sf::Vector2f v)
{
	return ((v.x * v.x) + (v.y * v.y));
}

sf::Vector2f Normalise(sf::Vector2f v)
{
	float len = Length(v);
	return sf::Vector2f(v.x / len, v.y / len);
}

float Dot(sf::Vector2f v1, sf::Vector2f v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y);
}
//////////////////////////////////////////////////////////////////////

//load the information from a JSON file into memory

rapidjson::Document LoadJSON(const char* filename)
{
	FILE* file = fopen(filename, "rb");
	
	char readBuffer[1024];

	rapidjson::FileReadStream inputStream(file, readBuffer, sizeof(readBuffer));

	rapidjson::Document doc;
	doc.ParseStream(inputStream);
	fclose(file);


	return doc;
}


int main()
{
	sf::UdpSocket socket;
	socket.setBlocking(true);
	
	{//bind UDP socket
		int answer;
		//initial game setup
	
		std::cout << "1 Host game, 2 Join game" << std::endl;
		std::cin >> answer;

		if (answer == 1)
		{
			port = 5400;
			otherPort = 5410;
			playerNum = 0;
		}
		else
		{
			port = 5410;
			otherPort = 5400;
			playerNum = 1;
		}

		if (socket.bind(port, sf::IpAddress::LocalHost) != socket.Done)
		{
			return 2; //exit game if unable to bind the socket
		}
		else
		{
			std::cout << "Bound socket on port " << std::to_string(port) << std::endl;
		}

		std::string entry;
		switch (answer)
		{
		case 1:
			std::cout << "Waiting for player to join...";
			break;
		case 2:
			do
			{
				std::cout << "Enter host ip: " << std::endl;
				std::cin >> otherIp;
			} while (otherIp == sf::IpAddress::None);
			foundPlayer = true;
			break;
		default:
			return 3;
			break;
		}
	}

	sf::RenderWindow window(sf::VideoMode(800, 800), "Breakout Battle");

	sf::RenderTexture renderTex;
	renderTex.create(800, 800);
	renderTex.setView(sf::View(sf::Vector2f(400, 400), sf::Vector2f(800, 800)));

	InputManager inputManager;

	sf::Clock clock;

	//initialise gamestate objects
	GameState gameState;
	gameState.players[1].playerPos = sf::Vector2f(0, 0);
	gameState.players[0].playerPos = sf::Vector2f(0, 790);
	gameState.balls[0].lastHitBy = 0;
	gameState.balls[1].lastHitBy = 1;

	if (playerNum == 0)
	{
		gameState.balls[0].ballNumber = 0;
		gameState.balls[1].ballNumber = 1;
	}
	else
	{
		gameState.balls[0].ballNumber = 1;
		gameState.balls[1].ballNumber = 0;
	}

	//load level data from JSON file
	rapidjson::Document levelDoc = LoadJSON("level.json");

	//populate gamestate with level geometry
	if (levelDoc.HasMember("level"))
	{
		//center the level to the middle of the screen
		float yOffset = 400.f - ((float)levelDoc["level"].Size() / 2.f) * 40.f;

		for (int y = 0; y < levelDoc["level"].Size(); y++)
		{
			for (int x = 0; x < levelDoc["level"][y]["layer"].Size(); x++)
			{
				if (levelDoc["level"][y]["layer"][x].GetInt() > 0)
				{
					//load any blocks from the json file into memory
					gameState.levelLayout.push_back(Block(levelDoc["level"][y]["layer"][x].GetInt(), sf::Vector2f((float)x * 80.f, yOffset + (float)y * 40.f)));
				}
			}
		}
	}
	else
	{
		return 5;
	}

	//create packet receipt thread
	std::thread receiptThread(checkPackets, &socket, &gameState, &window);

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			switch (event.type)
			{
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::KeyPressed: //update keyboard array
				inputManager.KeyDown(event.key.code);
				break;
			case sf::Event::KeyReleased:
				inputManager.KeyUp(event.key.code);
				break;
			default:
				break;
			}
		}

		//measure the delta time between each frame
		sf::Time frameTime = clock.getElapsedTime();
		clock.restart();

		window.clear();
		networkLock.lock();
		update(frameTime.asSeconds(), &gameState, &inputManager, &window);
		render(frameTime.asSeconds(), &gameState, &inputManager, &renderTex, &window);
		networkLock.unlock();
		sendPackets(&socket, &gameState);
		window.display();
	}

	receiptThread.join();
	return 0;
}

void update(float dt, GameState* gameState, InputManager* inputManager, sf::RenderWindow* window)
{
	//update the player's position
	if (inputManager->GetKey(sf::Keyboard::Left))
	{
		gameState->players[0].playerPos.x -= gameState->players[0].playerSpeed * dt;
		outPacket.playerDir = 1;
	}
	else if (inputManager->GetKey(sf::Keyboard::Right))
	{
		gameState->players[0].playerPos.x += gameState->players[0].playerSpeed * dt;
		outPacket.playerDir = 2;
	}
	else
	{
		outPacket.playerDir = 0;
	}

	if (inputManager->GetKey(sf::Keyboard::Space))
	{//press space to fire a ball
		for (int it = 0; it < MAXBALLS; it++)
		{
			if (gameState->balls[it].lastHitBy == 0 && !gameState->balls[it].isShot)
			{
				gameState->balls[it].isShot = true;
				//angle the ball if the player is moving while shooting the ball
				if (inputManager->GetKey(sf::Keyboard::Left))
				{
					gameState->balls[it].ballDir = sf::Vector2f(-.75f, -1.f);
				}
				else if (inputManager->GetKey(sf::Keyboard::Right))
				{
					gameState->balls[it].ballDir = sf::Vector2f(.75f, -1.f);
				}
				else
				{
					gameState->balls[it].ballDir = sf::Vector2f(0.f, -1.f);
				}
				break;
			}
		}
	}

	//update the ball positions
	for (int it = 0; it < MAXBALLS; ++it)
	{
		//if the ball has already been shot then move it
		if (gameState->balls[it].isShot)
		{
			gameState->balls[it].ballPos += gameState->balls[it].ballDir * (gameState->balls[it].ballSpeed * dt);
		}
		else //else, keep it connected to the player until they release it
		{
			float offset;
			if (gameState->balls[it].lastHitBy == 0)
			{
				offset = 10.f;
			}
			else
			{
				offset = -10.f;
			}
			gameState->balls[it].ballPos = gameState->players[gameState->balls[it].lastHitBy].playerPos - sf::Vector2f(-25.f, offset);
		}

		//ball collision detection, seperate the screen into distinct areas for collision checks
		if (gameState->balls[it].isShot)
		{
			if (gameState->balls[it].ballPos.y < 60.f)
			{
				gameState->balls[it].checkCollision(&gameState->players[1].playerShape, dt);
			}
			else if (gameState->balls[it].ballPos.y > 720.f)
			{
				gameState->balls[it].checkCollision(&gameState->players[0].playerShape, dt);
			}
			else if (gameState->balls[it].ballPos.y < 400.f)
			{
				// level area 1
				for (int levelIt = 0; levelIt < gameState->levelLayout.size(); levelIt++)
				{
					if (gameState->levelLayout[levelIt].areaFlag)
					{
						if (!gameState->levelLayout[levelIt].blockIsDead)
						{
							if (gameState->balls[it].checkCollision(&gameState->levelLayout[levelIt].blockShape, dt))
							{//if a ball collides with a level block, neg a health from it
								switch (--gameState->levelLayout[levelIt].blockHealth)
								{
								case 1:
									gameState->levelLayout[levelIt].blockShape.setFillColor(sf::Color::Yellow);
									break;
								case 2:
									gameState->levelLayout[levelIt].blockShape.setFillColor(sf::Color(255, 165, 0));
									break;
								case 3:
									gameState->levelLayout[levelIt].blockShape.setFillColor(sf::Color::Red);
									break;
								case 0:
									//if a block has 0 health, hide it and remove it's collision detection
									gameState->levelLayout[levelIt].blockIsDead = true;
									break;
								default:
									break;
								}
							}
						}
					}
					else
					{
						break;
					}
				}
			}
			else
			{
				// level area 2
				for (int levelIt = gameState->levelLayout.size() - 1; levelIt > 0; levelIt--)
				{
					if (!gameState->levelLayout[levelIt].areaFlag)
					{
						if (!gameState->levelLayout[levelIt].blockIsDead)
						{
							if (gameState->balls[it].checkCollision(&gameState->levelLayout[levelIt].blockShape, dt))
							{//if a ball collides with a level block, neg a health from it
								switch (--gameState->levelLayout[levelIt].blockHealth)
								{
								case 1:
									gameState->levelLayout[levelIt].blockShape.setFillColor(sf::Color::Yellow);
									break;
								case 2:
									gameState->levelLayout[levelIt].blockShape.setFillColor(sf::Color(255, 165, 0));
									break;
								case 3:
									gameState->levelLayout[levelIt].blockShape.setFillColor(sf::Color::Red);
									break;
								case 0:
									//if a block has 0 health, hide it and remove it's collision detection
									gameState->levelLayout[levelIt].blockIsDead = true;
									break;
								default:
									break;
								}
							}
						}
					}
					else
					{
						break;
					}
				}
			}
		}

		//bounce the ball off the edges of the screen
		if (gameState->balls[it].ballPos.x < 0 || gameState->balls[it].ballPos.x > 790.f)
		{
			gameState->balls[it].ballDir = sf::Vector2f(gameState->balls[it].ballDir.x * -1.f, gameState->balls[it].ballDir.y);
		}
	}

	//update enemy player position
	if (newPacketFlag)
	{
		//if there is a new packet, update the current positions with that info
		gameState->players[1].playerPos = sf::Vector2f(lastPacket.playerPos, 0.f);

		//update the current positions of the ball if they are not in our half of the screen
		for (int it = 0; it < MAXBALLS; it++)
		{
			if (gameState->balls[it].ballPos.y < 100.f || (gameState->balls[it].lastHitBy == playerNum && gameState->balls[it].ballPos.y > 100.f))
			{
				gameState->balls[it].isShot = lastPacket.ballIsShot[gameState->balls[it].ballNumber];

				if (gameState->balls[it].isShot)
				{
					gameState->balls[it].ballPos = lastPacket.ballPos[gameState->balls[it].ballNumber];
					gameState->balls[it].ballDir = lastPacket.ballDir[gameState->balls[it].ballNumber];
				}
			}
		}

		newPacketFlag = false;
	}
	else
	{
		//if there is no new packet, try to determine where the player would go
		switch (lastPacket.playerDir)
		{
		case 1:
			gameState->players[1].playerPos.x -= gameState->players[1].playerSpeed * dt;
			break;
		case 2:
			gameState->players[1].playerPos.x += gameState->players[1].playerSpeed * dt;
			break;
		default:
			break;
		}
	}
}

void render(float dt, GameState* gameState, InputManager* inputManager, sf::RenderTexture* renderTex, sf::RenderWindow* window)
{
	//render all the objects onto the screen
	for (int it = 0; it < MAXPLAYERS; it++)
	{
		gameState->players[it].playerShape.setFillColor(sf::Color::Green);
		gameState->players[it].playerShape.setPosition(gameState->players[it].playerPos);
		window->draw(gameState->players[it].playerShape);
	}

	for (int it = 0; it < gameState->levelLayout.size(); it++)
	{
		if (!gameState->levelLayout[it].blockIsDead)
		{
			window->draw(gameState->levelLayout[it].blockShape);
		}
	}


	//render the balls
	for (int it = 0; it < MAXPLAYERS; it++)
	{
		gameState->balls[it].ballShape.setPosition(gameState->balls[it].ballPos);
		window->draw(gameState->balls[it].ballShape);
	}

}



void checkPackets(sf::UdpSocket* socket, GameState* gameState, sf::RenderWindow* renderWindow)
{
	sf::IpAddress ipIn;
	unsigned short portIn;
	while (renderWindow->isOpen())
	{
		PosPacket packet;
		sf::Socket::Status status = socket->receive(packet.packet, ipIn, portIn);
		//check for new packets
		if (status != socket->Done)
		{
			networkLock.lock();
			std::cout << "Error retrieving packet: ";
			switch (status)
			{
			case sf::Socket::Disconnected:
				std::cout << "Disconnected";
				break;
			case sf::Socket::Error:
				std::cout << "Error";
				break;
			case sf::Socket::NotReady:
				std::cout << "Not Ready";
				break;
			}
			std::cout << std::endl;
			networkLock.unlock();
		}
		else
		{
			networkLock.lock();
			otherIp = ipIn;
			foundPlayer = true;

			//if a whole packet is received then act on the data, make sure the new packet is more up to date
			if (packet.testPacketNum() > lastPacket.packetNum)
			{
				packet.unpack();
				lastPacket = packet;
				newPacketFlag = true;
			}

			networkLock.unlock();
		}
	}
}

void sendPackets(sf::UdpSocket* socket, GameState* gameState)
{
	if (foundPlayer)
	{
		//offload data into packet struct
		outPacket.packetNum++;
		outPacket.playerPos = gameState->players[0].playerPos.x;
		outPacket.ballPos[gameState->balls[0].ballNumber] = sf::Vector2f(gameState->balls[0].ballPos.x, 800 - gameState->balls[0].ballPos.y);
		outPacket.ballPos[gameState->balls[1].ballNumber] = sf::Vector2f(gameState->balls[1].ballPos.x, 800 - gameState->balls[1].ballPos.y);
		outPacket.ballDir[gameState->balls[0].ballNumber] = gameState->balls[0].ballDir * -1.f;
		outPacket.ballDir[gameState->balls[1].ballNumber] = gameState->balls[1].ballDir * -1.f;
		outPacket.ballIsShot[gameState->balls[0].ballNumber] = gameState->balls[0].isShot;
		outPacket.ballIsShot[gameState->balls[1].ballNumber] = gameState->balls[1].isShot;
		outPacket.pack();
		
		std::cout << std::to_string(outPacket.ballPos[0].x) + " " + std::to_string(outPacket.ballPos[0].y) << std::endl;

		//send a packet with the most up to date information
		if (socket->send(outPacket.packet, otherIp, otherPort) != socket->Done)
		{
			std::cout << "Packet failed to send" << std::endl;
		}
	}
}