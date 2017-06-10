#include <irrlicht.h>
#include <vector>
#include <map>
#include <cmath>
#include <iostream>
#include "driverChoice.h"

using namespace irr;

#ifdef _MSC_VER
#pragma comment(lib, "Irrlicht.lib")
#endif

IrrlichtDevice* device;
irr::video::IVideoDriver* driver;

enum Direction
{
LEFT,
RIGHT
};

enum State
{
IDLE,
WALKING,
RUNNING,
SHOOTING,
DEAD
};

class SmartImage
{
public:
    SmartImage(irr::io::path fileName)
    {
        image = driver->getTexture(fileName);
        calculate_collision_rect(fileName);
    }
    irr::video::ITexture* get_image()
    {
        return image;
    }
    core::recti get_collision_rect()
    {
        return collisionRect;
    }
private:
    void calculate_collision_rect(irr::io::path fileName)
    {
        irr::video::IImage* image = driver->createImageFromFile(fileName);
        core::dimension2d<u32> imageDimensions = image->getDimension();
        
        u32 minX = imageDimensions.Width;
        u32 minY = imageDimensions.Height;
        u32 maxX = 0;
        u32 maxY = 0;

        static const u32 TRANSPARENT = 0;

        for(u32 x = 0; x < imageDimensions.Width; x++)
        {
            for(u32 y = 0; y < imageDimensions.Height; y++)
            {
                if(image->getPixel(x,y).getAlpha() != TRANSPARENT)
                {
                    if(x < minX)
                    {
                        minX = x;
                    }
                    if(x > maxX)
                    {
                        maxX = x;
                    }
                    if(y < minY)
                    {
                        minY = y;
                    }
                    if(y > maxY)
                    {
                        maxY = y;
                    }
                }
            }
        }
        std::cout << "Rect(" << minX << "," << minY << "," << maxX << "," << maxY << ")";
        collisionRect = core::recti(minX,minY,maxX,maxY);
        image->drop();
    }
private:
    irr::video::ITexture* image;
    core::recti collisionRect;
};

std::vector<SmartImage> load_sprites(irr::io::path fileNameBase)
{
    std::vector<SmartImage> images;
    int i = 1;
    io::path fileName = fileNameBase + io::path(" (") + io::path(i++) + io::path(").png");
    while(device->getFileSystem()->existFile(fileName))
    {
        images.push_back(SmartImage(fileName));
        fileName = fileNameBase + io::path(" (") + io::path(i++) + io::path(").png");
    }
    return images;
}

class Animation
{
public:
    Animation(irr::io::path fileNameBase, bool loop = true,u32 updateInterval = 100)
    {
        this->images = load_sprites(fileNameBase);
        this->updateInterval = updateInterval;
        this->loop = loop;
        reset();
    }
    Animation(std::vector<SmartImage> images, bool loop = true, u32 updateInterval = 100)
    {
        this->images = images;
        this->updateInterval = updateInterval;
        this->loop = loop;
        reset();
    }
    void draw_next(core::recti boundingRect, Direction direction)
    {
        /* if(we are being asked to draw despite being finished) */
        if(finished)
        {
            /* if(this animation is allowed to loop) */
            if(loop)
            {
                reset();
            }
            else
            {
                draw_current(boundingRect,direction);
                return;
            }
        }

        u32 time = device->getTimer()->getTime();
        if(time-previousTime > updateInterval)
        {
            imageIndex++;
            previousTime = time;
        }

        draw_current(boundingRect,direction);

        /* if(there is no next image in the animation) */
        if(imageIndex+1 >= images.size())
        {
            finished = true;
        }
    }
    void draw_current(core::recti boundingRect, Direction direction)
    {
        irr::video::ITexture* currentImage = images[imageIndex].get_image();
        auto imageSize = currentImage->getSize();
        int deltaX = imageSize.Width/2;
        int deltaY = imageSize.Height/2;

        core::recti sourceRect;
        if(direction == LEFT)
        {
            sourceRect = core::recti(deltaX*2,0,0,deltaY*2);
        }
        else
        {
            sourceRect = core::recti(0,0,deltaX*2,deltaY*2);
        }

        driver->draw2DImage(currentImage, boundingRect, sourceRect,0,0,true);
    }
    irr::core::recti get_collision_rect(irr::core::rectf boundingRect)
    {
        irr::core::vector2df scale = determine_scale_factor(boundingRect);
        irr::core::recti collisionRect = images[imageIndex].get_collision_rect();
        f32 ULX = collisionRect.UpperLeftCorner.X;
        f32 ULY = collisionRect.UpperLeftCorner.Y;
        f32 LRX = collisionRect.LowerRightCorner.X;
        f32 LRY = collisionRect.LowerRightCorner.Y;

        return irr::core::recti(round(ULX*scale.X),round(ULY*scale.Y),
                round(LRX*scale.X),round(LRY*scale.Y));
    }
    bool is_finished()
    {
        return finished;
    }
    void reset()
    {
        imageIndex = 0;
        previousTime = device->getTimer()->getTime();
        finished = false;
    }
private:
    irr::core::vector2df determine_scale_factor(irr::core::rectf boundingRect)
    {
        auto imageSize = images[imageIndex].get_image()->getSize();
        f32 xScale = (f32) boundingRect.getWidth() / (f32)imageSize.Width;
        f32 yScale = (f32) boundingRect.getHeight() / (f32)imageSize.Height;
        return irr::core::vector2df(xScale,yScale);
    }
private:
    std::vector<SmartImage> images;
    int imageIndex;
    u32 previousTime;
    u32 updateInterval;
    bool finished;
    bool loop;
};

