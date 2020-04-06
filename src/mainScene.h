#ifndef MAIN_SCENE_H
#define MAIN_SCENE_H

#include <array>

#include <sp2/timer.h>
#include <sp2/scene/scene.h>
#include <sp2/scene/tilemap.h>

class Scene : public sp::Scene
{
public:
    Scene();

    void onUpdate(float delta) override;
    bool onPointerDown(sp::io::Pointer::Button button, sp::Ray3d ray, int id) override;
    void updateMap();

    bool isSolid(int x, int y);

    void loadLevel();

    static constexpr int width = 20;
    static constexpr int height = 20;
    sp::P<sp::Tilemap> mapTiles;
    std::array<std::array<bool, height>, width> mapData;

    sp::P<sp::Node> result_gui;
    sp::P<sp::Node> player;
    sp::PList<sp::Node> enemies;

    sp::Timer resultTimer;
    int levelNumber = 1;
};

#endif//MAIN_SCENE_H
