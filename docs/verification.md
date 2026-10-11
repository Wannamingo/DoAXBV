# Publication verification / Проверка публикации

Verified on 11 October 2026 for this public source export:

- Preserved native Android/runtime/parser/recipe source files and the GPL license were compared byte for byte with the supplied 0.4.0-ru-USA5 archive. The updated Java importer, Beach sources, manifest, and assets were compared with the two newer source components.
- Private directories, signing keys, original-game files, generated original-game C, and local receipts were excluded.
- Relative documentation links were checked.
- The supplied APK passed ZIP CRC verification, and the release copy matched its SHA-256.
- The packaged native library matched the packaged-native SHA-256 recorded for USA5. All five Beach asset files matched the supplied APK.
- USA6 Java sources and Beach Java sources were compiled with Java 17 and Android API 35, then converted with D8. Both resulting DEX files matched the APK's `classes.dex` and `classes2.dex` byte for byte. The base DEX includes the recorded SDL3 Android Java library.
- The public Android CMake project configured successfully for ARM64 / API 26 with NDK r30, without a private `GAME_DIR` or local game files.

This verifies the publication package, Java-to-DEX correspondence, and public Android configuration. It does not establish a full native/APK rebuild, device compatibility, gameplay behavior, or passing runtime tests. The full upstream `public-ci` checks were not run: the supplied local snapshot omits its public-export tooling and pinned lifter checkout. No game or runtime source logic was modified for publication.

Проверены состав экспорта, сохранность исходников, ссылки, целостность APK и Android CMake без приватных входных данных. Оба DEX-файла пересобраны из Java-исходников и совпали с APK. Полная пересборка ядра и APK, запуск на устройстве и полный набор upstream-тестов не выполнялись. Логика игры и runtime не изменялась.
