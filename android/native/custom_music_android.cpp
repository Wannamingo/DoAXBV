// SPDX-License-Identifier: GPL-2.0-or-later
#include "custom_music.h"
#include <SDL3/SDL.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <chrono>
#include <codecvt>
#include <cstdio>
#include <filesystem>
#include <locale>
#include <memory>
#include <string>
#include <vector>
namespace {
struct Track {RecompMusicTrack info{};std::filesystem::path path;std::wstring wide;};
std::vector<Track> tracks;
AMediaExtractor *extractor(const std::filesystem::path &path,AMediaFormat **format)
{
    int fd=open(path.c_str(),O_RDONLY|O_CLOEXEC);struct stat info{};if(fd<0)return nullptr;
    AMediaExtractor *e=AMediaExtractor_new();bool okay=!fstat(fd,&info) && AMediaExtractor_setDataSourceFd(e,fd,0,info.st_size)==AMEDIA_OK;close(fd);
    if(okay)for(size_t i=0;i<AMediaExtractor_getTrackCount(e);++i){AMediaFormat *f=AMediaExtractor_getTrackFormat(e,i);const char *mime=nullptr;
        if(f && AMediaFormat_getString(f,AMEDIAFORMAT_KEY_MIME,&mime) && std::string(mime).find("audio/")==0 && AMediaExtractor_selectTrack(e,i)==AMEDIA_OK){*format=f;return e;}if(f)AMediaFormat_delete(f);}
    AMediaExtractor_delete(e);return nullptr;
}
}
struct RecompMusicDecoder {
    uint32_t id=0;AMediaExtractor *extractor=nullptr;AMediaCodec *codec=nullptr;SDL_AudioStream *stream=nullptr;
    bool started=false,input_end=false,output_end=false,failed=false;int rate=0,channels=0,encoding=2;
};
extern "C" void recomp_music_close(RecompMusicDecoder *d)
{if(!d)return;if(d->codec){if(d->started)AMediaCodec_stop(d->codec);AMediaCodec_delete(d->codec);}if(d->extractor)AMediaExtractor_delete(d->extractor);if(d->stream)SDL_DestroyAudioStream(d->stream);delete d;}
extern "C" RecompMusicDecoder *recomp_music_open(uint32_t id)
{
    if(!id || id>tracks.size())return nullptr;auto *d=new(std::nothrow) RecompMusicDecoder;if(!d)return nullptr;d->id=id;
    AMediaFormat *format=nullptr;d->extractor=extractor(tracks[id-1].path,&format);const char *mime=nullptr;
    if(!d->extractor || !AMediaFormat_getString(format,AMEDIAFORMAT_KEY_MIME,&mime)){if(format)AMediaFormat_delete(format);recomp_music_close(d);return nullptr;}
    AMediaFormat_getInt32(format,AMEDIAFORMAT_KEY_SAMPLE_RATE,&d->rate);AMediaFormat_getInt32(format,AMEDIAFORMAT_KEY_CHANNEL_COUNT,&d->channels);
    d->codec=AMediaCodec_createDecoderByType(mime);bool okay=d->codec && AMediaCodec_configure(d->codec,format,nullptr,nullptr,0)==AMEDIA_OK;AMediaFormat_delete(format);
    if(okay && AMediaCodec_start(d->codec)==AMEDIA_OK){d->started=true;return d;}recomp_music_close(d);return nullptr;
}
extern "C" void recomp_music_initialize(const char *folder)
{
    tracks.clear();if(!folder)return;
    try{std::filesystem::create_directories(folder);std::vector<std::filesystem::path> files;for(const auto &entry:std::filesystem::directory_iterator(folder))if(entry.is_regular_file())files.push_back(entry.path());std::sort(files.begin(),files.end());
        for(const auto &path:files){if(tracks.size()>=RECOMP_MUSIC_MAX_TRACKS)break;AMediaFormat *format=nullptr;AMediaExtractor *e=extractor(path,&format);if(!e)continue;const char *mime=nullptr;int64_t duration=0;AMediaFormat_getInt64(format,AMEDIAFORMAT_KEY_DURATION,&duration);AMediaFormat_getString(format,AMEDIAFORMAT_KEY_MIME,&mime);AMediaCodec *codec=mime ? AMediaCodec_createDecoderByType(mime):nullptr;AMediaFormat_delete(format);AMediaExtractor_delete(e);if(!codec || duration<=0){if(codec)AMediaCodec_delete(codec);continue;}AMediaCodec_delete(codec);
            Track track;track.path=path;track.wide=std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(path.string());track.info.id=tracks.size()+1;track.info.duration_ms=(uint32_t)std::min<int64_t>(UINT32_MAX,duration/1000);auto name=std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>{}.from_bytes(path.stem().string());for(size_t i=0;i<std::min<size_t>(31,name.size());++i)track.info.name[i]=name[i];tracks.push_back(std::move(track));}
        std::fprintf(stderr,"Android soundtrack: %zu supported local tracks\n",tracks.size());
    }catch(const std::exception &e){std::fprintf(stderr,"Android soundtrack: %s\n",e.what());tracks.clear();}
}
extern "C" void recomp_music_shutdown(void){tracks.clear();}
extern "C" uint32_t recomp_music_count(void){return tracks.size();}
extern "C" uint32_t recomp_music_duration(void){uint64_t sum=0;for(const auto &t:tracks)sum+=t.info.duration_ms;return (uint32_t)std::min<uint64_t>(UINT32_MAX,sum);}
extern "C" const RecompMusicTrack *recomp_music_track(uint32_t index){return index<tracks.size() ? &tracks[index].info:nullptr;}
extern "C" const wchar_t *recomp_music_path(uint32_t id){return id && id<=tracks.size() ? tracks[id-1].wide.c_str():nullptr;}
extern "C" int recomp_music_read(RecompMusicDecoder *d,void *pcm,uint32_t bytes)
{
    if(!d || !pcm || bytes>RECOMP_MUSIC_MAX_READ || bytes%4 || d->failed)return -1;unsigned written=0;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(written<bytes){if(d->stream){int count=SDL_GetAudioStreamData(d->stream,(uint8_t *)pcm+written,bytes-written);if(count<0){d->failed=true;return -1;}written+=count;if(written==bytes)break;}
        if(d->output_end)break;if(std::chrono::steady_clock::now()>deadline){d->failed=true;return -1;}
        if(!d->input_end){ssize_t index=AMediaCodec_dequeueInputBuffer(d->codec,0);if(index>=0){size_t capacity=0;uint8_t *buffer=AMediaCodec_getInputBuffer(d->codec,index,&capacity);ssize_t count=buffer ? AMediaExtractor_readSampleData(d->extractor,buffer,capacity):-2;if(count==-2){d->failed=true;return -1;}if(count<0){d->input_end=true;if(AMediaCodec_queueInputBuffer(d->codec,index,0,0,0,AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)!=AMEDIA_OK){d->failed=true;return -1;}}
            else{int64_t time=AMediaExtractor_getSampleTime(d->extractor);if(AMediaCodec_queueInputBuffer(d->codec,index,0,count,time,0)!=AMEDIA_OK){d->failed=true;return -1;}AMediaExtractor_advance(d->extractor);}}}
        AMediaCodecBufferInfo info{};ssize_t index=AMediaCodec_dequeueOutputBuffer(d->codec,&info,10000);
        if(index==AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED){AMediaFormat *f=AMediaCodec_getOutputFormat(d->codec);if(!f){d->failed=true;return -1;}AMediaFormat_getInt32(f,AMEDIAFORMAT_KEY_SAMPLE_RATE,&d->rate);AMediaFormat_getInt32(f,AMEDIAFORMAT_KEY_CHANNEL_COUNT,&d->channels);AMediaFormat_getInt32(f,"pcm-encoding",&d->encoding);AMediaFormat_delete(f);}
        else if(index>=0){size_t capacity=0;uint8_t *buffer=AMediaCodec_getOutputBuffer(d->codec,index,&capacity);bool okay=info.offset>=0 && info.size>=0 && size_t(info.offset)+info.size<=capacity;
            if(okay && info.size){SDL_AudioSpec in{d->encoding==4 ? SDL_AUDIO_F32LE:SDL_AUDIO_S16LE,d->channels,d->rate},out{SDL_AUDIO_S16LE,2,RECOMP_MUSIC_RATE};okay=(d->encoding==2 || d->encoding==4) && d->channels>0 && d->channels<=8 && d->rate>=1000 && d->rate<=200000;
                if(okay){if(!d->stream)d->stream=SDL_CreateAudioStream(&in,&out);else okay=SDL_SetAudioStreamFormat(d->stream,&in,&out);okay=okay && d->stream && SDL_PutAudioStreamData(d->stream,buffer+info.offset,info.size);}}
            d->output_end=(info.flags&AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)!=0;if(d->output_end && d->stream)SDL_FlushAudioStream(d->stream);AMediaCodec_releaseOutputBuffer(d->codec,index,false);if(!okay){d->failed=true;return -1;}}
        else if(index!=AMEDIACODEC_INFO_TRY_AGAIN_LATER && index!=AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED){d->failed=true;return -1;}
    }return written;
}
extern "C" int recomp_music_rewind(RecompMusicDecoder *d)
{
    if(!d)return 0;auto *replacement=recomp_music_open(d->id);if(!replacement)return 0;
    std::swap(*d,*replacement);recomp_music_close(replacement);return 1;
}
