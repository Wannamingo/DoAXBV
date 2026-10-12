# Verification / Проверка

## 0.4.2 build and automated checks

- Full ARM64 native rebuild with NDK r30, with the pinned USA5 recipe, generated
  file hashes, manifest hash, and existing EBP override checks enabled.
- Public Android CMake configured without GAME_DIR or original-game inputs.
- Both Java layers compiled together with Java 17/API 35 and converted with D8.
- Touch model tests: four viewports, eight directions, two sticks, multitouch,
  sliding release, remapping, normalized layout persistence, reset and cancel.
- Preference tests: independent touch/gamepad bindings, swap compatibility,
  invalid values, resets, and isolation of layout preferences.
- Native binding test executed on RG477M: simultaneous inputs, remapping cycles,
  shared actions, pressure, disabled actions, thresholds, and stick directions.
- Real GLES test executed on RG477M: repeated 1×/2×/3× changes preserve main and
  offscreen pixels; failed allocation retains the original targets.
- Beach preview: all three resolutions, mapping bridge, RU/EN, and import lock.
- APK ZIP, JNI exports, matching previous signing certificate, and 16 KiB alignment.
- The installed APK hash matches the local release APK. USA ZIP import and game
  startup were exercised on RG477M.

## 0.4.2 device checks: RG477M

Verified on 12 October 2026, GammaOS / Android 14, Mali-G615, 1280 × 960:

- The installed release APK has SHA-256
  `499cdb418004d1d568fcb144c7cc29d5ef9b4237239992720fc15b88d786b6b5`.
- In-game settings applied 2× → 3× → 1× → 2× and source → 16:9 → source.
  Native logs confirm all sizes; the game process remained the same.
- The pencil editor and the settings-to-editor path retained a rendered game
  frame. Dragging A and saving persisted its normalized position across a game
  restart. Cancel retained the saved position; Reset + Save restored defaults.
- A touch A → app menu assignment and an independent physical B → app menu
  assignment both worked without restarting the game. Both layouts were reset
  afterwards. Select also opened the menu with the touch controls hidden.
- Both physical and touch left sticks moved selection in the original game's
  menus.
- Exiting through the app menu returned to Beach; another game launch succeeded.
- All six pre-update save files remained byte-for-byte identical. Original
  2× / source / Russian / automatic-touch settings were restored after testing.

These checks do not establish a complete playthrough or performance on every
Android device. The original upstream public-ci export/lifter checks remain
unavailable in this supplied snapshot, as recorded below.

## Historical publication checks: 0.4.1


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
