#pragma once
#include <SFML/Window/Keyboard.hpp>

//handler class for keeping track of keyboard input
class InputManager
{
public:
	InputManager();
	~InputManager();

	//setter functions for the keys
	void KeyDown(int keyCode);
	void KeyUp(int keyCode);

	//grabber function
	bool GetKey(int keyCode);

private:
	bool keys[256];
};