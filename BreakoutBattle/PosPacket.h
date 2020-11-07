#pragma once
#include <SFML/Network/Packet.hpp>
#include <SFML/System/Vector2.hpp>

class PosPacket
{
public:
	PosPacket();
	~PosPacket();

	int testPacketNum();

	void pack();
	void unpack();

	//packet data
	int packetNum = 0;

	sf::Vector2f ballPos[2];
	sf::Vector2f ballDir[2];
	bool ballIsShot[2];
	float playerPos;
	int playerDir;

	sf::Packet packet;
};