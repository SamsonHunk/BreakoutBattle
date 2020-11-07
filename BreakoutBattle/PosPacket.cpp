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
}
