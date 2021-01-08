#include "PosPacket.h"

PosPacket::PosPacket()
{
}

PosPacket::~PosPacket()
{
}

int PosPacket::testPacketNum()
{// get just it's number and return it
	packet >> packetNum;
	return packetNum;
}

void PosPacket::unpack()
{
	//unpack the data into the public variables
	for (int it = 0; it < 2; it++)
	{
		packet >> ballPos[it].x >> ballPos[it].y >> ballDir[it].x >> ballDir[it].y >> ballIsShot[it];
	}

	packet >> playerPos >> playerDir;

	int levelSize;
	packet >> levelSize;
	packet >> levelSize;

	for (int it = 0; it < levelSize; it++)
	{
		int value;
		packet >> value;
		levelHealth.push_back(value);
	}
}

void PosPacket::pack()
{
	packet.clear();
	//pack the data into the sfml packet
	packet << packetNum;

	for (int it = 0; it < 2; it++)
	{
		packet << ballPos[it].x << ballPos[it].y << ballDir[it].x << ballDir[it].y << ballIsShot[it];
	}

	packet << playerPos << playerDir;

	packet << levelHealth.size();

	for (int it = 0; it < levelHealth.size(); it++)
	{
		packet << levelHealth[it];
	}
}
