# Public adaptation: local machine paths are configured through environment variables.
from pathlib import Path
import subprocess, zipfile, re, json, hashlib, shutil, os

ROOT=Path(__file__).resolve().parent
WORKSPACE=ROOT.parent
TOOLS=Path(os.environ['DOAXBV_TOOLCHAINS'])
JDK=TOOLS/'jdk-21.0.12.1+1/bin'
SDK=TOOLS/'sdk/android-15'
ANDROID=TOOLS/'sdk/android-35/android.jar'
SDL=TOOLS/'SDL3-android/aar/classes.jar'
ORIGINAL=Path(os.environ['DOAXBV_BASE_APK'])
EXISTING=Path(os.environ['DOAXBV_BASE_CLASSES'])
OLD=Path(os.environ['DOAXBV_NATIVE_SOURCE'])
KEY=Path(os.environ['DOAXBV_KEYSTORE'])
OUT=Path(os.environ.get('DOAXBV_OUTPUT_DIR', WORKSPACE/'private/apk'))
BUILD=Path(os.environ.get('DOAXBV_BUILD_DIR', WORKSPACE/'private/beach-build'))
BUILD.mkdir(parents=True,exist_ok=True);OUT.mkdir(parents=True,exist_ok=True)

def run(args):
    args=list(map(str,args))
    if Path(args[0]).name=='java.exe':args.insert(1,'-Djava.io.tmpdir='+str(BUILD))
    result=subprocess.run(args,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if result.returncode:
        raise RuntimeError(result.stdout+result.stderr)
    return result.stdout+result.stderr

classes=BUILD/'classes';dex=BUILD/'dex'
classes.mkdir(exist_ok=True);dex.mkdir(exist_ok=True)
run([JDK/'javac.exe','--release','17','-encoding','UTF-8','-classpath',str(ANDROID)+';'+str(SDL)+';'+str(EXISTING),'-d',classes,*sorted((ROOT/'java').rglob('*.java'))])
jar=BUILD/'beach-launcher.jar'
with zipfile.ZipFile(jar,'w',compression=zipfile.ZIP_DEFLATED) as target:
    for path in classes.rglob('*.class'):target.write(path,path.relative_to(classes).as_posix())
run([JDK/'java.exe','-cp',SDK/'lib/d8.jar','com.android.tools.r8.D8','--min-api','26','--lib',ANDROID,'--classpath',SDL,'--classpath',EXISTING,'--output',dex,jar])
unsigned=BUILD/'unsigned.apk'
run([SDK/'aapt2.exe','link','-o',unsigned,'--manifest',ROOT/'AndroidManifest.xml','-I',ANDROID,'--min-sdk-version','26','--target-sdk-version','35',OLD/'private/apk/icon-resources-0.zip',OLD/'private/apk/icon-resources-1.zip'])
retained=[]
with zipfile.ZipFile(ORIGINAL) as source,zipfile.ZipFile(unsigned,'a') as target:
    for entry in source.infolist():
        name=entry.filename
        if name in ('AndroidManifest.xml','resources.arsc') or name.startswith(('res/','META-INF/')):continue
        target.writestr(entry,source.read(name));retained.append(name)
    target.write(dex/'classes.dex','classes2.dex',compress_type=zipfile.ZIP_DEFLATED)
    for file in (ROOT/'assets').rglob('*'):
        if file.is_file():target.write(file,'assets/'+file.relative_to(ROOT/'assets').as_posix(),compress_type=zipfile.ZIP_DEFLATED)
aligned=BUILD/'aligned.apk'
run([SDK/'zipalign.exe','-P','16','-f','4',unsigned,aligned])
apk=OUT/'DOAXBV-USA6-Beach-Launcher.apk'
run([JDK/'java.exe','-jar',SDK/'lib/apksigner.jar','sign','--ks',KEY,'--ks-key-alias','androiddebugkey','--ks-pass','pass:android','--key-pass','pass:android','--out',apk,aligned])
old_cert=run([JDK/'java.exe','-jar',SDK/'lib/apksigner.jar','verify','--print-certs',ORIGINAL])
new_cert=run([JDK/'java.exe','-jar',SDK/'lib/apksigner.jar','verify','--verbose','--print-certs',apk])
pattern=r'certificate SHA-256 digest: (\w+)'
assert re.search(pattern,old_cert).group(1)==re.search(pattern,new_cert).group(1),'Signature changed'
run([SDK/'zipalign.exe','-c','-P','16','4',apk])
badging=run([SDK/'aapt2.exe','dump','badging',apk])
assert "versionCode='21'" in badging and "versionName='0.4.1-ru-USA6-Beach'" in badging
assert "launchable-activity: name='org.doaxbv.port.BeachLauncherActivity'" in badging
with zipfile.ZipFile(ORIGINAL) as source,zipfile.ZipFile(apk) as result:
    for name in retained:assert source.read(name)==result.read(name),'Original payload changed: '+name
    for name in source.namelist():
        if name.startswith('res/') or name=='resources.arsc':assert source.read(name)==result.read(name),'Original resources changed: '+name
    assert result.testzip() is None
    assert result.read('assets/launcher/background.jpg')==(ROOT/'assets/launcher/background.jpg').read_bytes()
    original_hash=hashlib.sha256(source.read('classes.dex')).hexdigest()
receipt={
 'version':'0.4.1-ru-USA6-Beach','versionCode':21,'package':'org.doaxbv.port',
 'apk':apk.name,'bytes':apk.stat().st_size,'sha256':hashlib.sha256(apk.read_bytes()).hexdigest(),
 'same_signing_certificate_as_supplied_apk':True,'signer_sha256':re.search(pattern,new_cert).group(1),
 'original_classes_dex_identical':True,'original_classes_dex_sha256':original_hash,
 'original_native_libraries_assets_and_resources_identical':True,'zipalign_16KiB':True,
 'preferences':{'file':'controls','retained_keys':['displayAspect','language','resolution','touchControls','swapButtons'],'legacy_aspect_migration':'Original LauncherActivity.savedAspect'},
 'background':'Supplied launcher background, unchanged',
 'logo':'Original datadvd.afs/spDemo.xpr, texture 0, decoded with original palette and alpha',
 'new_permissions':[],'android_device_tested':False,
}
(OUT/'verification.json').write_text(json.dumps(receipt,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
(OUT/'SHA256.txt').write_text(receipt['sha256']+'  '+apk.name+'\n',encoding='ascii')
(BUILD/'signature-verification.txt').write_text(new_cert,encoding='utf-8')
print(json.dumps(receipt,indent=2,ensure_ascii=False))
