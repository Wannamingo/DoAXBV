#include "audio_output.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <atomic>
namespace {
struct Voice { SDL_AudioStream *stream=nullptr; uint32_t rate=0, channels=0, bits=0; bool submitted=false; };
std::array<Voice,256> voices;
SDL_AudioDeviceID device;
SDL_AudioSpec output;
float master = 1.0f;
bool initialized;
std::atomic<uint64_t> mix_samples{},over_range{},empty_streams{};
std::atomic<unsigned> mix_peak{};
std::atomic<uint64_t> mix_max_gap_ns{},mix_late_callbacks{},rate_resets{},dropped_packets{};
std::atomic<bool> background{},flush_on_resume{},reset_mix_gap{};
uint64_t previous_mix_ns{},previous_mix_duration_ns{};
uint64_t last_report;
bool SDLCALL lifecycle(void *,SDL_Event *event)
{
    if(event->type==SDL_EVENT_WILL_ENTER_BACKGROUND || event->type==SDL_EVENT_DID_ENTER_BACKGROUND)background.store(true,std::memory_order_relaxed);
    if(event->type==SDL_EVENT_DID_ENTER_FOREGROUND){flush_on_resume.store(true,std::memory_order_release);reset_mix_gap.store(true,std::memory_order_release);background.store(false,std::memory_order_release);}
    return true;
}
void SDLCALL monitor_mix(void *,const SDL_AudioSpec *spec,float *samples,int bytes)
{
    if(reset_mix_gap.exchange(false,std::memory_order_acquire))previous_mix_ns=0;
    uint64_t now=SDL_GetTicksNS();
    if(previous_mix_ns){uint64_t gap=now-previous_mix_ns,old=mix_max_gap_ns.load(std::memory_order_relaxed);
        while(old<gap&&!mix_max_gap_ns.compare_exchange_weak(old,gap,std::memory_order_relaxed)){}
        if(previous_mix_duration_ns && gap>previous_mix_duration_ns*2)mix_late_callbacks.fetch_add(1,std::memory_order_relaxed);
    }
    previous_mix_ns=now;previous_mix_duration_ns=uint64_t(bytes)*1000000000ull/(sizeof(float)*spec->channels*spec->freq);
    uint64_t clipped=0;float peak=0;
    for(int i=0;i<bytes/(int)sizeof(float);++i){float value=std::fabs(samples[i]);peak=std::max(peak,value);clipped+=value>1.f;}
    mix_samples.fetch_add(bytes/sizeof(float),std::memory_order_relaxed);over_range.fetch_add(clipped,std::memory_order_relaxed);
    unsigned value=unsigned(std::min(peak,100.f)*1000),old=mix_peak.load(std::memory_order_relaxed);
    while(old<value&&!mix_peak.compare_exchange_weak(old,value,std::memory_order_relaxed)){}
}
}
void recomp_audio_output_initialize(void)
{
    if (initialized) return;
    initialized = true;
    if (const char *gain = std::getenv("RECOMP_AUDIO_GAIN")) master = std::clamp(std::strtof(gain,nullptr),0.0f,1.0f);
    // Compare the available Android output backends without changing guest
    // PCM or decoding. Keep AAudio as fallback if OpenSL ES cannot initialize.
    SDL_SetHint(SDL_HINT_ANDROID_LOW_LATENCY_AUDIO,"1");
    if (!(master > 0.0f))return;
    std::fprintf(stderr,"SDL audio available drivers:");for(int i=0;i<SDL_GetNumAudioDrivers();++i)std::fprintf(stderr," %s",SDL_GetAudioDriver(i));std::fprintf(stderr,"\n");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"openslES");
    if(!SDL_Init(SDL_INIT_AUDIO)){
        std::fprintf(stderr,"SDL OpenSL ES initialization: %s; trying AAudio\n",SDL_GetError());SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"AAudio");
        if(!SDL_Init(SDL_INIT_AUDIO)){std::fprintf(stderr,"SDL audio initialization: %s\n",SDL_GetError());return;}
    }
    SDL_AudioSpec requested{SDL_AUDIO_F32,2,48000};
    SDL_AddEventWatch(lifecycle,nullptr);
    device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&requested);
    if(device)SDL_SetAudioPostmixCallback(device,monitor_mix,nullptr);
    if (!device || !SDL_GetAudioDeviceFormat(device,&output,nullptr) || !SDL_ResumeAudioDevice(device)) {
        std::fprintf(stderr,"Android audio: %s\n",SDL_GetError());
        if (device) SDL_CloseAudioDevice(device);
        device=0;
    }
    if(device){int frames=0;SDL_GetAudioDeviceFormat(device,&output,&frames);std::fprintf(stderr,"SDL audio driver=%s rate=%d channels=%d frames=%d low_latency=%s\n",SDL_GetCurrentAudioDriver(),output.freq,output.channels,frames,SDL_GetHint(SDL_HINT_ANDROID_LOW_LATENCY_AUDIO));}
}
void recomp_audio_output_reset_voice(uint32_t slot)
{
    if (slot >= voices.size()) return;
    if (voices[slot].stream) SDL_DestroyAudioStream(voices[slot].stream);
    voices[slot] = Voice{};
}
void recomp_audio_output_shutdown(void)
{
    SDL_RemoveEventWatch(lifecycle,nullptr);
    for (uint32_t slot=0;slot<voices.size();++slot) recomp_audio_output_reset_voice(slot);
    if (device) SDL_CloseAudioDevice(device);
    device=0;
}
int recomp_audio_output_enabled(void) { return device!=0; }
void recomp_audio_output_submit(uint32_t slot,const uint8_t *pcm,uint32_t bytes,uint32_t rate,
                               uint32_t channels,uint32_t bits,int32_t volume)
{
    if(background.load(std::memory_order_acquire))return;
    if(flush_on_resume.exchange(false,std::memory_order_acquire)){
        for(uint32_t i=0;i<voices.size();++i)recomp_audio_output_reset_voice(i);
        std::fprintf(stderr,"SDL audio: cleared old playback queues on foreground\n");
    }
    if (!device || slot>=voices.size() || !pcm || !bytes || bytes>160000 || rate<1000 || rate>200000 ||
        (channels!=1 && channels!=2) || (bits!=8 && bits!=16) || bytes%(channels*bits/8)) return;
    Voice &voice=voices[slot];
    if (voice.stream && (voice.rate!=rate || voice.channels!=channels || voice.bits!=bits)){rate_resets.fetch_add(1,std::memory_order_relaxed);recomp_audio_output_reset_voice(slot);}
    if (!voice.stream) {
        SDL_AudioSpec input{bits==8 ? SDL_AUDIO_U8 : SDL_AUDIO_S16LE,(int)channels,(int)rate};
        voice.stream=SDL_CreateAudioStream(&input,&output);
        if (!voice.stream || !SDL_BindAudioStream(device,voice.stream)) { recomp_audio_output_reset_voice(slot); return; }
        voice.rate=rate;voice.channels=channels;voice.bits=bits;
    }
    uint32_t per_second=rate*channels*bits/8;
    int queued=SDL_GetAudioStreamQueued(voice.stream);
    if (queued<0 || (uint64_t)queued+bytes > (uint64_t)per_second/2+160000){dropped_packets.fetch_add(1,std::memory_order_relaxed);return;}
    if (!queued) {
        if(voice.submitted)empty_streams.fetch_add(1,std::memory_order_relaxed);
        std::vector<uint8_t> silence((rate/20)*channels*bits/8,bits==8 ? 128 : 0);
        SDL_PutAudioStreamData(voice.stream,silence.data(),(int)silence.size());
    }
    float fill=(float)queued/per_second;
    SDL_SetAudioStreamFrequencyRatio(voice.stream,1.0f+std::clamp((fill-0.05f)*0.2f,-0.01f,0.01f));
    SDL_SetAudioStreamGain(voice.stream,master*std::pow(10.0f,std::clamp(volume,-10000,0)/2000.0f));
    if (!SDL_PutAudioStreamData(voice.stream,pcm,(int)bytes)) std::fprintf(stderr,"Android PCM queue: %s\n",SDL_GetError());
    voice.submitted=true;
    uint64_t now=SDL_GetTicks();if(now-last_report>=2000){last_report=now;std::fprintf(stderr,"SDL audio mix peak=%.3f over_range=%llu samples=%llu empty_streams=%llu max_gap_ms=%.2f late_callbacks=%llu rate_resets=%llu dropped_packets=%llu\n",mix_peak.exchange(0)/1000.f,(unsigned long long)over_range.exchange(0),(unsigned long long)mix_samples.exchange(0),(unsigned long long)empty_streams.exchange(0),mix_max_gap_ns.exchange(0)/1e6,(unsigned long long)mix_late_callbacks.exchange(0),(unsigned long long)rate_resets.exchange(0),(unsigned long long)dropped_packets.exchange(0));}
}
