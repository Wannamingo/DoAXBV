#include "input_host_win32.h"
#include <SDL3/SDL.h>
#include <jni.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
namespace {
std::array<SDL_Gamepad *,RECOMP_INPUT_PORT_COUNT> pads{};
RecompInputGamepad touch{};
uint16_t pending_digital;
uint8_t pending_analog;
std::mutex touch_mutex;
bool initialized,face_swap;
int16_t up_axis(SDL_Gamepad *pad,SDL_GamepadAxis axis)
{
    int value=SDL_GetGamepadAxis(pad,axis);
    return (int16_t)(value==-32768 ? 32767 : -value);
}
void update_pads()
{
    if (!initialized) { initialized=true;const char *swap=std::getenv("RECOMP_FACE_SWAP");face_swap=swap && !std::strcmp(swap,"1"); SDL_Init(SDL_INIT_GAMEPAD); }
    SDL_UpdateGamepads();
    for (auto &pad:pads) if (pad && !SDL_GamepadConnected(pad)) { SDL_CloseGamepad(pad); pad=nullptr; }
    int count=0; SDL_JoystickID *ids=SDL_GetGamepads(&count);
    for (int i=0;ids && i<count;++i) {
        bool opened=false; SDL_Gamepad **available=nullptr;
        for (auto &pad:pads) { if (pad && SDL_GetGamepadID(pad)==ids[i]) opened=true; else if (!pad && !available) available=&pad; }
        if (!opened && available){*available=SDL_OpenGamepad(ids[i]);if(*available)std::fprintf(stderr,"Android gamepad port=%u name=%s face_swap=%u\n",unsigned(available-pads.data()),SDL_GetGamepadName(*available),unsigned(face_swap));}
    }
    SDL_free(ids);
}
}
extern "C" JNIEXPORT void JNICALL Java_org_doaxbv_port_GameActivity_nativePad(
    JNIEnv *,jclass,jint digital,jint analog,jfloat lx,jfloat ly,jfloat rx,jfloat ry)
{
    std::lock_guard<std::mutex> lock(touch_mutex);
    pending_digital|=(uint16_t)digital & ~touch.buttons;
    for(unsigned i=0;i<8;++i)if((analog&(1<<i)) && !touch.analog_buttons[i])pending_analog|=1<<i;
    touch={}; touch.buttons=(uint16_t)digital;
    for(unsigned i=0;i<8;++i) touch.analog_buttons[i]=(analog&(1<<i)) ? 255 : 0;
    touch.thumb_lx=(int16_t)(std::clamp(lx,-1.0f,1.0f)*32767);
    touch.thumb_ly=(int16_t)(std::clamp(ly,-1.0f,1.0f)*32767);
    touch.thumb_rx=(int16_t)(std::clamp(rx,-1.0f,1.0f)*32767);
    touch.thumb_ry=(int16_t)(std::clamp(ry,-1.0f,1.0f)*32767);
}
extern "C" JNIEXPORT void JNICALL Java_org_doaxbv_port_GameActivity_nativePadReset(JNIEnv *,jclass)
{std::lock_guard<std::mutex> lock(touch_mutex);touch={};pending_digital=0;pending_analog=0;}
extern "C" JNIEXPORT void JNICALL Java_org_doaxbv_port_GameActivity_nativeRequestExit(JNIEnv *,jclass)
{SDL_Event event{};event.type=SDL_EVENT_QUIT;SDL_PushEvent(&event);}
extern "C" float recomp_android_presenter_fps(void);
extern "C" JNIEXPORT jfloat JNICALL Java_org_doaxbv_port_GameActivity_nativeGetFps(JNIEnv *,jclass)
{return recomp_android_presenter_fps();}
bool recomp_input_host_sample(uint32_t port,RecompInputGamepad *result)
{
    if (!result || port>=pads.size()) return false;
    *result={}; update_pads(); SDL_Gamepad *pad=pads[port];
    if (pad) {
        const SDL_GamepadButton digital[]={SDL_GAMEPAD_BUTTON_DPAD_UP,SDL_GAMEPAD_BUTTON_DPAD_DOWN,
            SDL_GAMEPAD_BUTTON_DPAD_LEFT,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,SDL_GAMEPAD_BUTTON_START,
            SDL_GAMEPAD_BUTTON_BACK,SDL_GAMEPAD_BUTTON_LEFT_STICK,SDL_GAMEPAD_BUTTON_RIGHT_STICK};
        for(unsigned i=0;i<8;++i) if(SDL_GetGamepadButton(pad,digital[i])) result->buttons|=1u<<i;
        const SDL_GamepadButton face[]={SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_WEST,
            SDL_GAMEPAD_BUTTON_NORTH,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER};
        for(unsigned i=0;i<6;++i) result->analog_buttons[i]=SDL_GetGamepadButton(pad,face[i]) ? 255 : 0;
        // Apply the optional letter-layout correction to the physical pad;
        // touch labels always keep their named Xbox action.
        if(face_swap){std::swap(result->analog_buttons[0],result->analog_buttons[1]);std::swap(result->analog_buttons[2],result->analog_buttons[3]);}
        result->analog_buttons[6]=(uint8_t)(std::max(0,(int)SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFT_TRIGGER))>>7);
        result->analog_buttons[7]=(uint8_t)(std::max(0,(int)SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))>>7);
        result->thumb_lx=SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTX);result->thumb_ly=up_axis(pad,SDL_GAMEPAD_AXIS_LEFTY);
        result->thumb_rx=SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_RIGHTX);result->thumb_ry=up_axis(pad,SDL_GAMEPAD_AXIS_RIGHTY);
    }
    if (port==0) {
        std::lock_guard<std::mutex> lock(touch_mutex);
        result->buttons|=touch.buttons|pending_digital;
        for(unsigned i=0;i<8;++i) result->analog_buttons[i]=std::max(result->analog_buttons[i],uint8_t((pending_analog&(1<<i)) ? 255:touch.analog_buttons[i]));
        pending_digital=0;pending_analog=0;
        if(touch.thumb_lx || touch.thumb_ly) { result->thumb_lx=touch.thumb_lx;result->thumb_ly=touch.thumb_ly; }
        if(touch.thumb_rx || touch.thumb_ry) { result->thumb_rx=touch.thumb_rx;result->thumb_ry=touch.thumb_ry; }
    }
    return port==0 || pad;
}
void recomp_input_host_set_vibration(uint32_t port,uint16_t left,uint16_t right)
{ if(port<pads.size() && pads[port]) SDL_RumbleGamepad(pads[port],left,right,1000); }
void recomp_input_host_stop_vibration(void)
{ for(unsigned i=0;i<pads.size();++i) recomp_input_host_set_vibration(i,0,0); }
