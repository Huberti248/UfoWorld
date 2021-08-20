#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
#include <SDL_ttf.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
//#include <SDL_gpu.h>
//#include <SFML/Network.hpp>
//#include <SFML/Graphics.hpp>
#include <algorithm>
#include <atomic>
#include <codecvt>
#include <functional>
#include <locale>
#include <mutex>
#include <thread>
#ifdef __ANDROID__
#include "vendor/PUGIXML/src/pugixml.hpp"
#include <android/log.h> //__android_log_print(ANDROID_LOG_VERBOSE, "UfoWorld", "Example number log: %d", number);
#include <jni.h>
#else
#include <filesystem>
#include <pugixml.hpp>
#ifdef __EMSCRIPTEN__
namespace fs = std::__fs::filesystem;
#else
namespace fs = std::filesystem;
#endif
using namespace std::chrono_literals;
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

// NOTE: Remember to uncomment it on every release
//#define RELEASE

#if defined _MSC_VER && defined RELEASE
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

//240 x 240 (smart watch)
//240 x 320 (QVGA)
//360 x 640 (Galaxy S5)
//640 x 480 (480i - Smallest PC monitor)

#define GAME_SPEED 0.5

int windowWidth = 240;
int windowHeight = 320;
SDL_Point mousePos;
SDL_Point realMousePos;
bool keys[SDL_NUM_SCANCODES];
bool buttons[SDL_BUTTON_X2 + 1];
SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* robotoF;
bool running = true;

#define COW_TRAVEL_DISTANCE 10
#define MAX_TRAVEL_DISTANCE 100
#define DEFAULT_COW_SPAWN_DELAY 3000
#define DEFAULT_MAX_HEARTS 1
#define MIN_HEARTS_COUNT 1
#define FLYING_ENEMY_LAST_FRAME_PLUS_ONE 8

void logOutputCallback(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
    std::cout << message << std::endl;
}

int random(int min, int max)
{
    return min + rand() % ((max + 1) - min);
}

int SDL_QueryTextureF(SDL_Texture* texture, Uint32* format, int* access, float* w, float* h)
{
    int wi, hi;
    int result = SDL_QueryTexture(texture, format, access, &wi, &hi);
    if (w) {
        *w = wi;
    }
    if (h) {
        *h = hi;
    }
    return result;
}

SDL_bool SDL_PointInFRect(const SDL_Point* p, const SDL_FRect* r)
{
    return ((p->x >= r->x) && (p->x < (r->x + r->w)) && (p->y >= r->y) && (p->y < (r->y + r->h))) ? SDL_TRUE : SDL_FALSE;
}

std::ostream& operator<<(std::ostream& os, SDL_FRect r)
{
    os << r.x << " " << r.y << " " << r.w << " " << r.h;
    return os;
}

std::ostream& operator<<(std::ostream& os, SDL_Rect r)
{
    SDL_FRect fR;
    fR.w = r.w;
    fR.h = r.h;
    fR.x = r.x;
    fR.y = r.y;
    os << fR;
    return os;
}

SDL_Texture* renderText(SDL_Texture* previousTexture, TTF_Font* font, SDL_Renderer* renderer, const std::string& text, SDL_Color color)
{
    if (previousTexture) {
        SDL_DestroyTexture(previousTexture);
    }
    SDL_Surface* surface;
    if (text.empty()) {
        surface = TTF_RenderUTF8_Blended(font, " ", color);
    }
    else {
        surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    }
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        return texture;
    }
    else {
        return 0;
    }
}

struct Text {
    std::string text;
    SDL_Surface* surface = 0;
    SDL_Texture* t = 0;
    SDL_FRect dstR{};
    bool autoAdjustW = false;
    bool autoAdjustH = false;
    float wMultiplier = 1;
    float hMultiplier = 1;

    ~Text()
    {
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
    }

    Text()
    {
    }

    Text(const Text& rightText)
    {
        text = rightText.text;
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (rightText.surface) {
            surface = SDL_ConvertSurface(rightText.surface, rightText.surface->format, SDL_SWSURFACE);
        }
        if (rightText.t) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
        dstR = rightText.dstR;
        autoAdjustW = rightText.autoAdjustW;
        autoAdjustH = rightText.autoAdjustH;
        wMultiplier = rightText.wMultiplier;
        hMultiplier = rightText.hMultiplier;
    }

    Text& operator=(const Text& rightText)
    {
        text = rightText.text;
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (rightText.surface) {
            surface = SDL_ConvertSurface(rightText.surface, rightText.surface->format, SDL_SWSURFACE);
        }
        if (rightText.t) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
        dstR = rightText.dstR;
        autoAdjustW = rightText.autoAdjustW;
        autoAdjustH = rightText.autoAdjustH;
        wMultiplier = rightText.wMultiplier;
        hMultiplier = rightText.hMultiplier;
        return *this;
    }

    void setText(SDL_Renderer* renderer, TTF_Font* font, std::string text, SDL_Color c = { 255, 255, 255 })
    {
        this->text = text;
#if 1 // NOTE: renderText
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (text.empty()) {
            surface = TTF_RenderUTF8_Blended(font, " ", c);
        }
        else {
            surface = TTF_RenderUTF8_Blended(font, text.c_str(), c);
        }
        if (surface) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
#endif
        if (autoAdjustW) {
            SDL_QueryTextureF(t, 0, 0, &dstR.w, 0);
        }
        if (autoAdjustH) {
            SDL_QueryTextureF(t, 0, 0, 0, &dstR.h);
        }
        dstR.w *= wMultiplier;
        dstR.h *= hMultiplier;
    }

    void setText(SDL_Renderer* renderer, TTF_Font* font, int value, SDL_Color c = { 255, 255, 255 })
    {
        setText(renderer, font, std::to_string(value), c);
    }

    void draw(SDL_Renderer* renderer)
    {
        if (t) {
            SDL_RenderCopyF(renderer, t, 0, &dstR);
        }
    }
};

int SDL_RenderDrawCircle(SDL_Renderer* renderer, int x, int y, int radius)
{
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while (offsety >= offsetx) {
        status += SDL_RenderDrawPoint(renderer, x + offsetx, y + offsety);
        status += SDL_RenderDrawPoint(renderer, x + offsety, y + offsetx);
        status += SDL_RenderDrawPoint(renderer, x - offsetx, y + offsety);
        status += SDL_RenderDrawPoint(renderer, x - offsety, y + offsetx);
        status += SDL_RenderDrawPoint(renderer, x + offsetx, y - offsety);
        status += SDL_RenderDrawPoint(renderer, x + offsety, y - offsetx);
        status += SDL_RenderDrawPoint(renderer, x - offsetx, y - offsety);
        status += SDL_RenderDrawPoint(renderer, x - offsety, y - offsetx);

        if (status < 0) {
            status = -1;
            break;
        }

        if (d >= 2 * offsetx) {
            d -= 2 * offsetx + 1;
            offsetx += 1;
        }
        else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        }
        else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}