class AnimationManager
{
public:
    AnimationManager(const std::map<State,Animation*> &animationMap)
    {
        this->animationMap = animationMap;
        auto iterator = animationMap.begin();
        this->state = iterator->first;
        this->direction = RIGHT;
        this->currentAnimation = animationMap.at(this->state);
    }
    State get_state()
    {
        return state;
    }
    void set_state(State state)
    {
        if(state != this->state)
        {
            auto iterator = animationMap.find(state);
            if(iterator != animationMap.end())
            {
                this->state = state;
                currentAnimation = animationMap[state];
                currentAnimation->reset();
            }
        }
    }
    Direction get_direction()
    {
        return direction;
    }
    void set_direction(Direction direction)
    {
        this->direction = direction;
    }
    irr::core::recti get_collision_rect(irr::core::rectf boundingRect)
    {
        return currentAnimation->get_collision_rect(boundingRect);
    }
    void draw_current_animation(core::recti boundingRect)
    {
        currentAnimation->draw_next(boundingRect,direction);
    }
    bool current_animation_is_finished()
    {
        return currentAnimation->is_finished();
    }
    void reset_current_animation()
    {
        currentAnimation->reset();
    }
private:
    std::map<State,Animation*> animationMap;
    Animation* currentAnimation;
    State state;
    Direction direction;
};

int round(float value)
{
    return floor(value+0.5f);
}

class EventReceiver : public IEventReceiver
{
public:
    // This is the one method that we have to implement
    virtual bool OnEvent(const SEvent& event)
    {
        // Remember whether each key is down or up
        if (event.EventType == irr::EET_KEY_INPUT_EVENT)
            KeyIsDown[event.KeyInput.Key] = event.KeyInput.PressedDown;

        return false;
    }

    // This is used to check whether a key is being held down
    virtual bool IsKeyDown(EKEY_CODE keyCode) const
    {
        return KeyIsDown[keyCode];
    }
    
    EventReceiver()
    {
        for (u32 i=0; i<KEY_KEY_CODES_COUNT; ++i)
            KeyIsDown[i] = false;
    }

private:
    // We use this array to store the current state of each key
    bool KeyIsDown[KEY_KEY_CODES_COUNT];
};

