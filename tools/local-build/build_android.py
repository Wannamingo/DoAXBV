import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
source = root / 'doaxbv-android'
build = source / 'private/build-arm64'
cmake = Path(os.environ.get('DOAXBV_CMAKE',str(root/'toolchains/cmake/bin/cmake.exe')))
if not cmake.is_file():
    raise SystemExit('Put CMake in work/toolchains/cmake or set DOAXBV_CMAKE to its executable.')
command = [str(cmake), '-S', str(source / 'android'), '-B', str(build), '-G', 'Ninja',
           '-DCMAKE_MAKE_PROGRAM=' + str(root / 'toolchains/ninja/ninja.exe'),
           '-DCMAKE_TOOLCHAIN_FILE=' + str(root / 'toolchains/android-ndk-r30/build/cmake/android.toolchain.cmake'),
           '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-26', '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
           '-DGAME_DIR=' + str(source / 'private/game/generated'),
           '-DGAME_RECIPE=' + str(source / 'tools/game-recipe/usa-android.json'),
           '-DGAME_RECIPE_SHA256=999e544bb24387de67f9f8d291769d8fd54103db57a317213e981c4ca16e0d37',
           '-DGLSLANG_DIR=' + str(root / 'toolchains/glslang-16.6.0'),
           '-DSPIRV_CROSS_DIR=' + str(root / 'toolchains/SPIRV-Cross-aa217aeb6c9f0ace7a0ab233b28807edf45eb165'),
           '-DSDL_ANDROID_ROOT=' + str(root / 'toolchains/SDL3-android/aar')]
for name,args in [('app-configure',command),('app-build',[str(cmake), '--build', str(build), '--target', *(sys.argv[1:] or ['main']), '--parallel', '2'])]:
    log = source / ('private/' + name + '.log')
    print(name, flush=True)
    with log.open('w') as output:
        result = subprocess.run(args, stdout=output, stderr=subprocess.STDOUT)
    print('\n'.join(log.read_text(errors='replace').splitlines()[-25:]), flush=True)
    if result.returncode:
        sys.exit(result.returncode)
