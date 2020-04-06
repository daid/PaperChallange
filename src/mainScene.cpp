#include "mainScene.h"
#include "main.h"

#include <sp2/random.h>
#include <sp2/audio/sound.h>
#include <sp2/timer.h>
#include <sp2/graphics/gui/loader.h>
#include <sp2/scene/node.h>
#include <sp2/scene/camera.h>
#include <sp2/scene/tilemap.h>
#include <sp2/scene/particleEmitter.h>
#include <sp2/graphics/spriteAnimation.h>
#include <sp2/graphics/meshdata.h>
#include <sp2/collision/2d/box.h>

class Enemy : public sp::Node
{
public:
    Enemy(sp::P<sp::Node> parent, const sp::string& type)
    : sp::Node(parent)
    {
        setAnimation(sp::SpriteAnimation::load("Paper-Pixels/enemies.txt"));

        animationPlay(type);
        animationSetFlags(sp::SpriteAnimation::FlipFlag);

        sp::collision::Box2D shape(0.6, 0.6);
        shape.type = sp::collision::Shape::Type::Sensor;
        shape.fixed_rotation = true;
        setCollisionShape(shape);
    }

    void takeDamage()
    {
        sp::audio::Sound::play("sfx/qubodupImpactMeat01.wav");
        auto pe = new sp::ParticleEmitter(getParent(), "die.txt");
        pe->setPosition(getPosition2D());
        delete this;
    }

    void onCollision(sp::CollisionInfo& info) override;
};

class Gunshot : public sp::Node
{
public:
    Gunshot(sp::P<sp::Node> parent, sp::Vector2d position, double forward)
    : sp::Node(parent)
    {
        setPosition(position + sp::Vector2d(25.5, 0) * forward);
        render_data.type = sp::RenderData::Type::Normal;
        render_data.shader = sp::Shader::get("internal:color.shader");
        render_data.color = sp::Color(0, 0, 0, 0.5);
        render_data.order = 10;
        render_data.mesh = sp::MeshData::createQuad(sp::Vector2f(50, 1.0/8.0));

        sp::P<Scene> scene = getScene();
        scene->queryCollision(sp::Rect2d(position, sp::Vector2d(forward * 50, 0)), [](sp::P<sp::Node> obj)
        {
            sp::P<Enemy> enemy = obj;
            if (enemy)
                enemy->takeDamage();
            return true;
        });
    }

    void onUpdate(float delta) override
    {
        timeout -= delta;
        if (timeout <= 0.0)
            delete this;
    }

    float timeout = 0.2;
};

class Player : public sp::Node
{
public:
    Player(sp::P<sp::Node> parent)
    : sp::Node(parent)
    {
        setAnimation(sp::SpriteAnimation::load("Paper-Pixels/player.txt"));

        animationPlay("IDLE");

        sp::collision::Box2D shape(0.8, 1.0);
        shape.type = sp::collision::Shape::Type::Dynamic;
        shape.fixed_rotation = true;
        setCollisionShape(shape);

        startTimer.start(2.0);
    }

    virtual void onFixedUpdate() override
    {
        auto velocity = getLinearVelocity2D();
        auto forward = 1.0;
        if (animationGetFlags() & sp::SpriteAnimation::FlipFlag)
            forward = -forward;

        sp::P<Scene> scene = getScene();
        if (!scene->isSolid(getPosition2D().x, getPosition2D().y - 1.0) && !scene->isSolid(getPosition2D().x + forward * 0.5, getPosition2D().y - 1.0))
        {
            if (canJump)
            {
                velocity.y += 8.6;
                sp::audio::Sound::play("sfx/jump_04.wav");
            }
        }
        if (gunTimer.isExpired())
        {
            new Gunshot(getParent(), getPosition2D(), forward);
            sp::audio::Sound::play("sfx/silencer.wav");
        }

        if (startTimer.isRunning())
            animationPlay("IDLE");
        else if (std::abs(velocity.y) < 0.01)
            animationPlay("RUN");
        else
            animationPlay("JUMP");

        if (getPosition2D().x < 0.5)
            animationSetFlags(0);
        if (getPosition2D().x > Scene::width - 0.5)
            animationSetFlags(sp::SpriteAnimation::FlipFlag);

        velocity.y -= 0.3;
        if (!startTimer.isRunning())
            velocity.x = 1.5 * forward;
        if (startTimer.isExpired())
            gunTimer.repeat(3.0);
        setLinearVelocity(velocity);
        canJump = false;

        if (getPosition2D().y < -1)
            takeDamage();
    }

    virtual void onCollision(sp::CollisionInfo& info) override
    {
        if (info.other && info.other->isSolid() && (info.normal - sp::Vector2d(0, -1)).length() < 0.01)
        {
            canJump = true;
        }
    }

    void takeDamage()
    {
        sp::audio::Sound::play("sfx/qubodupImpactMeat01.wav");
        auto pe = new sp::ParticleEmitter(getParent(), "die.txt");
        pe->setPosition(getPosition2D());
        delete this;
    }

