# Build notes / Примечания к сборке

## Current 0.4.2 packaging

The native CMake project and authenticated USA5 recipe are unchanged in their
input validation. Build `main`, `doaxbv_button_bindings_test`, and
`doaxbv_live_display_test` with the toolchain below. The native build also needs
local `ru_catalog.inc` and `ru_font.inc`; these original-game-dependent inputs
are not published.

`tools/package_android.py` recompiles `android/java/` and `launcher/java/`
together, packages the current `private/build-arm64/libmain.so`, checks JNI
exports, preserves disc identity/notices from the previous USA6 APK, includes
Beach assets, and verifies the signing certificate and 16 KiB ZIP alignment.
It requires a `private/build-arm64/build-receipt.json` containing the verified
recipe, XBE, generated manifest, and native SHA-256 identities from the build.

```text
python tools/build_android.py --toolchains <toolchains> --game-dir <authenticated-generated-code> --localization <private-localization>
python tools/verify_controls.py --toolchains <toolchains>
python tools/package_android.py --toolchains <toolchains> --base-apk <previous-USA6-Beach.apk> --icon-resources <icon-resources-0.zip> <icon-resources-1.zip> --keystore <private-key.jks> --output <output-directory>
```

The icon resources are the original aapt2-compiled ZIPs. The preserved local
development key uses alias `androiddebugkey`; do not commit it. Source from a
clean clone alone is still insufficient to rebuild the full native game.

`launcher/build_apk.py` now forwards to the current packaging script. The
historical overlay-only packaging procedure below describes 0.4.1 only.

## Historical 0.4.1 toolchain and packaging


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

In 0.4.1, `launcher/build_apk.py` used `launcher/AndroidManifest.xml`, the Beach Java sources, and the bundled interface assets. It preserved the base APK's native libraries and original classes, added the Beach DEX, aligned the APK for 16 KiB pages, and signed it. This historical builder used environment variables:

| Variable | Required value |
| --- | --- |
| `DOAXBV_TOOLCHAINS` | Folder with the toolchain layout listed above |
| `DOAXBV_BASE_APK` | Local USA6 ISO APK used as the base, before the Beach overlay |
| `DOAXBV_BASE_CLASSES` | Compiled classes from the USA6 Java sources in `android/java/` |
| `DOAXBV_NATIVE_SOURCE` | Local native project with `private/apk/icon-resources-0.zip` and `icon-resources-1.zip` |
| `DOAXBV_KEYSTORE` | Private development keystore; never commit it |
| `DOAXBV_OUTPUT_DIR` | Optional output directory; defaults to ignored `private/apk` |
| `DOAXBV_BUILD_DIR` | Optional build directory; defaults to ignored `private/beach-build` |

The 0.4.1 builder expected the original development-key alias and passwords and verified that the signing certificate matched the base APK. For 0.4.2, use the current CLI procedure at the top of this document. The ISO-import Java sources compile with Java 17, Android API 35, and the SDL3 Android classes JAR; compiled classes themselves are not published.
