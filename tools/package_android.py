"""Package the current native build and both Java layers with the existing app key."""
from pathlib import Path
import argparse,subprocess,zipfile,hashlib,json,shutil,re,os
import xml.etree.ElementTree as ET

parser=argparse.ArgumentParser()
parser.add_argument('--toolchains',type=Path,required=True)
parser.add_argument('--base-apk',type=Path,required=True,help='Previous USA6 Beach APK, for disc identity and certificate verification')
parser.add_argument('--icon-resources',type=Path,nargs='+',required=True,help='Original aapt2-compiled icon resource ZIPs')
parser.add_argument('--keystore',type=Path,required=True,help='Private development key matching the previous APK')
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1];tools=args.toolchains.resolve();out=args.output.resolve()
out.mkdir(parents=True,exist_ok=True)
build=root/'private/apk';build.mkdir(parents=True,exist_ok=True)
native=root/'private/build-arm64/libmain.so'
receipt=json.loads((root/'private/build-arm64/build-receipt.json').read_text())
recipe=root/'tools/game-recipe/usa-android.json';identity=json.loads(recipe.read_text())
assert receipt['recipe_sha256']==hashlib.sha256(recipe.read_bytes()).hexdigest()
assert receipt['native_sha256']==hashlib.sha256(native.read_bytes()).hexdigest()
for key in ['xbe_sha256','program_manifest_sha256']:assert receipt[key]==identity[key]
jdk=tools/'jdk-21.0.12.1+1/bin';sdk=tools/'sdk/android-15';android=tools/'sdk/android-35/android.jar';sdl=tools/'SDL3-android/aar'
llvm=tools/'android-ndk-r30/toolchains/llvm/prebuilt/windows-x86_64/bin'
env=dict(os.environ,TMP=str(build),TEMP=str(build))
def run(cmd):
    result=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,encoding='utf-8',errors='replace',env=env)
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    return result.stdout+result.stderr

sources=sorted((root/'android/java').rglob('*.java'))+sorted((root/'launcher/java').rglob('*.java'))
symbols=run([llvm/'llvm-nm.exe','-D',native])
for path in sources:
    source=path.read_text(encoding='utf-8');package=re.search(r'package\s+([\w.]+);',source).group(1)
    for method in re.findall(r'\bnative\s+\w+\s+(\w+)\s*\(',source):
        symbol='Java_'+package.replace('.','_')+'_'+path.stem+'_'+method
        assert re.search(r'\bT '+re.escape(symbol)+r'\s*$',symbols,re.M),symbol
classes=build/'classes';dex=build/'dex'
for folder in [classes,dex]:
    assert folder.resolve().parent==build.resolve() and build.resolve().is_relative_to(root.resolve())
    if folder.exists():shutil.rmtree(folder)
    folder.mkdir()
run([jdk/'javac.exe','--release','17','-encoding','UTF-8','-classpath',str(android)+';'+str(sdl/'classes.jar'),'-d',classes,*sources])
jar=build/'app-classes.jar';run([jdk/'jar.exe','--create','--file',jar,'-C',classes,'.'])
run([jdk/'java.exe','-Djava.io.tmpdir='+str(build),'-cp',sdk/'lib/d8.jar','com.android.tools.r8.D8','--min-api','26','--lib',android,'--output',dex,jar,sdl/'classes.jar'])
unsigned=build/'unsigned.apk';run([sdk/'aapt2.exe','link','-o',unsigned,'--manifest',root/'launcher/AndroidManifest.xml','-I',android,'--min-sdk-version','26','--target-sdk-version','35',*args.icon_resources])
library=build/'libmain.so';shutil.copy2(native,library);run([llvm/'llvm-strip.exe','--strip-debug',library])
with zipfile.ZipFile(args.base_apk) as previous,zipfile.ZipFile(unsigned,'a') as archive:
    for name in previous.namelist():
        if name.startswith('assets/') and not name.startswith('assets/launcher/'):
            archive.writestr(name,previous.read(name),compress_type=zipfile.ZIP_DEFLATED)
    assert json.loads(previous.read('assets/disc-identity.json'))['default.xbe']['sha256']==identity['xbe_sha256']
    for path in (root/'launcher/assets').rglob('*'):
        if path.is_file():archive.write(path,'assets/'+path.relative_to(root/'launcher/assets').as_posix(),compress_type=zipfile.ZIP_DEFLATED)
    for path in dex.glob('classes*.dex'):archive.write(path,path.name,compress_type=zipfile.ZIP_DEFLATED)
    archive.write(library,'lib/arm64-v8a/libmain.so',compress_type=zipfile.ZIP_STORED)
    archive.write(sdl/'prefab/modules/SDL3-shared/libs/android.arm64-v8a/libSDL3.so','lib/arm64-v8a/libSDL3.so',compress_type=zipfile.ZIP_STORED)
aligned=build/'aligned.apk';run([sdk/'zipalign.exe','-P','16','-f','4',unsigned,aligned])
manifest=ET.parse(root/'launcher/AndroidManifest.xml').getroot()
version=manifest.get('{http://schemas.android.com/apk/res/android}versionName');code=manifest.get('{http://schemas.android.com/apk/res/android}versionCode')
assert re.fullmatch(r'[0-9.]+-ru-USA6-Beach',version),version
apk=out/('DOAXBV-'+version+'.apk')
run([jdk/'java.exe','-jar',sdk/'lib/apksigner.jar','sign','--ks',args.keystore,'--ks-key-alias','androiddebugkey','--ks-pass','pass:android','--key-pass','pass:android','--out',apk,aligned])
verification=run([jdk/'java.exe','-jar',sdk/'lib/apksigner.jar','verify','--verbose','--print-certs',apk])
previous=run([jdk/'java.exe','-jar',sdk/'lib/apksigner.jar','verify','--print-certs',args.base_apk])
pattern=r'certificate SHA-256 digest: (\w+)'
assert re.search(pattern,previous).group(1)==re.search(pattern,verification).group(1),'Signing certificate changed'
run([sdk/'zipalign.exe','-c','-P','16','4',apk])
badging=run([sdk/'aapt2.exe','dump','badging',apk])
assert "package: name='org.doaxbv.port'" in badging and "launchable-activity: name='org.doaxbv.port.BeachLauncherActivity'" in badging
assert "versionCode='"+code+"'" in badging and "versionName='"+version+"'" in badging
with zipfile.ZipFile(apk) as archive:
    assert archive.testzip() is None
    for path in (root/'launcher/assets').rglob('*'):
        if path.is_file():assert archive.read('assets/'+path.relative_to(root/'launcher/assets').as_posix())==path.read_bytes()
(build/'signature-verification.txt').write_text(verification)
result={'apk':apk.name,'version':version,'versionCode':int(code),'bytes':apk.stat().st_size,'sha256':hashlib.sha256(apk.read_bytes()).hexdigest(),'abi':'arm64-v8a','aligned_16k':True,'same_signing_certificate':True}
(out/'build-verification.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
