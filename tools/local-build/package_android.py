from pathlib import Path
import subprocess
import zipfile
import shutil
import hashlib
import json
import re

root = Path(__file__).resolve().parent
port = root / 'doaxbv-android'
out = port / 'private/apk'
out.mkdir(parents=True, exist_ok=True)
tools = root / 'toolchains'
jdk = tools / 'jdk-21.0.12.1+1/bin'
sdk = tools / 'sdk/android-15'
android = tools / 'sdk/android-35/android.jar'
sdl = tools / 'SDL3-android/aar'
classes = out / 'classes'
dex = out / 'dex'
classes.mkdir(exist_ok=True)
dex.mkdir(exist_ok=True)
def run(command):
    result = subprocess.run(list(map(str, command)), capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise RuntimeError('Packaging tool failed: ' + str(command[0]))
    if result.stderr.strip(): print(result.stderr.strip()[:1500])

native = port/'private/build-arm64/libmain.so'
symbols = subprocess.check_output([str(tools/'android-ndk-r30/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-nm.exe'),'-D',str(native)],text=True)
for java in (port/'android/java').rglob('*.java'):
    source = java.read_text(encoding='utf-8')
    package = re.search(r'package\s+([\w.]+);',source).group(1)
    for method in re.findall(r'\bnative\s+\w+\s+(\w+)\s*\(',source):
        symbol = 'Java_'+package.replace('.','_')+'_'+java.stem+'_'+method
        if not re.search(r'\bT '+re.escape(symbol)+r'\s*$',symbols,re.M):
            raise RuntimeError('Native library is stale or incomplete: '+symbol)

run([jdk/'javac.exe','--release','17','-encoding','UTF-8','-classpath',str(android)+';'+str(sdl/'classes.jar'),
     '-d',classes,*sorted((port/'android/java').rglob('*.java'))])
jar = out / 'app-classes.jar'
run([jdk/'jar.exe','--create','--file',jar,'-C',classes,'.'])
run([jdk/'java.exe','-cp',sdk/'lib/d8.jar','com.android.tools.r8.D8','--min-api','26',
     '--lib',android,'--output',dex,jar,sdl/'classes.jar'])
unsigned = out / 'unsigned.apk'
identity=json.loads((port/'private/official-usa/identity.json').read_text(encoding='utf8'))
disc_identity=json.dumps({name:{'size':entry['official_bytes'],'sha256':entry['official_sha256']}
                         for name,entry in identity['files'].items()},sort_keys=True).encode('utf8')
resources = []
for index, folder in enumerate((port/'android/res', port/'private/app-icon/res')):
    compiled = out / f'icon-resources-{index}.zip'
    run([sdk/'aapt2.exe','compile','--dir',folder,'-o',compiled])
    resources.append(compiled)
run([sdk/'aapt2.exe','link','-o',unsigned,'--manifest',port/'android/AndroidManifest.xml',
     '-I',android,'--min-sdk-version','26','--target-sdk-version','35',*resources])
library = out / 'libmain.so'
shutil.copy2(native,library)
run([tools/'android-ndk-r30/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe','--strip-debug',library])
with zipfile.ZipFile(unsigned,'a') as archive:
    archive.writestr('assets/disc-identity.json',disc_identity,compress_type=zipfile.ZIP_DEFLATED)
    for notice in sorted((port/'android/third_party/notices').glob('*')):
        if notice.is_file():archive.write(notice,'assets/third-party/'+notice.name,compress_type=zipfile.ZIP_DEFLATED)
    archive.write(port/'android/third_party/xxhash/LICENSE',
                  'assets/third-party/xxhash-LICENSE.txt',compress_type=zipfile.ZIP_DEFLATED)
    for file in sorted(dex.glob('classes*.dex')):
        archive.write(file,file.name,compress_type=zipfile.ZIP_DEFLATED)
    archive.write(library,'lib/arm64-v8a/libmain.so',compress_type=zipfile.ZIP_STORED)
    archive.write(sdl/'prefab/modules/SDL3-shared/libs/android.arm64-v8a/libSDL3.so',
                  'lib/arm64-v8a/libSDL3.so',compress_type=zipfile.ZIP_STORED)
aligned = out / 'aligned.apk'
run([sdk/'zipalign.exe','-P','16','-f','4',unsigned,aligned])
key = out / 'local-debug.jks'
if not key.exists():
    run([jdk/'keytool.exe','-genkeypair','-keystore',key,'-storepass','android','-keypass','android',
         '-alias','androiddebugkey','-keyalg','RSA','-keysize','2048','-validity','3650',
         '-dname','CN=Local DOAXBV Android Development'])
apk = out / 'DOAXBV-RG477M-arm64-dev.apk'
run([jdk/'java.exe','-jar',sdk/'lib/apksigner.jar','sign','--ks',key,'--ks-key-alias','androiddebugkey',
     '--ks-pass','pass:android','--key-pass','pass:android','--out',apk,aligned])
run([jdk/'java.exe','-jar',sdk/'lib/apksigner.jar','verify','--verbose',apk])
run([sdk/'zipalign.exe','-c','-P','16','4',apk])
manifest=(port/'android/AndroidManifest.xml').read_text(encoding='utf-8')
version=re.search(r'android:versionName="([^"]+)"',manifest).group(1)
receipt={'version':version,'apk':apk.name,'bytes':apk.stat().st_size,'sha256':hashlib.sha256(apk.read_bytes()).hexdigest(),
         'abi':'arm64-v8a','min_api':26,'target_api':35,'display_profile':'complete frame fit: source, 4:3 or 16:9; internal 720x480 or 1440x960','text_languages':['ru','en'],'controls':'SDL3 physical gamepad; optional touch controls and face-button swap','native_sha256':hashlib.sha256(native.read_bytes()).hexdigest(),'packaged_native_sha256':hashlib.sha256(library.read_bytes()).hexdigest(),'private_game_manifest':identity['program_manifest_sha256'],'game_resources':'official USA, all 29 files verified against user USA ISO','xbe_sha256':identity['xbe_sha256'],
         'status':'Russian localization in progress; prototype requires glyph/layout and coverage verification; baseline 0.3.3 gameplay/audio fixes retained; complete playthrough not verified'}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt,indent=2))
