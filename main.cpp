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

struct Cow {
    SDL_FRect r{};
};

enum class State {
    Gameplay,
    Shop,
};

SDL_Texture* ufoT;
SDL_Texture* cowT;
SDL_Texture* heartT;
SDL_Texture* shopT;
SDL_Texture* buyT;
SDL_Texture* backArrowT;
SDL_Texture* backgroundT;
Entity player;
Clock globalClock;
std::vector<Cow> cows;
Clock cowClock;
Text scoreText;
std::vector<SDL_FRect> hearthRects;
SDL_FRect shopR;
State state = State::Gameplay;
Text buyHeartPriceText;
SDL_FRect buyHeartR;
SDL_FRect buyHeartBtnR;
SDL_FRect backArrowR;
SDL_FRect backgroundDstR;
SDL_FRect backgroundDstR2;
SDL_FRect backgroundDstR3;

Cow generateCow(Entity player)
{
    Cow cow;
    do {
        cow.r.w = 32;
        cow.r.h = 32;
        cow.r.x = random(0, 1) ? -cow.r.w : windowWidth;
        cow.r.y = windowHeight - cow.r.h;
    } while (SDL_HasIntersectionF(&cow.r, &player.r));
    return cow;
}

void mainLoop()
{
    float deltaTime = globalClock.restart();
    if (state == State::Gameplay) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
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
                if (SDL_PointInFRect(&mousePos, &shopR)) {
                    state = State::Shop;
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
        player.dx = 0;
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
                if (random(0, 1)) {
                    cows[i].r.x += deltaTime * GAME_SPEED;
                }
                else {
                    cows[i].r.x += -deltaTime * GAME_SPEED;
                }
            }
        }
        if (cowClock.getElapsedTime() > 3000) {
            cows.push_back(generateCow(player));
            cowClock.restart();
        }
        for (int i = 0; i < cows.size(); ++i) {
            if (SDL_HasIntersectionF(&player.r, &cows[i].r)) {
                cows[i].r.y += -deltaTime * GAME_SPEED;
                if (cows[i].r.y <= 150) {
                    scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) + 1, {});
                    cows.erase(cows.begin() + i--);
                }
            }
            else {
                cows[i].r.y += deltaTime * GAME_SPEED;
                if (cows[i].r.y + cows[i].r.h > windowHeight) {
                    cows[i].r.y = windowHeight - cows[i].r.h;
                }
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);
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
        SDL_RenderCopyF(renderer, ufoT, 0, &player.r);
        for (int i = 0; i < cows.size(); ++i) {
            SDL_RenderCopyF(renderer, cowT, 0, &cows[i].r);
        }
        scoreText.draw(renderer);
        for (int i = 0; i < hearthRects.size(); ++i) {
            SDL_RenderCopyF(renderer, heartT, 0, &hearthRects[i]);
        }
        SDL_RenderPresent(renderer);
    }
    else if (state == State::Shop) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
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
                if (SDL_PointInFRect(&mousePos, &buyHeartBtnR) && hearthRects.size() < 5) {
                    if (std::stoi(scoreText.text) >= std::stoi(buyHeartPriceText.text)) {
                        scoreText.setText(renderer, robotoF, std::stoi(scoreText.text) - std::stoi(buyHeartPriceText.text), {});
                        hearthRects.push_back(SDL_FRect());
                        hearthRects.back() = hearthRects[hearthRects.size() - 2];
                        hearthRects.back().x = hearthRects[hearthRects.size() - 2].x + hearthRects[hearthRects.size() - 2].w + 5;
                        if (hearthRects.size() > 3) {
                            if (hearthRects.size() == 4) {
                                hearthRects.back().x = hearthRects.front().x;
                            }
                            hearthRects.back().y = hearthRects.front().x + hearthRects.front().h;
                        }
                        buyHeartPriceText.setText(renderer, robotoF, std::stoi(buyHeartPriceText.text) + 100, {});
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
        if (hearthRects.size() < 5) {
            SDL_RenderCopyF(renderer, heartT, 0, &buyHeartR);
            buyHeartPriceText.draw(renderer);
            SDL_RenderCopyF(renderer, buyT, 0, &buyHeartBtnR);
        }
        SDL_RenderCopyF(renderer, backArrowT, 0, &backArrowR);
        scoreText.draw(renderer);
        SDL_RenderPresent(renderer);
    }
}

int main(int argc, char* argv[])
{
    std::srand(std::time(0));
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    SDL_LogSetOutputFunction(logOutputCallback, 0);
    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();
    SDL_GetMouseState(&mousePos.x, &mousePos.y);
    window = SDL_CreateWindow("UfoWorld", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    robotoF = TTF_OpenFont("res/roboto.ttf", 72);
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_RenderSetScale(renderer, w / (float)windowWidth, h / (float)windowHeight);
    SDL_AddEventWatch(eventWatch, 0);
    cowT = IMG_LoadTexture(renderer, "res/cow.png");
    ufoT = IMG_LoadTexture(renderer, "res/ufo.png");
    heartT = IMG_LoadTexture(renderer, "res/heart.png");
    shopT = IMG_LoadTexture(renderer, "res/shop.png");
    buyT = IMG_LoadTexture(renderer, "res/buy.png");
    backArrowT = IMG_LoadTexture(renderer, "res/backArrow.png");
    backgroundT = IMG_LoadTexture(renderer, "res/background.png");
    player.r.w = 100;
    player.r.h = windowHeight;
    player.r.x = windowWidth / 2 - player.r.w / 2;
    player.r.y = 15;
    cows.push_back(generateCow(player));
    scoreText.setText(renderer, robotoF, "0", {});
    scoreText.dstR.w = 20;
    scoreText.dstR.h = 35;
    scoreText.dstR.x = windowWidth / 2 - scoreText.dstR.w / 2;
    scoreText.dstR.y = 5;
    hearthRects.push_back(SDL_FRect());
    hearthRects.back().w = 32;
    hearthRects.back().h = 32;
    hearthRects.back().x = 0;
    hearthRects.back().y = 0;
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
    buyHeartPriceText.setText(renderer, robotoF, "200", {});
    buyHeartPriceText.dstR.w = 50;
    buyHeartPriceText.dstR.h = 20;
    buyHeartPriceText.dstR.x = buyHeartR.x + buyHeartR.w / 2 - buyHeartPriceText.dstR.w / 2;
    buyHeartPriceText.dstR.y = backArrowR.y + backArrowR.h;
    buyHeartR.y = buyHeartPriceText.dstR.y + buyHeartPriceText.dstR.h + 5;
    buyHeartBtnR.w = buyHeartR.w;
    buyHeartBtnR.h = 38;
    buyHeartBtnR.x = buyHeartR.x;
    buyHeartBtnR.y = buyHeartR.y + buyHeartR.h + 5;
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
    globalClock.restart();
    cowClock.restart();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (running) {
        mainLoop();
    }
#endif
    // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
    return 0;
}