    bool canJump = false;
    sp::Timer startTimer;
    sp::Timer gunTimer;
};

Scene::Scene()
: sp::Scene("MAIN")
{
    auto camera = new sp::Camera(getRoot());
    camera->setOrtographic(sp::Vector2d(width * 0.5, height * 0.5));
    camera->setPosition(sp::Vector2d(width * 0.5, height * 0.5));
    setDefaultCamera(camera);

    auto backdrop = new sp::Tilemap(getRoot(), "Paper-Pixels/Tiles.png", 1, 1, 144/8, 320/8);
    backdrop->render_data.order = -100;
    for(int x=0;x<width;x++)
        for(int y=0;y<height;y++)
            backdrop->setTile(x, y, 18 * 15);
    mapTiles = new sp::Tilemap(getRoot(), "Paper-Pixels/Tiles.png", 1, 1, 144/8, 320/8);

    loadLevel();
}

void Scene::loadLevel()
{
    for(auto enemy : enemies)
        enemy.destroy();
    player.destroy();

    result_gui.destroy();
    auto stream = sp::io::ResourceProvider::get("levels/" + sp::string(levelNumber) + ".txt");
    for(int y=19; y>=0; y--)
    {
        auto line = stream->readLine();
        for(int x=0; x<20; x++)
        {
            mapData[x][y] = false;
            switch(line[x])
            {
            case '#': mapData[x][y] = true; break;
            case 'P':
                {
                    player = new Player(getRoot());
                    player->setPosition(sp::Vector2d(x + 0.5, y + 0.5));
                }
                break;
            case 'C':
                {
                    auto enemy = new Enemy(getRoot(), "CHICKEN");
                    enemy->setPosition(sp::Vector2d(x + .5, y + .5));
                    enemies.add(enemy);
                }
                break;
            case 'S':
                {
                    auto enemy = new Enemy(getRoot(), "SNAKE");
                    enemy->setPosition(sp::Vector2d(x + .5, y + .5));
                    enemies.add(enemy);
                }
                break;
            case 'B':
                {
                    auto enemy = new Enemy(getRoot(), "BEE");
                    enemy->setPosition(sp::Vector2d(x + .5, y + .5));
                    enemies.add(enemy);
                }
                break;
            case 'M':
                {
                    auto enemy = new Enemy(getRoot(), "MOUSE");
                    enemy->setPosition(sp::Vector2d(x + .5, y + .5));
                    enemies.add(enemy);
                }
                break;
            case 'T':
                {
                    auto enemy = new Enemy(getRoot(), "THING");
                    enemy->setPosition(sp::Vector2d(x + .5, y + .5));
                    enemies.add(enemy);
                }
                break;
            }
        }
    }
    updateMap();
}

bool Scene::onPointerDown(sp::io::Pointer::Button button, sp::Ray3d ray, int id)
{
    int x = ray.start.x;
    int y = ray.start.y;
    if (x < 0 || y < 0 || x >= width || y >= height)
        return false;
    mapData[x][y] = !mapData[x][y];
    updateMap();
    return true;
}

void Scene::updateMap()
{
    for(int x=0;x<width;x++)
    {
        for(int y=0;y<height;y++)
        {
            bool left = x > 0 ? mapData[x - 1][y] : true;
            bool right = x < width - 1 ? mapData[x + 1][y] : true;
            //bool down = y > 0 ? mapData[x - 1][y] : false;
            //bool up = y < height - 1 ? mapData[x + 1][y] : true;
            if (mapData[x][y])
            {
                if (!left)
                    mapTiles->setTile(x, y, 18 * 6 + 3, sp::Tilemap::Collision::Platform);
                else if (!right)
                    mapTiles->setTile(x, y, 18 * 6 + 6, sp::Tilemap::Collision::Platform);
                else
                    mapTiles->setTile(x, y, 18 * 6 + 4, sp::Tilemap::Collision::Platform);
            }
            else
                mapTiles->setTile(x, y, -1);
        }
    }
}

void Scene::onUpdate(float delta)
{
    if (!player && !result_gui)
    {
        auto result = sp::gui::Loader::load("gui/result.gui", "RESULT");
        result->setAttribute("caption", "You died...");
        result_gui = result;
        resultTimer.start(3.0);
    }
    if (enemies.empty() && !result_gui)
    {
        auto result = sp::gui::Loader::load("gui/result.gui", "RESULT");
        result->setAttribute("caption", "You win!");
        result_gui = result;
        resultTimer.start(3.0);
        levelNumber += 1;
    }
    if (resultTimer.isExpired())
    {
        loadLevel();
    }
    if (escape_key.getUp())
    {
        delete this;
        openMainMenu();
        return;
    }
}

bool Scene::isSolid(int x, int y)
{
    if (x < 0 || x >= width)
        return true;
    if (y < 0 || y >= height)
        return false;
    return mapData[x][y];
}

void Enemy::onCollision(sp::CollisionInfo& info)
{
    sp::P<Player> player = info.other;
    if (player)
        player->takeDamage();
}