int SDL_RenderFillCircle(SDL_Renderer* renderer, int x, int y, int radius)
{
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while (offsety >= offsetx) {

        status += SDL_RenderDrawLine(renderer, x - offsety, y + offsetx,
            x + offsety, y + offsetx);
        status += SDL_RenderDrawLine(renderer, x - offsetx, y + offsety,
            x + offsetx, y + offsety);
        status += SDL_RenderDrawLine(renderer, x - offsetx, y - offsety,
            x + offsetx, y - offsety);
        status += SDL_RenderDrawLine(renderer, x - offsety, y - offsetx,
            x + offsety, y - offsetx);

        if (status < 0) {
            status = -1;
            break;
        }

        if (d >= 2 * offsetx) {
            d -= 2 * offsetx + 1;
            offsetx += 1;
        }
        else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        }
        else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}

struct Clock {
    Uint64 start = SDL_GetPerformanceCounter();

    float getElapsedTime()
    {
        Uint64 stop = SDL_GetPerformanceCounter();
        float secondsElapsed = (stop - start) / (float)SDL_GetPerformanceFrequency();
        return secondsElapsed * 1000;
    }

    float restart()
    {
        Uint64 stop = SDL_GetPerformanceCounter();
        float secondsElapsed = (stop - start) / (float)SDL_GetPerformanceFrequency();
        start = SDL_GetPerformanceCounter();
        return secondsElapsed * 1000;
    }
};

