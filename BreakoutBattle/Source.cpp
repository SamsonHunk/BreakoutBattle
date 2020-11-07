//includes
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <string>
#include "InputManager.h"
#include <thread>
#include <mutex>
#include "PosPacket.h"

#define MAXPLAYERS 2
#define MAXBALLS 2

///////////////////////////////////////////////
//networking objects
sf::IpAddress otherIp;
unsigned short otherPort;
unsigned short port;

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
};

struct Block
{
	sf::RectangleShape blockShape;
	int blockHealth = 0;
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
		}
		else
		{
			port = 5410;
			otherPort = 5400;
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

	//TODO: load level

	//initialise gamestate objects
	GameState gameState;
	gameState.players[1].playerPos = sf::Vector2f(0, 0);
	gameState.players[0].playerPos = sf::Vector2f(0, 790);
	gameState.balls[0].lastHitBy = 0;
	gameState.balls[1].lastHitBy = 1;

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
					gameState->balls[it].ballDir = sf::Vector2f(-.5f, -1.f);
				}
				else if (inputManager->GetKey(sf::Keyboard::Right))
				{
					gameState->balls[it].ballDir = sf::Vector2f(.5f, -1.f);
				}
				else
				{
					gameState->balls[it].ballDir = sf::Vector2f(0.f, -1.f);
				}
				gameState->balls[it].lastHitBy = 0;
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
	}

	//update enemy player position
	if (newPacketFlag)
	{
		//if there is a new packet, update the current positions with that info
		gameState->players[1].playerPos = sf::Vector2f(lastPacket.playerPos, 0.f);
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

	//TODO: render the level


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
		//check for new packets
		if (socket->receive(packet.packet, ipIn, portIn) != socket->Done)
		{
			networkLock.lock();
			std::cout << "Error retrieving packet" << std::endl;
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
		outPacket.ballPos[0] = gameState->balls[0].ballPos;
		outPacket.ballPos[1] = gameState->balls[1].ballPos;
		outPacket.ballDir[0] = gameState->balls[0].ballDir;
		outPacket.ballDir[1] = gameState->balls[1].ballDir;
		outPacket.ballIsShot[0] = gameState->balls[0].isShot;
		outPacket.ballIsShot[1] = gameState->balls[1].isShot;
		outPacket.pack();
		

		//send a packet with the most up to date information
		if (socket->send(outPacket.packet, otherIp, otherPort) != socket->Done)
		{
			std::cout << "Packet failed to send" << std::endl;
		}
	}
}