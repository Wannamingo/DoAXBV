# Build notes / Примечания к сборке

The APK combines the Beach launcher, USA6 Java ISO importer, and unchanged USA5 native core. See [source composition](source-status.md). A clean clone lacks the private inputs needed for a full native rebuild.

APK состоит из Beach-лаунчера, Java-импортера ISO USA6 и неизменённого ядра USA5. Чистое клонирование не включает приватные входные данные для полной сборки ядра.

## Toolchain recorded in the local source bundle

| Component | Version / layout |
| --- | --- |
| Python | Python 3 |
| Android NDK | r30, Clang 21 |
| CMake | 4.2.1 |
| Generator | Ninja |
| JDK | 21.0.12.1+1 |
| Android SDK build tools | 35.0.1 |
| Android SDK platform | API 35 |
| Native target | `arm64-v8a`, API 26 |
| SDL3 Android AAR | 3.4.16 |
| glslang | 16.6.0 |
| SPIRV-Cross | `aa217aeb6c9f0ace7a0ab233b28807edf45eb165` |

These versions are recorded by the provided bundle. They are not claims of a newly reproduced build.

## Layout and entry points

`android/CMakeLists.txt` defines the Android native targets. `recomp-runtime/CMakeLists.txt` defines the runtime and its model tests.

The preserved scripts in `tools/local-build/` originally lived next to `doaxbv-android/` and `toolchains/`. They resolve their source, toolchain, and private output directories relative to that location. Their location here is for review; running them unchanged from this directory does not recreate that layout.

The native build consumes authenticated generated code through `GAME_DIR` and `GAME_RECIPE`, and uses the recorded `GAME_RECIPE_SHA256`. Do not disable these checks or substitute a different game revision.

The older packaging workflow also expects local localization inputs, an app icon, a disc identity file, and a development signing key. These inputs are excluded from the public export. Use a private signing key for a build you prepare locally; never commit it.

For the original public generation workflow and lifter dependencies, consult the upstream [doaxbv-re repository](https://github.com/NoRain211/doaxvb-re). Those tools are not included in the supplied Android snapshot. Merely configuring the public runtime or passing model tests does not establish a working Android game build.


## Beach packaging

`launcher/build_apk.py` retains the supplied packaging workflow, using `launcher/AndroidManifest.xml`, the Beach Java sources, and the bundled interface assets. It preserves the base APK's native libraries and original classes, adds the Beach DEX, aligns the APK for 16 KiB pages, and signs it. Paths are configured through environment variables:

| Variable | Required value |
| --- | --- |
| `DOAXBV_TOOLCHAINS` | Folder with the toolchain layout listed above |
| `DOAXBV_BASE_APK` | Local USA6 ISO APK used as the base, before the Beach overlay |
| `DOAXBV_BASE_CLASSES` | Compiled classes from the USA6 Java sources in `android/java/` |
| `DOAXBV_NATIVE_SOURCE` | Local native project with `private/apk/icon-resources-0.zip` and `icon-resources-1.zip` |
| `DOAXBV_KEYSTORE` | Private development keystore; never commit it |
| `DOAXBV_OUTPUT_DIR` | Optional output directory; defaults to ignored `private/apk` |
| `DOAXBV_BUILD_DIR` | Optional build directory; defaults to ignored `private/beach-build` |

After preparing those local inputs, run `python launcher/build_apk.py`. The preserved builder expects the original development-key alias and passwords and verifies that the signing certificate matches the base APK. It is not a general release-signing tool. The ISO-import Java sources can be compiled with Java 17, Android API 35, and the SDL3 Android classes JAR; compiled classes themselves are not published.