SDL_bool SDL_FRectEmpty(const SDL_FRect* r)
{
    return ((!r) || (r->w <= 0) || (r->h <= 0)) ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_IntersectFRect(const SDL_FRect* A, const SDL_FRect* B, SDL_FRect* result)
{
    int Amin, Amax, Bmin, Bmax;

    if (!A) {
        SDL_InvalidParamError("A");
        return SDL_FALSE;
    }

    if (!B) {
        SDL_InvalidParamError("B");
        return SDL_FALSE;
    }

    if (!result) {
        SDL_InvalidParamError("result");
        return SDL_FALSE;
    }

    /* Special cases for empty rects */
    if (SDL_FRectEmpty(A) || SDL_FRectEmpty(B)) {
        result->w = 0;
        result->h = 0;
        return SDL_FALSE;
    }

    /* Horizontal intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin)
        Amin = Bmin;
    result->x = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    result->w = Amax - Amin;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin)
        Amin = Bmin;
    result->y = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    result->h = Amax - Amin;

    return (SDL_FRectEmpty(result) == SDL_TRUE) ? SDL_FALSE : SDL_TRUE;
}

SDL_bool SDL_HasIntersectionF(const SDL_FRect* A, const SDL_FRect* B)
{
    int Amin, Amax, Bmin, Bmax;

    if (!A) {
        SDL_InvalidParamError("A");
        return SDL_FALSE;
    }

    if (!B) {
        SDL_InvalidParamError("B");
        return SDL_FALSE;
    }

    /* Special cases for empty rects */
    if (SDL_FRectEmpty(A) || SDL_FRectEmpty(B)) {
        return SDL_FALSE;
    }

    /* Horizontal intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    if (Amax <= Amin)
        return SDL_FALSE;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    if (Amax <= Amin)
        return SDL_FALSE;

    return SDL_TRUE;
}

int eventWatch(void* userdata, SDL_Event* event)
{
    // WARNING: Be very careful of what you do in the function, as it may run in a different thread
    if (event->type == SDL_APP_TERMINATING || event->type == SDL_APP_WILLENTERBACKGROUND) {
    }
    return 0;
}

struct Entity {
    SDL_FRect r{};
    int dx = 0;
};

enum class EnemyType {
    Landing,
    Flying,
};

struct Collectable {
    SDL_FRect r{};
    bool isLeft = false;
    int turns = 0;
    EnemyType enemyType = EnemyType::Landing;
    int currTexture = 0;
};

enum class State {
    Gameplay,
    Shop,
    Ufos,
    Credits,
};

SDL_Texture* ufoT;
SDL_Texture* cowT;
SDL_Texture* heartT;
SDL_Texture* shopT;
SDL_Texture* buyT;
SDL_Texture* backArrowT;
SDL_Texture* backgroundT;
SDL_Texture* mutedT;
SDL_Texture* unmutedT;
SDL_Texture* alienT;
std::vector<SDL_Texture*> flyingEnemyTextures;
SDL_Texture* ufoWithRedLaserT;
SDL_Texture* redLaserT;
SDL_Texture* moreCowsUpgradeT;
SDL_Texture* manyUfosT;
SDL_Texture* addT;
SDL_Texture* leftArrowT;
SDL_Texture* collectAllT;
SDL_Texture* shieldT;
SDL_Texture* bubbleT;
SDL_Texture* goLeftT;
SDL_Texture* goRightT;
SDL_Texture* yellowLaserT;
Mix_Music* music;
Mix_Chunk* pickupS;
Mix_Chunk* powerupS;
Mix_Chunk* hitS;
Mix_Chunk* laserSwitchS;
Entity player;
Clock globalClock;
std::vector<Collectable> cows;
std::vector<Collectable> enemies;
Clock cowClock;
Clock enemyClock;
Clock heartClock;
Clock cowsClock;
Text scoreText;
std::vector<SDL_FRect> hearthRects;
SDL_FRect shopR;
State state = State::Gameplay;
Text buyHeartPriceText;
SDL_FRect buyHeartR;
SDL_FRect buyHeartBtnR;
Text buyLaserPriceText;
SDL_FRect buyLaserR;
SDL_FRect buyLaserBtnR;
Text buyMoreCowsPriceText;
SDL_FRect buyMoreCowsR;
SDL_FRect buyMoreCowsBtnR;
Text buyShieldPriceText;
SDL_FRect buyShieldR;
SDL_FRect buyShieldBtnR;
Text buyUfosPriceText;
SDL_FRect buyUfosR;
SDL_FRect buyUfosBtnR;
SDL_FRect backArrowR;
SDL_FRect backgroundDstR;
SDL_FRect backgroundDstR2;
SDL_FRect backgroundDstR3;
SDL_FRect soundBtnR;
bool hasSounds = true;
int maxHeartsCount = 1;
std::string prefPath;
bool laserBought = false;
bool hasLaser = false;
float cowSpawnDelay = DEFAULT_COW_SPAWN_DELAY;
std::vector<Collectable> hearts;
SDL_FRect ufosBtnR;
Text scorePerSecondText;
Text collectedScoreText;
SDL_FRect addUfoBtnR;
Text addUfoPriceText;
SDL_FRect collectAllBtnR;
Text ufosCountText;
bool hasShield = false;
bool boughtUfos = false;
Text fooText;
Text creditsText;
std::vector<Text> credits;
Text creditsInfoText;
Text authorText;
Text authorValueText;
Text graphicsText;
Text graphicsValueText;
Text otherText;
SDL_FRect goLeftR;
SDL_FRect goRightR;
SDL_FRect laserBtnR;

Collectable generateCollectable(Entity player)
{
    Collectable collectable;
    do {
        collectable.r.w = 32;
        collectable.r.h = 32;
        collectable.r.x = random(0, 1) ? -collectable.r.w : windowWidth;
        collectable.r.y = windowHeight - collectable.r.h;
    } while (SDL_HasIntersectionF(&collectable.r, &player.r));
    collectable.enemyType = (EnemyType)random(0, 1);
    return collectable;
}

void muteMusicAndSounds()
{
    Mix_VolumeMusic(0);
    Mix_Volume(-1, 0);
}

void unmuteMusicAndSounds()
{
    Mix_VolumeMusic(128);
    Mix_Volume(-1, 128);
}

float lerp(float source, float destination, float speed)
{
    return source + (destination - source) * speed;
}

void addHeart(std::vector<SDL_FRect>& hearthRects)
{
    hearthRects.push_back(SDL_FRect());
    if (hearthRects.size() == 1) {
        hearthRects.back().w = 32;
        hearthRects.back().h = 32;
        hearthRects.back().x = 0;
        hearthRects.back().y = 0;
    }
    else {
        hearthRects.back() = hearthRects[hearthRects.size() - 2];
        hearthRects.back().x = hearthRects[hearthRects.size() - 2].x + hearthRects[hearthRects.size() - 2].w + 5;
        if (hearthRects.size() > 3) {
            if (hearthRects.size() == 4) {
                hearthRects.back().x = hearthRects.front().x;
            }
            hearthRects.back().y = hearthRects.front().x + hearthRects.front().h;
        }
    }
}

void readData(Text& scoreText, int& maxHeartsCount, bool& laserBought, float& cowSpawnDelay)
{
#ifdef __EMSCRIPTEN__
    scoreText.setText(renderer, robotoF,
        EM_ASM_INT({
            var s = localStorage.getItem("score");
            if (s) {
                return s;
            }
            else {
                return $0;
            }
        },
            0),
        {});
    maxHeartsCount = EM_ASM_INT({
        var s = localStorage.getItem("maxHearts");
        if (s) {
            return s;
        }
        else {
            return $0;
        }
    },
        DEFAULT_MAX_HEARTS);
    laserBought = EM_ASM_INT({
        var s = localStorage.getItem("laserBought");
        if (s) {
            return s;
        }
        else {
            return $0;
        }
    });
    cowSpawnDelay = EM_ASM_INT({
        var s = localStorage.getItem("cowSpawnDelay");
        if (s) {
            return s;
        }
        else {
            return $0;
        }
    },
        DEFAULT_COW_SPAWN_DELAY);
    int hearts = EM_ASM_INT({
        var s = localStorage.getItem("hearts");
        if (s) {
            return s;
        }
        else {
            return $0;
        }
    },
        MIN_HEARTS_COUNT);
    for (int i = 0; i < hearts; ++i) {
        addHeart(hearthRects);
    }
    int ufosCount = EM_ASM_INT({
        var s = localStorage.getItem("ufosCount");
        if (s) {
            return s;
        }
        else {
            return $0;
        }
    },
        0);
    ufosCountText.setText(renderer, robotoF, ufosCount, {});
    scorePerSecondText.setText(renderer, robotoF, std::to_string(ufosCount) + "/s", {});
    collectedScoreText.setText(renderer, robotoF, EM_ASM_INT({
        var s = localStorage.getItem("collectedScore");
        if (s) {
            return s;
        }
        else {
            return $0;
        }
    },
                                                      0),
        {});
    boughtUfos = EM_ASM_INT({
        var s = localStorage.getItem("boughtUfos");
        if (s) {
            return s;
        }
        else {
            return $0;
        }
    },
        0);
    hasShield = EM_ASM_INT({
        var s = localStorage.getItem("hasShield");
        if (s) {
            return s;
        }
        else {
            return $0;
        }
    },
        0);
#else
    pugi::xml_document doc;
    doc.load_file((prefPath + "data.xml").c_str());
    pugi::xml_node rootNode = doc.child("root");
    scoreText.setText(renderer, robotoF, rootNode.child("score").text().as_string("0"), {});
    maxHeartsCount = rootNode.child("maxHearts").text().as_int(DEFAULT_MAX_HEARTS);
    laserBought = rootNode.child("laserBought").text().as_bool();
    cowSpawnDelay = rootNode.child("cowSpawnDelay").text().as_float(DEFAULT_COW_SPAWN_DELAY);
    for (int i = 0; i < rootNode.child("hearts").text().as_int(MIN_HEARTS_COUNT); ++i) {
        addHeart(hearthRects);
    }
    ufosCountText.setText(renderer, robotoF, rootNode.child("ufosCount").text().as_int(), {});
    scorePerSecondText.setText(renderer, robotoF, ufosCountText.text + "/s", {});
    collectedScoreText.setText(renderer, robotoF, rootNode.child("collectedScore").text().as_int(), {});
    boughtUfos = rootNode.child("boughtUfos").text().as_bool();
    hasShield = rootNode.child("hasShield").text().as_bool();
#endif
}

void saveData()
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
        localStorage.setItem("score", $0);
    },
        std::stoi(scoreText.text));
    EM_ASM({
        localStorage.setItem("maxHearts", $0);
    },
        maxHeartsCount);
    EM_ASM({
        localStorage.setItem("laserBought", $0);
    },
        laserBought);
    EM_ASM({
        localStorage.setItem("cowSpawnDelay", $0);
    },
        cowSpawnDelay);
    EM_ASM({
        localStorage.setItem("hearts", $0);
    },
        hearthRects.size());
    EM_ASM({
        localStorage.setItem("ufosCount", $0);
    },
        std::stoi(ufosCountText.text));
    EM_ASM({
        localStorage.setItem("collectedScore", $0);
    },
        std::stoi(collectedScoreText.text));
    EM_ASM({
        localStorage.setItem("boughtUfos", $0);
    },
        boughtUfos);
    EM_ASM({
        localStorage.setItem("hasShield", $0);
    },
        hasShield);
#else
    pugi::xml_document doc;
    pugi::xml_node rootNode = doc.append_child("root");
    pugi::xml_node scoreNode = rootNode.append_child("score");
    scoreNode.append_child(pugi::node_pcdata).set_value(scoreText.text.c_str());
    pugi::xml_node maxHeartsNode = rootNode.append_child("maxHearts");
    maxHeartsNode.append_child(pugi::node_pcdata).set_value(std::to_string(maxHeartsCount).c_str());
    pugi::xml_node laserBoughtNode = rootNode.append_child("laserBought");
    laserBoughtNode.append_child(pugi::node_pcdata).set_value(std::to_string(laserBought).c_str());
    pugi::xml_node cowSpawnDelayNode = rootNode.append_child("cowSpawnDelay");
    cowSpawnDelayNode.append_child(pugi::node_pcdata).set_value(std::to_string(cowSpawnDelay).c_str());
    pugi::xml_node heartsNode = rootNode.append_child("hearts");
    heartsNode.append_child(pugi::node_pcdata).set_value(std::to_string(hearthRects.size()).c_str());
    pugi::xml_node ufosCountNode = rootNode.append_child("ufosCount");
    ufosCountNode.append_child(pugi::node_pcdata).set_value(ufosCountText.text.c_str());
    pugi::xml_node collectedScoreNode = rootNode.append_child("collectedScore");
    collectedScoreNode.append_child(pugi::node_pcdata).set_value(collectedScoreText.text.c_str());
    pugi::xml_node boughtUfosNode = rootNode.append_child("boughtUfos");
    boughtUfosNode.append_child(pugi::node_pcdata).set_value(std::to_string(boughtUfos).c_str());
    pugi::xml_node hasShieldNode = rootNode.append_child("hasShield");
    hasShieldNode.append_child(pugi::node_pcdata).set_value(std::to_string(hasShield).c_str());
    doc.save_file((prefPath + "data.xml").c_str());
#endif
}

void mainLoop()
{
    float deltaTime = globalClock.restart();
    if (cowsClock.getElapsedTime() > 1000) {
        collectedScoreText.setText(renderer, robotoF, std::stoi(collectedScoreText.text) + std::stoi(ufosCountText.text), {});
        cowsClock.restart();
    }
    if (state == State::Gameplay) {
        player.dx = 0;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
                saveData();
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                keys[event.key.keysym.scancode] = true;
                if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    if (laserBought) {
                        hasLaser = !hasLaser;
                        Mix_PlayChannel(-1, laserSwitchS, 0);
                    }
                }
            }
            if (event.type == SDL_KEYUP) {
                keys[event.key.keysym.scancode] = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
                if (SDL_PointInFRect(&mousePos, &shopR)) {
                    state = State::Shop;
                }
                if (SDL_PointInFRect(&mousePos, &soundBtnR)) {
                    hasSounds = !hasSounds;
                    if (hasSounds) {
                        unmuteMusicAndSounds();
                    }
                    else {
                        muteMusicAndSounds();
                    }
                }
                if (SDL_PointInFRect(&mousePos, &ufosBtnR) && boughtUfos) {
                    state = State::Ufos;
                }
                if (SDL_PointInFRect(&mousePos, &creditsText.dstR)) {
                    state = State::Credits;
                }
#ifdef __ANDROID__
                if (SDL_PointInFRect(&mousePos, &laserBtnR)) {
                    if (laserBought) {
                        hasLaser = !hasLaser;
                        Mix_PlayChannel(-1, laserSwitchS, 0);
                    }
                }
#endif
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                buttons[event.button.button] = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                float scaleX, scaleY;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                mousePos.x = event.motion.x / scaleX;
                mousePos.y = event.motion.y / scaleY;
                realMousePos.x = event.motion.x;
                realMousePos.y = event.motion.y;
            }
        }
#ifdef __ANDROID__
        if (buttons[SDL_BUTTON_LEFT]) {
            if (SDL_PointInFRect(&mousePos, &goLeftR)) {
                player.dx = -1;
            }
            if (SDL_PointInFRect(&mousePos, &goRightR)) {
                player.dx = 1;
            }
        }
#endif
        if (keys[SDL_SCANCODE_A]) {
            player.dx = -1;
        }
        else if (keys[SDL_SCANCODE_D]) {
            player.dx = 1;
        }
        backgroundDstR.x += -player.dx * deltaTime * GAME_SPEED;
        backgroundDstR2.x += -player.dx * deltaTime * GAME_SPEED;
        backgroundDstR3.x += -player.dx * deltaTime * GAME_SPEED;
        for (int i = 0; i < cows.size(); ++i) {
            cows[i].r.x += -player.dx * deltaTime * GAME_SPEED;
            if (player.dx == 0) {
#if 0
                if (cows[i].walkState == WalkState::Waiting) {
                    if (cows[i].waitClock.getElapsedTime() > 3000) {
                        if (random(0, 1)) {
                            cows[i].walkState = WalkState::Moving;
                            cows[i].dstX = random(cows[i].r.x - MAX_TRAVEL_DISTANCE, cows[i].r.x + MAX_TRAVEL_DISTANCE);
                        }
                        else {
                            cows[i].walkState = WalkState::Waiting;
                            cows[i].waitClock.restart();
                        }
                    }
                }
                else {
                    if (cows[i].dstX - cows[i].r.x > 0) {
                        cows[i].isLeft = false;
                    }
                    else if (cows[i].dstX - cows[i].r.x < 0) {
                        cows[i].isLeft = true;
                    }
                    cows[i].r.x = lerp(cows[i].r.x, cows[i].dstX, deltaTime * GAME_SPEED);

                    if (cows[i].r.x == cows[i].dstX) {
                        if (random(0, 1)) {
                            cows[i].walkState = WalkState::Moving;
                            cows[i].dstX = random(cows[i].r.x - MAX_TRAVEL_DISTANCE, cows[i].r.x + MAX_TRAVEL_DISTANCE);
                        }
                        else {
                            cows[i].walkState = WalkState::Waiting;
                            cows[i].waitClock.restart();
                        }
                    }
                }
#endif
#if 1
                if (cows[i].turns > 0) {
                    --cows[i].turns;
                    if (cows[i].isLeft) {
                        cows[i].r.x += -deltaTime * GAME_SPEED;
                    }
                    else {
                        cows[i].r.x += deltaTime * GAME_SPEED;
                    }
                }
                else {
                    if (random(0, 1)) {
                        cows[i].r.x += deltaTime * GAME_SPEED;
                        cows[i].isLeft = false;
                    }
                    else {
                        cows[i].r.x += -deltaTime * GAME_SPEED;
                        cows[i].isLeft = true;
                    }
                    cows[i].turns = 5;
                }
#endif
            }
        }
        for (int i = 0; i < enemies.size(); ++i) {
            enemies[i].r.x += -player.dx * deltaTime * GAME_SPEED;
            if (player.dx == 0) {
                if (random(0, 1)) {
                    enemies[i].r.x += deltaTime * GAME_SPEED;
                }
                else {
                    enemies[i].r.x += -deltaTime * GAME_SPEED;
                }
            }
        }
        for (int i = 0; i < hearts.size(); ++i) {
            hearts[i].r.x += -player.dx * deltaTime * GAME_SPEED;
            if (player.dx == 0) {
                if (random(0, 1)) {
                    hearts[i].r.x += deltaTime * GAME_SPEED;
                }
                else {
                    hearts[i].r.x += -deltaTime * GAME_SPEED;
                }
            }
        }
        if (cowClock.getElapsedTime() > cowSpawnDelay) {
            cows.push_back(generateCollectable(player));
            cowClock.restart();
        }
        if (enemyClock.getElapsedTime() > 3000) {
            enemies.push_back(generateCollectable(player));
            enemyClock.restart();
        }
        if (heartClock.getElapsedTime() > 10000) {
            if (hearts.empty() && hearthRects.size() < maxHeartsCount) {
                hearts.push_back(Collectable());
                hearts.back().r.w = 32;
                hearts.back().r.h = 32;
                hearts.back().r.x = random(0, 1) ? -hearts.back().r.w : windowWidth;
                hearts.back().r.y = windowHeight - hearts.back().r.h;
            }
            heartClock.restart();
        }
        for (int i = 0; i < cows.size(); ++i) {
            if (SDL_HasIntersectionF(&player.r, &cows[i].r)) {
                cows[i].r.y += -deltaTime * GAME_SPEED;
                if (cows[i].r.y <= 150) {
                    cows.erase(cows.begin() + i--);
                    if (hasLaser) {
                        if (hasShield) {
                            hasShield = false;
                        }
                        else {
                            hearthRects.pop_back();
                            if (hearthRects.empty()) {
                                scoreText.setText(renderer, robotoF, "0", {});
                                for (int i = 0; i < maxHeartsCount; ++i) {
                                    addHeart(hearthRects);
                                }
                            }
                        }
                        Mix_PlayChannel(-1, hitS, 0);
                    }
                    else {
                        scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) + 1, {});
                        Mix_PlayChannel(-1, pickupS, 0);
                    }
                }
            }
            else {
                cows[i].r.y += deltaTime * GAME_SPEED;
                if (cows[i].r.y + cows[i].r.h > windowHeight) {
                    cows[i].r.y = windowHeight - cows[i].r.h;
                }
            }
        }
        for (int i = 0; i < enemies.size(); ++i) {
            if (SDL_HasIntersectionF(&player.r, &enemies[i].r)) {
                enemies[i].r.y += -deltaTime * GAME_SPEED;
                if (enemies[i].r.y <= 150) {
                    enemies.erase(enemies.begin() + i--);
                    if (hasLaser) {
                        scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) + 1, {});
                        Mix_PlayChannel(-1, pickupS, 0);
                    }
                    else {
                        if (hasShield) {
                            hasShield = false;
                        }
                        else {
                            hearthRects.pop_back();
                            if (hearthRects.empty()) {
                                scoreText.setText(renderer, robotoF, "0", {});
                                for (int i = 0; i < maxHeartsCount; ++i) {
                                    addHeart(hearthRects);
                                }
                            }
                        }
                        Mix_PlayChannel(-1, hitS, 0);
                    }
                }
            }
            else {
                enemies[i].r.y += deltaTime * GAME_SPEED;
                if (enemies[i].r.y + enemies[i].r.h > windowHeight) {
                    enemies[i].r.y = windowHeight - enemies[i].r.h;
                }
            }
        }
        for (int i = 0; i < hearts.size(); ++i) {
            if (SDL_HasIntersectionF(&player.r, &hearts[i].r)) {
                hearts[i].r.y += -deltaTime * GAME_SPEED;
                if (hearts[i].r.y <= 150) {
                    hearts.erase(hearts.begin() + i--);
                    Mix_PlayChannel(-1, pickupS, 0);
                    addHeart(hearthRects);
                }
            }
            else {
                hearts[i].r.y += deltaTime * GAME_SPEED;
                if (hearts[i].r.y + hearts[i].r.h > windowHeight) {
                    hearts[i].r.y = windowHeight - hearts[i].r.h;
                }
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);
        SDL_RenderCopyF(renderer, backgroundT, 0, 0);
        SDL_RenderCopyF(renderer, backgroundT, 0, &backgroundDstR);
        SDL_RenderCopyExF(renderer, backgroundT, 0, &backgroundDstR2, 0, 0, SDL_FLIP_HORIZONTAL);
        SDL_RenderCopyExF(renderer, backgroundT, 0, &backgroundDstR3, 0, 0, SDL_FLIP_HORIZONTAL);
        if (backgroundDstR.x + backgroundDstR.w < 0) {
            backgroundDstR.x = backgroundDstR3.x + backgroundDstR3.w;
        }
        if (backgroundDstR.x > windowWidth) {
            backgroundDstR.x = backgroundDstR2.x - backgroundDstR.w;
        }
        if (backgroundDstR3.x + backgroundDstR3.w < 0) {
            backgroundDstR3.x = backgroundDstR.x + backgroundDstR.w;
        }
        if (backgroundDstR3.x > windowWidth) {
            backgroundDstR3.x = backgroundDstR.x + backgroundDstR3.w;
        }
        if (backgroundDstR2.x > windowWidth) {
            backgroundDstR2.x = backgroundDstR.x - backgroundDstR2.w;
        }
        if (backgroundDstR2.x + backgroundDstR2.w < 0) {
            backgroundDstR2.x = backgroundDstR.x + backgroundDstR.w;
        }
        SDL_RenderCopyF(renderer, shopT, 0, &shopR);
        if (hasShield) {
            SDL_RenderCopyF(renderer, bubbleT, 0, &player.r);
        }
        if (hasLaser) {
            SDL_RenderCopyF(renderer, ufoWithRedLaserT, 0, &player.r);
        }
        else {
            SDL_RenderCopyF(renderer, ufoT, 0, &player.r);
        }
        for (int i = 0; i < cows.size(); ++i) {
            if (cows[i].isLeft) {
                SDL_RenderCopyExF(renderer, cowT, 0, &cows[i].r, 0, 0, SDL_FLIP_HORIZONTAL);
            }
            else {
                SDL_RenderCopyExF(renderer, cowT, 0, &cows[i].r, 0, 0, SDL_FLIP_NONE);
            }
        }
        for (int i = 0; i < enemies.size(); ++i) {
            if (enemies[i].enemyType == EnemyType::Landing) {
                SDL_RenderCopyExF(renderer, alienT, 0, &enemies[i].r, 0, 0, SDL_FLIP_NONE);
            }
            else if (enemies[i].enemyType == EnemyType::Flying) {
                SDL_RenderCopyExF(renderer, flyingEnemyTextures[enemies[i].currTexture], 0, &enemies[i].r, 0, 0, SDL_FLIP_NONE);
                ++enemies[i].currTexture;
                if (enemies[i].currTexture == FLYING_ENEMY_LAST_FRAME_PLUS_ONE) {
                    enemies[i].currTexture = 0;
                }
            }
        }
        scoreText.draw(renderer);
        for (int i = 0; i < hearthRects.size(); ++i) {
            SDL_RenderCopyF(renderer, heartT, 0, &hearthRects[i]);
        }
        for (int i = 0; i < hearts.size(); ++i) {
            SDL_RenderCopyF(renderer, heartT, 0, &hearts[i].r);
        }
        if (hasSounds) {
            SDL_RenderCopyF(renderer, unmutedT, 0, &soundBtnR);
        }
        else {
            SDL_RenderCopyF(renderer, mutedT, 0, &soundBtnR);
        }
        if (boughtUfos) {
            SDL_RenderCopyF(renderer, manyUfosT, 0, &ufosBtnR);
        }
        fooText.draw(renderer);
        creditsText.draw(renderer);
#ifdef __ANDROID__
        SDL_RenderCopyF(renderer, goLeftT, 0, &goLeftR);
        SDL_RenderCopyF(renderer, goRightT, 0, &goRightR);
        if (laserBought) {
            if (hasLaser) {
                SDL_RenderCopyF(renderer, yellowLaserT, 0, &laserBtnR);
            }
            else {
                SDL_RenderCopyF(renderer, redLaserT, 0, &laserBtnR);
            }
        }
#endif
        SDL_RenderPresent(renderer);
    }
    else if (state == State::Shop) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
                saveData();
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                keys[event.key.keysym.scancode] = true;
            }
            if (event.type == SDL_KEYUP) {
                keys[event.key.keysym.scancode] = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
                if (SDL_PointInFRect(&mousePos, &backArrowR)) {
                    state = State::Gameplay;
                }
                if (SDL_PointInFRect(&mousePos, &buyHeartBtnR) && maxHeartsCount < 5) {
                    if (std::stoi(scoreText.text) >= std::stoi(buyHeartPriceText.text)) {
                        Mix_PlayChannel(-1, powerupS, 0);
                        scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) - std::stoi(buyHeartPriceText.text), {});
                        addHeart(hearthRects);
                        ++maxHeartsCount;
                        if (maxHeartsCount == 2) {
                            buyHeartPriceText.setText(renderer, robotoF, "10", {});
                        }
                        else if (maxHeartsCount == 3) {
                            buyHeartPriceText.setText(renderer, robotoF, "120", {});
                        }
                        else if (maxHeartsCount == 4) {
                            buyHeartPriceText.setText(renderer, robotoF, "200", {});
                        }
                    }
                }
                if (SDL_PointInFRect(&mousePos, &buyLaserBtnR) && !laserBought) {
                    if (std::stoi(scoreText.text) >= std::stoi(buyLaserPriceText.text)) {
                        Mix_PlayChannel(-1, powerupS, 0);
                        scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) - std::stoi(buyLaserPriceText.text), {});
                        laserBought = true;
                    }
                }
                if (SDL_PointInFRect(&mousePos, &buyMoreCowsBtnR)) {
                    if (std::stoi(scoreText.text) >= std::stoi(buyMoreCowsPriceText.text)) {
                        Mix_PlayChannel(-1, powerupS, 0);
                        scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) - std::stoi(buyMoreCowsPriceText.text), {});
                        cowSpawnDelay = 1500;
                    }
                }
                if (SDL_PointInFRect(&mousePos, &buyShieldBtnR) && !hasShield) {
                    if (std::stoi(scoreText.text) >= std::stoi(buyShieldPriceText.text)) {
                        Mix_PlayChannel(-1, powerupS, 0);
                        scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) - std::stoi(buyShieldPriceText.text), {});
                        hasShield = true;
                    }
                }
                if (SDL_PointInFRect(&mousePos, &buyUfosBtnR) && !boughtUfos) {
                    if (std::stoi(scoreText.text) >= std::stoi(buyUfosPriceText.text)) {
                        Mix_PlayChannel(-1, powerupS, 0);
                        scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) - std::stoi(buyUfosPriceText.text), {});
                        boughtUfos = true;
                    }
                }
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                buttons[event.button.button] = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                float scaleX, scaleY;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                mousePos.x = event.motion.x / scaleX;
                mousePos.y = event.motion.y / scaleY;
                realMousePos.x = event.motion.x;
                realMousePos.y = event.motion.y;
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);
        if (maxHeartsCount < 5) {
            SDL_RenderCopyF(renderer, heartT, 0, &buyHeartR);
            buyHeartPriceText.draw(renderer);
            SDL_RenderCopyF(renderer, buyT, 0, &buyHeartBtnR);
        }
        if (!laserBought) {
            SDL_RenderCopyF(renderer, redLaserT, 0, &buyLaserR);
            buyLaserPriceText.draw(renderer);
            SDL_RenderCopyF(renderer, buyT, 0, &buyLaserBtnR);
        }
        if (cowSpawnDelay != 1500) {
            SDL_RenderCopyF(renderer, moreCowsUpgradeT, 0, &buyMoreCowsR);
            buyMoreCowsPriceText.draw(renderer);
            SDL_RenderCopyF(renderer, buyT, 0, &buyMoreCowsBtnR);
        }
        if (!hasShield) {
            SDL_RenderCopyF(renderer, shieldT, 0, &buyShieldR);
            buyShieldPriceText.draw(renderer);
            SDL_RenderCopyF(renderer, buyT, 0, &buyShieldBtnR);
        }
        if (!boughtUfos) {
            SDL_RenderCopyF(renderer, manyUfosT, 0, &buyUfosR);
            buyUfosPriceText.draw(renderer);
            SDL_RenderCopyF(renderer, buyT, 0, &buyUfosBtnR);
        }
        SDL_RenderCopyF(renderer, backArrowT, 0, &backArrowR);
        scoreText.draw(renderer);
        SDL_RenderPresent(renderer);
    }
    else if (state == State::Ufos) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
                saveData();
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                keys[event.key.keysym.scancode] = true;
            }
            if (event.type == SDL_KEYUP) {
                keys[event.key.keysym.scancode] = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
                if (SDL_PointInFRect(&mousePos, &backArrowR)) {
                    state = State::Gameplay;
                }
                if (SDL_PointInFRect(&mousePos, &addUfoBtnR)) {
                    if (std::stoi(scoreText.text) >= std::stoi(addUfoPriceText.text)) {
                        scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) - std::stoi(addUfoPriceText.text), {});
                        Mix_PlayChannel(-1, powerupS, 0);
                        ufosCountText.setText(renderer, robotoF, std::stoi(ufosCountText.text) + 1, {});
                        scorePerSecondText.setText(renderer, robotoF, ufosCountText.text + "/s", {});
                    }
                }
                if (SDL_PointInFRect(&mousePos, &collectAllBtnR)) {
                    scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) + std::stoi(collectedScoreText.text), {});
                    collectedScoreText.setText(renderer, robotoF, "0", {});
                }
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                buttons[event.button.button] = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                float scaleX, scaleY;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                mousePos.x = event.motion.x / scaleX;
                mousePos.y = event.motion.y / scaleY;
                realMousePos.x = event.motion.x;
                realMousePos.y = event.motion.y;
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);
        scorePerSecondText.draw(renderer);
        collectedScoreText.draw(renderer);
        SDL_RenderCopyF(renderer, addT, 0, &addUfoBtnR);
        addUfoPriceText.draw(renderer);
        SDL_RenderCopyF(renderer, collectAllT, 0, &collectAllBtnR);
        SDL_RenderCopyF(renderer, backArrowT, 0, &backArrowR);
        ufosCountText.draw(renderer);
        SDL_FRect ufoR;
        ufoR.w = 32;
        ufoR.h = 32;
        ufoR.x = ufosCountText.dstR.x + ufosCountText.dstR.w;
        ufoR.y = ufosCountText.dstR.y;
        SDL_RenderCopyF(renderer, ufoT, 0, &ufoR);
        SDL_RenderPresent(renderer);
    }
    else if (state == State::Credits) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
                saveData();
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                keys[event.key.keysym.scancode] = true;
            }
            if (event.type == SDL_KEYUP) {
                keys[event.key.keysym.scancode] = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
                if (SDL_PointInFRect(&mousePos, &backArrowR)) {
                    state = State::Gameplay;
                }
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                buttons[event.button.button] = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                float scaleX, scaleY;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                mousePos.x = event.motion.x / scaleX;
                mousePos.y = event.motion.y / scaleY;
                realMousePos.x = event.motion.x;
                realMousePos.y = event.motion.y;
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);
        for (int i = 0; i < credits.size(); ++i) {
            credits[i].draw(renderer);
        }
        SDL_RenderCopyF(renderer, backArrowT, 0, &backArrowR);
        creditsInfoText.draw(renderer);
        authorText.draw(renderer);
        authorValueText.draw(renderer);
        graphicsText.draw(renderer);
        graphicsValueText.draw(renderer);
        otherText.draw(renderer);
        SDL_RenderPresent(renderer);
    }
    saveData();
}

int main(int argc, char* argv[])
{
    std::srand(std::time(0));
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    SDL_LogSetOutputFunction(logOutputCallback, 0);
    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    SDL_GetMouseState(&mousePos.x, &mousePos.y);
    window = SDL_CreateWindow("UfoWorld", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    robotoF = TTF_OpenFont("res/roboto.ttf", 72);
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_RenderSetScale(renderer, w / (float)windowWidth, h / (float)windowHeight);
    SDL_AddEventWatch(eventWatch, 0);
    prefPath = SDL_GetPrefPath("NextCodeApps", "UfoWorld");
    readData(scoreText, maxHeartsCount, laserBought, cowSpawnDelay);
    cowT = IMG_LoadTexture(renderer, "res/cow.png");
    ufoT = IMG_LoadTexture(renderer, "res/ufo.png");
    heartT = IMG_LoadTexture(renderer, "res/heart.png");
    shopT = IMG_LoadTexture(renderer, "res/shop.png");
    buyT = IMG_LoadTexture(renderer, "res/buy.png");
    backArrowT = IMG_LoadTexture(renderer, "res/backArrow.png");
    backgroundT = IMG_LoadTexture(renderer, "res/background.png");
    mutedT = IMG_LoadTexture(renderer, "res/muted.png");
    unmutedT = IMG_LoadTexture(renderer, "res/unmuted.png");
    alienT = IMG_LoadTexture(renderer, "res/enemy.png");
    ufoWithRedLaserT = IMG_LoadTexture(renderer, "res/ufoWithRedLaser.png");
    redLaserT = IMG_LoadTexture(renderer, "res/redLaser.png");
    moreCowsUpgradeT = IMG_LoadTexture(renderer, "res/moreCowsUpgrade.png");
    manyUfosT = IMG_LoadTexture(renderer, "res/manyUfos.png");
    addT = IMG_LoadTexture(renderer, "res/plus.png");
    leftArrowT = IMG_LoadTexture(renderer, "res/leftArrow.png");
    collectAllT = IMG_LoadTexture(renderer, "res/collectAll.png");
    shieldT = IMG_LoadTexture(renderer, "res/shield.png");
    bubbleT = IMG_LoadTexture(renderer, "res/bubble.png");
    for (int i = 1; i < 9; ++i) {
        flyingEnemyTextures.push_back(IMG_LoadTexture(renderer, ("res/FlyingMonster/png/frame-" + std::to_string(i) + ".png").c_str()));
    }
    goLeftT = IMG_LoadTexture(renderer, "res/goLeft.png");
    goRightT = IMG_LoadTexture(renderer, "res/goRight.png");
    yellowLaserT = IMG_LoadTexture(renderer, "res/yellowLaser.png");
    music = Mix_LoadMUS("res/music.ogg");
    pickupS = Mix_LoadWAV("res/pickup.wav");
    powerupS = Mix_LoadWAV("res/powerup.wav");
    hitS = Mix_LoadWAV("res/hit.wav");
    laserSwitchS = Mix_LoadWAV("res/laserSwitch.wav");
    Mix_PlayMusic(music, -1);
    cows.push_back(generateCollectable(player));
    scoreText.dstR.w = 20;
    scoreText.dstR.h = 35;
    scoreText.dstR.x = windowWidth / 2 - scoreText.dstR.w / 2;
    scoreText.dstR.y = 5;
    player.r.w = 100;
    player.r.h = windowHeight;
    player.r.x = windowWidth / 2 - player.r.w / 2;
    player.r.y = scoreText.dstR.y + scoreText.dstR.h;
    shopR.w = 48;
    shopR.h = 48;
    shopR.x = windowWidth - shopR.w;
    shopR.y = 0;
    backArrowR.w = 32;
    backArrowR.h = 32;
    backArrowR.x = 0;
    backArrowR.y = 0;
    buyHeartR.w = 64;
    buyHeartR.h = 64;
    buyHeartR.x = 5;
    buyHeartPriceText.setText(renderer, robotoF, "5", {});
    buyHeartPriceText.dstR.w = 25;
    buyHeartPriceText.dstR.h = 20;
    buyHeartPriceText.dstR.x = buyHeartR.x + buyHeartR.w / 2 - buyHeartPriceText.dstR.w / 2;
    buyHeartPriceText.dstR.y = backArrowR.y + backArrowR.h;
    buyHeartR.y = buyHeartPriceText.dstR.y + buyHeartPriceText.dstR.h + 5;
    buyHeartBtnR.w = buyHeartR.w;
    buyHeartBtnR.h = 38;
    buyHeartBtnR.x = buyHeartR.x;
    buyHeartBtnR.y = buyHeartR.y + buyHeartR.h + 5;
    buyLaserR.w = 64;
    buyLaserR.h = 64;
    buyLaserR.x = buyHeartR.x + buyHeartR.w + 15;
    buyLaserPriceText.setText(renderer, robotoF, "40", {});
    buyLaserPriceText.dstR.w = 50;
    buyLaserPriceText.dstR.h = 20;
    buyLaserPriceText.dstR.x = buyLaserR.x + buyLaserR.w / 2 - buyLaserPriceText.dstR.w / 2;
    buyLaserPriceText.dstR.y = backArrowR.y + backArrowR.h;
    buyLaserR.y = buyLaserPriceText.dstR.y + buyLaserPriceText.dstR.h + 5;
    buyLaserBtnR.w = buyLaserR.w;
    buyLaserBtnR.h = 38;
    buyLaserBtnR.x = buyLaserR.x;
    buyLaserBtnR.y = buyLaserR.y + buyLaserR.h + 5;
    buyMoreCowsR.w = 64;
    buyMoreCowsR.h = 64;
    buyMoreCowsR.x = buyLaserR.x + buyLaserR.w + 15;
    buyMoreCowsPriceText.setText(renderer, robotoF, "20", {});
    buyMoreCowsPriceText.dstR.w = 50;
    buyMoreCowsPriceText.dstR.h = 20;
    buyMoreCowsPriceText.dstR.x = buyMoreCowsR.x + buyMoreCowsR.w / 2 - buyMoreCowsPriceText.dstR.w / 2;
    buyMoreCowsPriceText.dstR.y = backArrowR.y + backArrowR.h;
    buyMoreCowsR.y = buyMoreCowsPriceText.dstR.y + buyMoreCowsPriceText.dstR.h + 5;
    buyMoreCowsBtnR.w = buyMoreCowsR.w;
    buyMoreCowsBtnR.h = 38;
    buyMoreCowsBtnR.x = buyMoreCowsR.x;
    buyMoreCowsBtnR.y = buyMoreCowsR.y + buyMoreCowsR.h + 5;
    buyShieldR.w = 64;
    buyShieldR.h = 64;
    buyShieldR.x = buyHeartR.x;
    buyShieldPriceText.setText(renderer, robotoF, "20", {});
    buyShieldPriceText.dstR.w = 50;
    buyShieldPriceText.dstR.h = 20;
    buyShieldPriceText.dstR.x = buyShieldR.x + buyShieldR.w / 2 - buyShieldPriceText.dstR.w / 2;
    buyShieldPriceText.dstR.y = buyHeartBtnR.y + buyHeartBtnR.h;
    buyShieldR.y = buyShieldPriceText.dstR.y + buyShieldPriceText.dstR.h + 5;
    buyShieldBtnR.w = buyShieldR.w;
    buyShieldBtnR.h = 38;
    buyShieldBtnR.x = buyShieldR.x;
    buyShieldBtnR.y = buyShieldR.y + buyShieldR.h + 5;
    buyUfosR.w = 64;
    buyUfosR.h = 64;
    buyUfosR.x = buyLaserR.x;
    buyUfosPriceText.setText(renderer, robotoF, "50", {});
    buyUfosPriceText.dstR.w = 50;
    buyUfosPriceText.dstR.h = 20;
    buyUfosPriceText.dstR.x = buyUfosR.x + buyUfosR.w / 2 - buyUfosPriceText.dstR.w / 2;
    buyUfosPriceText.dstR.y = buyHeartBtnR.y + buyHeartBtnR.h;
    buyUfosR.y = buyUfosPriceText.dstR.y + buyUfosPriceText.dstR.h + 5;
    buyUfosBtnR.w = buyUfosR.w;
    buyUfosBtnR.h = 38;
    buyUfosBtnR.x = buyUfosR.x;
    buyUfosBtnR.y = buyUfosR.y + buyUfosR.h + 5;
    backgroundDstR.w = windowWidth;
    backgroundDstR.h = windowHeight;
    backgroundDstR.x = 0;
    backgroundDstR.y = 0;
    backgroundDstR2.w = windowWidth;
    backgroundDstR2.h = windowHeight;
    backgroundDstR2.x = -windowWidth;
    backgroundDstR2.y = 0;
    backgroundDstR3.w = windowWidth;
    backgroundDstR3.h = windowHeight;
    backgroundDstR3.x = windowWidth;
    backgroundDstR3.y = 0;
    soundBtnR.w = 32;
    soundBtnR.h = 32;
    soundBtnR.x = windowWidth - soundBtnR.w;
    soundBtnR.y = shopR.y + shopR.h;
    scorePerSecondText.dstR.w = 100;
    scorePerSecondText.dstR.h = 30;
    scorePerSecondText.dstR.x = windowWidth / 2 - scorePerSecondText.dstR.w / 2;
    scorePerSecondText.dstR.y = 5;
    collectedScoreText.dstR.w = 50;
    collectedScoreText.dstR.h = 30;
    collectedScoreText.dstR.x = windowWidth / 2 - collectedScoreText.dstR.w / 2;
    collectedScoreText.dstR.y = scorePerSecondText.dstR.y + scorePerSecondText.dstR.h + 5;
    addUfoBtnR.w = 32;
    addUfoBtnR.h = 32;
    addUfoBtnR.x = 30;
    addUfoBtnR.y = collectedScoreText.dstR.y + collectedScoreText.dstR.h / 2 - addUfoBtnR.h / 2;
    addUfoPriceText.setText(renderer, robotoF, "50", {});
    addUfoPriceText.dstR.w = 100;
    addUfoPriceText.dstR.h = 30;
    addUfoPriceText.dstR.x = addUfoBtnR.x + addUfoBtnR.w / 2 - addUfoPriceText.dstR.w / 2;
    addUfoPriceText.dstR.y = addUfoBtnR.y + addUfoBtnR.h + 3;
    collectAllBtnR.w = 100;
    collectAllBtnR.h = 60;
    collectAllBtnR.x = windowWidth - collectAllBtnR.w;
    collectAllBtnR.y = collectedScoreText.dstR.y + collectedScoreText.dstR.h / 2;
    ufosCountText.dstR.w = 50;
    ufosCountText.dstR.h = 20;
    ufosCountText.dstR.x = windowWidth / 2 - ufosCountText.dstR.w / 2;
    ufosCountText.dstR.y = collectAllBtnR.y + collectAllBtnR.h;
    creditsText.setText(renderer, robotoF, "C", {});
    creditsText.dstR.w = 50;
    creditsText.dstR.h = 40;
    creditsText.dstR.x = windowWidth - creditsText.dstR.w;
    creditsText.dstR.y = soundBtnR.y + soundBtnR.h;
    ufosBtnR.w = 48;
    ufosBtnR.h = 48;
    ufosBtnR.x = windowWidth - ufosBtnR.w;
    ufosBtnR.y = creditsText.dstR.y + creditsText.dstR.h;
    creditsInfoText.dstR.w = 100;
    creditsInfoText.dstR.h = 20;
    creditsInfoText.dstR.x = windowWidth / 2 - creditsInfoText.dstR.w / 2;
    creditsInfoText.dstR.y = 0;
    authorText.setText(renderer, robotoF, "Author:", {});
    authorText.dstR.w = 100;
    authorText.dstR.h = 20;
    authorText.dstR.x = 0;
    authorText.dstR.y = backArrowR.y + backArrowR.h;
    authorValueText.setText(renderer, robotoF, "Huberti", {});
    authorValueText.dstR.w = 100;
    authorValueText.dstR.h = 20;
    authorValueText.dstR.x = 0;
    authorValueText.dstR.y = authorText.dstR.y + authorText.dstR.h;
    graphicsText.setText(renderer, robotoF, "Graphics:", {});
    graphicsText.dstR.w = 100;
    graphicsText.dstR.h = 20;
    graphicsText.dstR.x = 0;
    graphicsText.dstR.y = authorValueText.dstR.y + authorValueText.dstR.h;
    graphicsValueText.setText(renderer, robotoF, "MalgorzataMika", {});
    graphicsValueText.dstR.w = 100;
    graphicsValueText.dstR.h = 20;
    graphicsValueText.dstR.x = 0;
    graphicsValueText.dstR.y = graphicsText.dstR.y + graphicsText.dstR.h;
    otherText.setText(renderer, robotoF, "Other:", {});
    otherText.dstR.w = 100;
    otherText.dstR.h = 20;
    otherText.dstR.x = 0;
    otherText.dstR.y = graphicsValueText.dstR.y + graphicsValueText.dstR.h;
    credits.push_back(Text());
    credits.back().setText(renderer, robotoF, "Lima Studio", {});
    credits.back().dstR.w = 100;
    credits.back().dstR.h = 20;
    credits.back().dstR.x = 0;
    credits.back().dstR.y = otherText.dstR.y + otherText.dstR.h;
    credits.push_back(credits.back());
    credits.back().setText(renderer, robotoF, "Freepik", {});
    credits.back().dstR.y += credits.back().dstR.h;
    credits.push_back(credits.back());
    credits.back().setText(renderer, robotoF, "Nikita Golubev", {});
    credits.back().dstR.y += credits.back().dstR.h;
    credits.push_back(credits.back());
    credits.back().setText(renderer, robotoF, "Becris", {});
    credits.back().dstR.y += credits.back().dstR.h;
    credits.push_back(credits.back());
    credits.back().setText(renderer, robotoF, "surang", {});
    credits.back().dstR.y += credits.back().dstR.h;
    credits.push_back(credits.back());
    credits.back().setText(renderer, robotoF, "StubbornParakeet", {});
    credits.back().dstR.y += credits.back().dstR.h;
    credits.push_back(credits.back());
    credits.back().setText(renderer, robotoF, "Pixel perfect", {});
    credits.back().dstR.y += credits.back().dstR.h;
    credits.push_back(credits.back());
    credits.back().setText(renderer, robotoF, "Vectors Market", {});
    credits.back().dstR.y += credits.back().dstR.h;
    credits.push_back(credits.back());
    credits.back().setText(renderer, robotoF, "Roundicons", {});
    credits.back().dstR.y += credits.back().dstR.h;
    creditsInfoText.setText(renderer, robotoF, "Credits", {});
    goLeftR.w = 40;
    goLeftR.h = 40;
    goLeftR.x = windowWidth - goLeftR.w * 3;
    goLeftR.y = windowHeight - 50;
    goRightR = goLeftR;
    goRightR.x = windowWidth - goRightR.w;
    laserBtnR.w = 32;
    laserBtnR.h = 32;
    laserBtnR.x = windowWidth - laserBtnR.w;
    laserBtnR.y = ufosBtnR.y + ufosBtnR.h;
    globalClock.restart();
    cowClock.restart();
    enemyClock.restart();
    heartClock.restart();
    cowsClock.restart();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (running) {
        mainLoop();
    }
#endif
    // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
    saveData();
    return 0;
}