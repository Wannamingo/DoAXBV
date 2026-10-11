// SPDX-License-Identifier: GPL-2.0-or-later
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <android/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cstring>
#include <vector>
int recomp_game_main(int,char **);
int main(int argc,char **argv)
{
    // Android presentation options stay outside the authenticated guest runner.
    setenv("RECOMP_DISPLAY_ASPECT","source",1);
    setenv("RECOMP_LANGUAGE","ru",1);
    setenv("RECOMP_FACE_SWAP","0",1);
    setenv("RECOMP_RENDER_SCALE","1",1);
    std::vector<char *> game_args;
    for(int i=0;i<argc;++i){
        if(!std::strcmp(argv[i],"--android-display=4:3"))setenv("RECOMP_DISPLAY_ASPECT","4:3",1);
        else if(!std::strcmp(argv[i],"--android-display=source"))setenv("RECOMP_DISPLAY_ASPECT","source",1);
        else if(!std::strcmp(argv[i],"--android-display=16:9"))setenv("RECOMP_DISPLAY_ASPECT","16:9",1);
        else if(!std::strcmp(argv[i],"--android-language=en"))setenv("RECOMP_LANGUAGE","en",1);
        else if(!std::strcmp(argv[i],"--android-language=ru"))setenv("RECOMP_LANGUAGE","ru",1);
        else if(!std::strcmp(argv[i],"--android-swap-buttons"))setenv("RECOMP_FACE_SWAP","1",1);
        else if(!std::strcmp(argv[i],"--android-resolution=2"))setenv("RECOMP_RENDER_SCALE","2",1);
        else game_args.push_back(argv[i]);
    }
    const int game_argc=(int)game_args.size();game_args.push_back(nullptr);
    const char *root=SDL_GetAndroidInternalStoragePath();
    if(!root){__android_log_print(ANDROID_LOG_ERROR,"DOAXBV","No application storage: %s",SDL_GetError());return 2;}
    std::filesystem::path folder(root);std::filesystem::create_directories(folder/"logs");
    int fd=open((folder/"logs/game.log").c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
    if(fd>=0){dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);close(fd);setvbuf(stdout,nullptr,_IOLBF,0);setvbuf(stderr,nullptr,_IONBF,0);}
    std::fprintf(stderr,"DOAXBV Android native ARM64 launch\n");
    auto music=(folder/"UserMusic").string();setenv("RECOMP_USER_MUSIC",music.c_str(),1);setenv("RECOMP_AUDIO_GAIN","0.75",0);
    setenv("RECOMP_STORAGE_ROOT",root,1);
    // Import existing profiles once; Android journals start on its own filesystem.
    if(!std::filesystem::exists(folder/".recomp-storage")){
        auto from=folder/"disc/.recomp-storage";
        if(std::filesystem::exists(from/"save-undo-v1/undo")){std::fprintf(stderr,"Unfinished Windows save journal: recover on Windows before importing.\n");return 2;}
        if(std::filesystem::is_directory(from)){
            auto staging=folder/".recomp-storage.importing";
            std::error_code error;std::filesystem::remove_all(staging,error);std::filesystem::create_directories(staging);
            for(const auto &entry:std::filesystem::directory_iterator(from))if(entry.path().filename()!="save-undo-v1")std::filesystem::copy(entry.path(),staging/entry.path().filename(),std::filesystem::copy_options::recursive);
            std::filesystem::rename(staging,folder/".recomp-storage");
        }
    }
    return recomp_game_main(game_argc,game_args.data());
}
