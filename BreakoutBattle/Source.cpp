//includes
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <string>
#include "InputManager.h"

#define MAXPLAYERS 2
#define MAXBALLS 2

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
void checkPackets(sf::UdpSocket* socket, GameState* gameState);
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
	unsigned short port = 5400;
	std::string otherIp = "";

	{//bind UDP socket
		int answer;
		//initial game setup
		std::cout << "1 Connect ip, 2 Connet localhost" << std::endl;
		std::cin >> answer;
		switch (answer)
		{
		case 1:
			if (socket.bind(port, sf::IpAddress::getPublicAddress()) != socket.Done)
			{
				return 2; //exit game if unable to bind the socket
			}
			else
			{
				std::cout << "Bound public socket on port " << std::to_string(port) << std::endl;
			}
			break;
		case 2:
			if (socket.bind(port, sf::IpAddress::getLocalAddress()) != socket.Done)
			{
				return 2; //exit game if unable to bind the socket
			}
			else
			{
				std::cout << "Bound local socket on port " << std::to_string(port) << std::endl;
			}
			break;
		default:
			return 3;
			break;
		}

		std::cout << "1 Host game, 2 Join game" << std::endl;
		std::cin >> answer;
		switch (answer)
		{
		case 1:
			std::cout << "Waiting for player to join...";
			break;
		case 2:
			std::cout << "Enter host ip: " << std::endl;
			std::cin >> otherIp;
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
		//checkPackets(&socket, &gameState);
		update(frameTime.asSeconds(), &gameState, &inputManager, &window);
		render(frameTime.asSeconds(), &gameState, &inputManager, &renderTex, &window);
		//sendPackets(&socket, &gameState);
		window.display();
	}

	return 0;
}

void update(float dt, GameState* gameState, InputManager* inputManager, sf::RenderWindow* window)
{
	//update the player's position
	if (inputManager->GetKey(sf::Keyboard::Left))
	{
		gameState->players[0].playerPos.x -= gameState->players[0].playerSpeed * dt;
	}
	else if (inputManager->GetKey(sf::Keyboard::Right))
	{
		gameState->players[0].playerPos.x += gameState->players[0].playerSpeed * dt;
	}
	else if (inputManager->GetKey(sf::Keyboard::Space))
	{//press space to fire a ball
		for (int it = 0; it < 2; it++)
		{
			if (gameState->balls[it].lastHitBy == 1 && !gameState->balls[it].isShot)
			{
				gameState->balls[it].isShot = true;
				//angle the ball if the player is moving while shooting the ball
				if (inputManager->GetKey(sf::Keyboard::Left))
				{
					gameState->balls[it].ballDir = sf::Vector2f(-1.f, -0.5f);
				}
				else if (inputManager->GetKey(sf::Keyboard::Right))
				{
					gameState->balls[it].ballDir = sf::Vector2f(-1.f, 0.5f);
				}
				else
				{
					gameState->balls[it].ballDir = sf::Vector2f(-1.f, 0.f);
				}
				gameState->balls[it].lastHitBy = 1;
				break;
			}
		}
	}

	//update the ball positions
	for (int i = 0; i < 2; ++i)
	{
		//if the ball has already been shot then move it
		int debug = i;
		if (gameState->balls[i].isShot)
		{
			gameState->balls[i].ballPos += gameState->balls[i].ballDir * (gameState->balls[i].ballSpeed * dt);
		}
		else //else, keep it connected to the player until they release it
		{
			gameState->balls[i].ballPos = gameState->players[gameState->balls[i].lastHitBy].playerPos - sf::Vector2f(-25, 10);
		}
	}
}

void render(float dt, GameState* gameState, InputManager* inputManager, sf::RenderTexture* renderTex, sf::RenderWindow* window)
{
	//render all the objects onto the screen


	for (int it = 0; it < 2; it++)
	{
		gameState->players[it].playerShape.setPosition(gameState->players->playerPos);
		window->draw(gameState->players[it].playerShape);
	}


	//TODO: render the level


	//render the balls

	for (int it = 0; it < 2; it++)
	{
		gameState->balls[it].ballShape.setPosition(gameState->balls[it].ballPos);
		window->draw(gameState->balls[it].ballShape);
	}

}

void checkPackets(sf::UdpSocket* socket, GameState* gameState)
{
	//check for any new packets and apply their data to the gamestate
}

void sendPackets(sf::UdpSocket* socket, GameState* gameState)
{
	//send a packet with the most up to date information
}