"""Authenticated native Android build using local private game inputs."""
from pathlib import Path
import argparse,subprocess,os,shutil,json,hashlib

parser=argparse.ArgumentParser()
parser.add_argument('--toolchains',type=Path,required=True)
parser.add_argument('--game-dir',type=Path,required=True)
parser.add_argument('--localization',type=Path,required=True,help='Private folder containing ru_catalog.inc and ru_font.inc')
parser.add_argument('--parallel',type=int,default=2)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1];tools=args.toolchains.resolve()
build=root/'private/build-arm64';build.mkdir(parents=True,exist_ok=True)
localization=root/'private/localization';localization.mkdir(parents=True,exist_ok=True)
for name in ['ru_catalog.inc','ru_font.inc']:
    source=(args.localization/name).resolve();destination=localization/name
    if source!=destination.resolve():shutil.copy2(source,destination)
env=dict(os.environ,TEMP=str(build),TMP=str(build))
cmake=tools/'cmake/bin/cmake.exe';recipe=root/'tools/game-recipe/usa-android.json'
command=[cmake,'-S',root/'android','-B',build,'-G','Ninja',
 '-DCMAKE_MAKE_PROGRAM='+str(tools/'ninja/ninja.exe'),
 '-DCMAKE_TOOLCHAIN_FILE='+str(tools/'android-ndk-r30/build/cmake/android.toolchain.cmake'),
 '-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-26','-DCMAKE_BUILD_TYPE=RelWithDebInfo',
 '-DGAME_DIR='+str(args.game_dir.resolve()),'-DGAME_RECIPE='+str(recipe),
 '-DGAME_RECIPE_SHA256=999e544bb24387de67f9f8d291769d8fd54103db57a317213e981c4ca16e0d37',
 '-DGLSLANG_DIR='+str(tools/'glslang-16.6.0'),
 '-DSPIRV_CROSS_DIR='+str(tools/'SPIRV-Cross-aa217aeb6c9f0ace7a0ab233b28807edf45eb165'),
 '-DSDL_ANDROID_ROOT='+str(tools/'SDL3-android/aar')]
for name,command in [('configure',command),('build',[cmake,'--build',build,'--target','main','doaxbv_button_bindings_test','doaxbv_live_display_test','--parallel',str(max(1,args.parallel))])]:
    print(name,flush=True)
    with (build/(name+'.log')).open('w') as log:
        result=subprocess.run(list(map(str,command)),env=env,stdout=log,stderr=subprocess.STDOUT)
    print('\n'.join((build/(name+'.log')).read_text(errors='replace').splitlines()[-15:]),flush=True)
    result.check_returncode()
identity=json.loads(recipe.read_text())
receipt={key:identity[key] for key in ['xbe_sha256','program_manifest_sha256']}
receipt.update(recipe_sha256=hashlib.sha256(recipe.read_bytes()).hexdigest(),native_sha256=hashlib.sha256((build/'libmain.so').read_bytes()).hexdigest())
(build/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt),flush=True)
