//SFML includes
#include <SFML/Graphics.hpp>

//handler classes
#include "InputManager.h"

//function forward declarations
void update(float dt);
void render(float dt);

int main()
{
    //initial game setup

    sf::RenderWindow window(sf::VideoMode(800, 800), "Breakout Battle");
    
    InputManager inputManager;

    sf::Clock clock;



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
        update(frameTime.asSeconds());
        render(frameTime.asSeconds());
        window.display();
    }

    return 0;
}

void update(float dt)
{
    
}

void render(float dt)
{

}