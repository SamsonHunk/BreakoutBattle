#include "InputManager.h"

InputManager::InputManager()
{
	//set all keys as false
	for (int it = 0; it < 256; it++)
	{
		keys[it] = false;
	}
}

InputManager::~InputManager()
{
}

void InputManager::KeyDown(int keyCode)
{
	keys[keyCode] = true;
}

void InputManager::KeyUp(int keyCode)
{
	keys[keyCode] = false;
}

bool InputManager::GetKey(int keyCode)
{
	return keys[keyCode];
}