class GameObject
{
public:
GameObject(core::vector2df position, f32 width, f32 height, 
        AnimationManager* animationManager, 
        core::vector2df velocity = core::vector2df(0,0),
        EventReceiver* eventReceiver = 0)

{
    core::vector2df delta(width/2,height/2);
    core::vector2df topLeft      = position - delta;
    core::vector2df bottomRight  = position + delta;

    this->boundingRect     = core::rectf(topLeft,bottomRight);
    this->animationManager = animationManager;
    this->velocity         = velocity;
    this->eventReceiver    = eventReceiver;
    this->state            = IDLE;
    this->direction        = RIGHT;
}

irr::core::vector2df get_position()
{
    return boundingRect.getCenter();
}

irr::core::vector2di get_rounded_position()
{
    irr::core::vector2df position = get_position();
    std::cout << position.X << "," << position.Y << std::endl;
    return irr::core::vector2di(round(position.X),round(position.Y));
}

void set_position(core::vector2df position)
{
    core::vector2df difference = position - boundingRect.getCenter();
    boundingRect += difference;
}

irr::core::vector2df get_velocity()
{
    return velocity;
}

void set_velocity(core::vector2df velocity)
{
    this->velocity = velocity;
}

void update(const std::vector<GameObject*>& gameObjects)
{
    determine_state();

    boundingRect += velocity;

    for(int index = 0; index < gameObjects.size(); index++)
    {
        GameObject* other = gameObjects[index];
        if(this != other && this->has_collided(other))
        {
            std::cout << "COLLISION!" << std::endl;
            handle_collision(other);
        }
    }

    core::vector2df centre = boundingRect.getCenter();
    core::vector2df delta  = core::vector2df(boundingRect.getWidth()/2,
                                       boundingRect.getHeight()/2);
    
    core::vector2df topLeft     = centre - delta;
    core::vector2df bottomRight = centre + delta;

    core::vector2di topLeftRounded(round(topLeft.X),round(topLeft.Y));
    core::vector2di bottomRightRounded(round(bottomRight.X),round(bottomRight.Y));

    core::recti boundingRectRounded(topLeftRounded,bottomRightRounded);
    animationManager->draw_current_animation(boundingRectRounded);
}

irr::core::recti get_collision_rect()
{
    print_rect(animationManager->get_collision_rect(boundingRect));
    return animationManager->get_collision_rect(boundingRect) + get_rounded_position();
}

void print_rect(irr::core::recti rectangle)
{
    std::cout << "Rect(";
    std::cout << rectangle.UpperLeftCorner.X  << ",";
    std::cout << rectangle.UpperLeftCorner.Y  << ",";
    std::cout << rectangle.LowerRightCorner.X << ",";
    std::cout << rectangle.LowerRightCorner.Y;
    std::cout << ")" << std::endl;
}

bool has_collided(GameObject* other)
{
    std::cout << "----------" << std::endl;
    print_rect(get_collision_rect());
    print_rect(other->get_collision_rect());
    std::cout << "----------" << std::endl;
    return get_collision_rect().isRectCollided(other->get_collision_rect());
}

virtual void handle_collision(GameObject* other)
{
    /* Undo move by default */
    boundingRect -= velocity;
}

private:

    void determine_state()
    {
        if(state == DEAD)
        {
            return;
        }
        if(state == SHOOTING && !animationManager->current_animation_is_finished())
        {
            return;
        }
        if(eventReceiver)
        {
            if(eventReceiver->IsKeyDown(irr::KEY_KEY_D))
            {
                state     = RUNNING;
                direction = RIGHT;
                velocity  = core::vector2df(5,0);
            }
            else if(eventReceiver->IsKeyDown(irr::KEY_KEY_A))
            {
                state     = RUNNING;
                direction = LEFT;
                velocity  = core::vector2df(-5,0);
            }
            else if(eventReceiver->IsKeyDown(irr::KEY_SPACE))
            {
                state     = SHOOTING;
                velocity  = core::vector2df(0,0);
            }
            else
            {
                state     = IDLE;
                velocity  = core::vector2df(0,0);
            }
            animationManager->set_state(state);
            animationManager->set_direction(direction);
        }
    }

protected:
    core::rectf       boundingRect;
    core::vector2df   velocity;
    AnimationManager* animationManager;
    EventReceiver*    eventReceiver;
    State state;
    Direction direction;
};

class AdventureGirl;

class Zombie : public GameObject
{
public:
Zombie(core::vector2df position, f32 width, f32 height, 
        AnimationManager* animationManager, 
        core::vector2df velocity = core::vector2df(0,0),
        EventReceiver* eventReceiver = 0) :
GameObject(position, width, height, animationManager, velocity, eventReceiver)
{}


virtual void handle_collision(GameObject* other)
{
    GameObject::handle_collision(other);
}
};

class AdventureGirl : public GameObject
{
public:
AdventureGirl(core::vector2df position, f32 width, f32 height, 
        AnimationManager* animationManager, 
        core::vector2df velocity = core::vector2df(0,0),
        EventReceiver* eventReceiver = 0) :
GameObject(position, width, height, animationManager, velocity, eventReceiver)
{}
virtual void handle_collision(GameObject* other)
{
    if(Zombie* zombie = dynamic_cast<Zombie*>(other))
    {
        std::cout << "DEAD!" << std::endl;
        state = DEAD;
        animationManager->set_state(state);
        velocity  = core::vector2df(0,0);
    }
}
};



