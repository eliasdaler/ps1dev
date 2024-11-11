#include <psyqo/application.hh>
#include <psyqo/font.hh>
#include <psyqo/gpu.hh>
#include <psyqo/scene.hh>

namespace
{

class Game final : public psyqo::Application {
    void prepare() override;
    void createScene() override;

public:
    psyqo::Font<> systemFont;
};

class Scene final : public psyqo::Scene {
    void frame() override;
};

Game hello;
Scene helloScene;

}

void Game::prepare()
{
    psyqo::GPU::Configuration config;
    config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::AUTO)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(config);
}

void Game::createScene()
{
    systemFont.uploadSystemFont(gpu());
    pushScene(&helloScene);
}

void Scene::frame()
{
    psyqo::Color bg{{.r = 0, .g = 64, .b = 91}};
    hello.gpu().clear(bg);
    psyqo::Color c = {{.r = 255, .g = 255, .b = 255}};
    hello.systemFont.print(hello.gpu(), "Hello world!", {{.x = 16, .y = 32}}, c);
}

int main()
{
    return hello.run();
}
