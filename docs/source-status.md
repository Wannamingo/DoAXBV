# Source composition / Состав исходников

## Current version: 0.4.2-ru-USA6-Beach

Version code 22 combines the existing Beach artwork and USA6 ISO importer with
the new control model, remapping, layout editor, in-game settings, and live GLES
resolution changes. The authenticated USA5 game recipe is unchanged. Native
code and both Java layers were rebuilt together; this APK does not reuse the
old native library or old DEX files.

The release contains handwritten source only. Generated game code, original
disc files, localization inputs, saves, signing keys, and local test receipts
remain private. A clean clone still lacks those private native build inputs.

The final manifest is `launcher/AndroidManifest.xml`. Use
`tools/package_android.py` to package the current native build and both Java
layers. See [build notes](building.md) and [verification](verification.md).

В 0.4.2 ядро и обе Java-части пересобраны вместе. Импорт USA6, рисунки Beach,
проверка версии диска и формат сохранений сохранены. Приватные игровые данные
и ключ подписи не включены в публичные исходники.

## Previous publication: 0.4.1


The 0.4.1-ru-USA6-Beach APK combines three supplied source components:

| Component | Public location | Provenance |
| --- | --- | --- |
| Beach interface and launcher | `launcher/` | Beach launcher source and manifest for version code 21 |
| Java application and ISO importer | `android/java/org/doaxbv/port/` | USA6 ISO Java source, including `XboxIso.java` |
| Native runtime and Android backend | `android/native/`, `recomp-runtime/`, `xbe/`, `tools/game-recipe/` | 0.4.0-ru-USA5 local source archive |

APK 0.4.1 использует Beach-интерфейс, Java-код импорта ISO USA6 и неизменённое нативное ядро USA5. Все три части включены в публичный экспорт.

The packaged `libmain.so` SHA-256 is `86fa2e699177298e8b40eb5b92745166ca86da2d5667213527016ddfcdf7c5f9`, identical to the packaged-native identity recorded for USA5. All five launcher asset files match the supplied APK byte for byte. Both Java layers were recompiled and converted with D8: the resulting `classes.dex` and `classes2.dex` match the APK byte for byte. This verifies native binary identity, Java-to-DEX correspondence, and launcher assets; it is not a new end-to-end native rebuild.

The older `android/AndroidManifest.xml` is retained with the USA5 native snapshot. The final Beach packaging entry point uses **`launcher/AndroidManifest.xml`**, version **0.4.1-ru-USA6-Beach**, code **21**.

### Public export in 0.4.1

Original game files, generated original-game C, signing keys, local receipts, machine-specific execution evidence, and internal progress documents are excluded. The private local-source ZIP is not a public release asset. The original GPL text and existing author notices are preserved.

In 0.4.1, `launcher/build_apk.py` was a public adaptation of the supplied Beach builder. Local absolute paths were replaced with environment settings; its local background check referred to the included launcher asset. Java, native, and interface source logic was otherwise unchanged. For the current 0.4.2 workflow, see [build notes](building.md).

### Reproducibility limits of the 0.4.1 export

A clean clone does not contain all native build inputs. It lacks locally generated authenticated original-game code, private localization inputs, the pinned lifter checkout, and parts of the original public generation tooling. The Beach packaging workflow also needs a local USA6 base APK, its compiled Java classes, compiled icon resources, and a private signing key. No signing key is published.

Для полной пересборки нужны собственный оригинальный образ, локальная генерация кода и недостающие локальные входные данные. Совпадение ядра и ресурсов не заменяет проверку полной пересборки APK. Проверки публикации описаны в [verification.md](verification.md).