class AdventureGirlFactory
{
public:
static GameObject* create_adventure_girl(int x,
EventReceiver* eventReceiver = 0)
{
    check_images();

    /* Load the sprites for adventure girl */
    Animation* idleAnimation  = new Animation(adventureGirlIdleImages);
    Animation* runAnimation   = new Animation(adventureGirlRunningImages);
    Animation* shootAnimation = new Animation(adventureGirlShootingImages);
    Animation* deadAnimation  = new Animation(adventureGirlDeadImages,false);

    /* Determine which animations should play when adventure girl is in
     * different states.
     */
    std::map<State,Animation*> animationMap;
    animationMap[IDLE]     = idleAnimation;
    animationMap[RUNNING]  = runAnimation;
    animationMap[SHOOTING] = shootAnimation;
    animationMap[DEAD]     = deadAnimation;

    AnimationManager* animationManager = new AnimationManager(animationMap);
    animationManager->set_state(IDLE);

    /* Create adventure girl! */
    return new AdventureGirl(core::vector2df(x,384-(128/2)),128,128,animationManager,
            core::vector2df(0,0),eventReceiver);
}
private:
static void check_images()
{
    if(adventureGirlIdleImages.size() == 0)
    {
        adventureGirlIdleImages = load_sprites("./images/adventure_girl/Idle");
    }
    if(adventureGirlRunningImages.size() == 0)
    {
        adventureGirlRunningImages = load_sprites("./images/adventure_girl/Run");
    }
    if(adventureGirlShootingImages.size() == 0)
    {
        adventureGirlShootingImages = load_sprites("./images/adventure_girl/Shoot");
    }
    if(adventureGirlDeadImages.size() == 0)
    {
        adventureGirlDeadImages = load_sprites("./images/adventure_girl/Dead");
    }
}
private:
static std::vector<SmartImage> adventureGirlIdleImages;
static std::vector<SmartImage> adventureGirlRunningImages;
static std::vector<SmartImage> adventureGirlShootingImages;
static std::vector<SmartImage> adventureGirlDeadImages;
};
std::vector<SmartImage> AdventureGirlFactory::adventureGirlIdleImages;
std::vector<SmartImage> AdventureGirlFactory::adventureGirlRunningImages;
std::vector<SmartImage> AdventureGirlFactory::adventureGirlShootingImages;
std::vector<SmartImage> AdventureGirlFactory::adventureGirlDeadImages;

class ZombieFactory
{
public:
static GameObject* create_normal_zombie(int x)
{

    zombie1IdleImages = load_sprites("./images/zombie1/Idle");
    zombie1WalkImages = load_sprites("./images/zombie1/Walk");

    Animation* idleAnimation = new Animation(zombie1IdleImages,100);
    Animation* walkAnimation  = new Animation(zombie1WalkImages,100);

    std::map<State,Animation*> animationMap;
    animationMap[IDLE]     = idleAnimation;
    animationMap[WALKING]  = walkAnimation;
    
    AnimationManager* animationManager = new AnimationManager(animationMap);
    animationManager->set_state(WALKING);

    return new Zombie(core::vector2df(x,384-(128/2)),128,128,animationManager,
            core::vector2df(1,0));
}

GameObject* create_fast_zombie();
GameObject* create_strong_zombie();
GameObject* create_fast_and_strong_zombie();

private:
static std::vector<SmartImage> zombie1IdleImages;
static std::vector<SmartImage> zombie1WalkImages;
};
std::vector<SmartImage> ZombieFactory::zombie1IdleImages;
std::vector<SmartImage> ZombieFactory::zombie1WalkImages;

int main()
{
    /* Irrlicht engine setup */
	video::E_DRIVER_TYPE driverType=driverChoiceConsole();
	if (driverType==video::EDT_COUNT)
		return 1;
    EventReceiver* eventReceiver = new EventReceiver();
	device = createDevice(driverType, core::dimension2d<u32>(512, 384), 16,
            false, false, false, eventReceiver);
	if (device == 0)
		return 1; // could not create selected driver.
	device->setWindowCaption(L"Idle Adventure Girl");
	driver = device->getVideoDriver();

    std::cout << "-----------------------------" << std::endl;
    GameObject* adventureGirl = AdventureGirlFactory::create_adventure_girl(512/2,eventReceiver);
    std::cout << "-----------------------------" << std::endl;
    GameObject* zombie = ZombieFactory::create_normal_zombie(64);
    std::cout << "-----------------------------" << std::endl;

    std::vector<GameObject*> gameObjects;
    gameObjects.push_back(adventureGirl);
    gameObjects.push_back(zombie);

	while(device->run() && driver)
	{
		if (device->isWindowActive())
		{
			driver->beginScene(true, true, video::SColor(255,120,102,136));

            for(int index = 0; index < gameObjects.size(); index++)
            {
                gameObjects[index]->update(gameObjects);
            }
			
            driver->endScene();
		}
	}

	device->drop();

	return 0;
}
