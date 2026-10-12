"""Run the platform-independent touch and preference tests with the build JDK."""
from pathlib import Path
import argparse,os,subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--toolchains',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
tools=args.toolchains.resolve();jdk=tools/'jdk-21.0.12.1+1/bin'
android=tools/'sdk/android-35/android.jar'
build=root/'private/control-tests';build.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,TMP=str(build),TEMP=str(build))
java=root/'android/java/org/doaxbv/port';tests=root/'android/tests/org/doaxbv/port'
sources=[java/(name+'.java') for name in ['ControlBindings','ControlPreferences','TouchControlsModel','TouchLayoutStore']]
sources += [tests/(name+'.java') for name in ['TouchControlsModelTest','ControlPreferencesTest']]
subprocess.run([str(jdk/'javac.exe'),'-encoding','UTF-8','-cp',str(android),'-d',str(build),*map(str,sources)],env=env,check=True)
for test in ['TouchControlsModelTest','ControlPreferencesTest']:
    subprocess.run([str(jdk/'java.exe'),'-cp',str(build)+os.pathsep+str(android),'org.doaxbv.port.'+test],env=env,check=True)
