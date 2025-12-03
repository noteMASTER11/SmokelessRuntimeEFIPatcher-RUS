# Building — руководство по сборке Smokeless Runtime EFI Patcher

Документ описывает подготовку окружения и порядок сборки `SmokelessRuntimeEFIPatcher` средствами EDK II. Утилита рассчитана на работу в среде UEFI Shell и использует отдельные заголовки из старого EDK1 (для `EFI_DISPLAY_PROTOCOL`), поэтому в списке зависимостей потребуется как современный EDK II, так и копия EDK1/EDK Legacy.

## 1. Программные требования

| Компонент | Назначение | Рекомендации |
|-----------|------------|--------------|
| Windows 10/11 x64 | Основная хост-система (поддерживается и Linux, но команды приведены для Windows). | Запуск от имени администратора ускорит настройку. |
| Visual Studio 2019/2022 (Desktop development with C++) | Компилятор и тулчейн для `VS2019`/`VS2022`. | Убедитесь, что выбран тот же тулчейн в `Conf/target.txt`. |
| Python 3.9+ | Необходим `BaseTools` EDK II. | Добавьте в `%PATH%`. |
| NASM 2.15+ | Сборка криптографических библиотек. | Добавьте в `%PATH%`. |
| Git | Получение исходников EDK II и EDK1. | — |
| **EDK II** (репозиторий `https://github.com/tianocore/edk2`) | Базовая инфраструктура сборки. | Клонируйте в каталог `<workspace>\edk2`. |
| **EDK Legacy (EDK1)** (`https://github.com/tianocore/edklegacy` или архив UDK2010`) | (Опционально) Полный набор заголовков старого `DisplayProtocol`. | В репозитории уже присутствует минимальный заголовок, но при необходимости можно заменить его полноценной версией. |
| Oniguruma.efi (предварительно собранный) | Поставляет `EFI_REGULAR_EXPRESSION_PROTOCOL`. | Разместите рядом с итоговым бинарником для запуска. |

> Примечание. Минимальный `DisplayProtocol.h` уже включён в репозиторий. При желании его можно заменить файлом из `edklegacy`, если требуется полная совместимость с оригинальной реализацией.

## 2. Подготовка структуры каталогов

1. Создайте рабочий каталог, например `C:\Work\UEFI`.
2. Клонируйте EDK II и edklegacy:
   ```bat
   cd C:\Work\UEFI
   git clone https://github.com/tianocore/edk2.git
   git clone https://github.com/tianocore/edklegacy.git
   ```
3. Клонируйте данный репозиторий в подкаталог `edk2\SREPPkg`:
   ```bat
   cd edk2
   git clone https://github.com/<your-account>/SmokelessRuntimeEFIPatcher-RUS.git SREPPkg
   ```
4. (Опционально) Если требуется оригинальный заголовок из EDK1, скопируйте его поверх поставленного в `SREPPkg\SmokelessRuntimeEFIPatcher\Include\Protocol\DisplayProtocol.h`.

## 3. Настройка EDK II

1. Откройте “x64 Native Tools Command Prompt for VS 2019/2022”.
2. Перейдите в каталог `edk2` и инициализируйте среду:
   ```bat
   cd C:\Work\UEFI\edk2
   git submodule update --init  # если требуется
   edksetup.bat Rebuild
   ```
3. Отредактируйте `Conf\target.txt` (или используйте переменные окружения) и задайте параметры:
   ```
   ACTIVE_PLATFORM  = SREPPkg/SmokelessRuntimeEFIPatcher.dsc
   TARGET           = DEBUG
   TARGET_ARCH      = X64
   TOOL_CHAIN_TAG   = VS2019
   ```
   При необходимости добавьте `IA32` в `TARGET_ARCH`, если планируется 32-битная сборка.
4. Убедитесь, что `Conf\tools_def.txt` содержит запись для выбранного тулчейна (`VS2019` или `VS2022`). Если используете `VS2022`, достаточно добавить в `TOOL_CHAIN_TAG` значение `VS2022` и подключить соответствующую секцию из свежего edk2.
5. (Опционально) Начиная с VS2019, стоит задать переменную `CLANG_BIN`/`NASM_PREFIX`, если инструменты установлены в нестандартные директории.

## 4. Сборка

- Сценарий для 64-битной сборки в режиме `DEBUG`:
  ```bat
  build -a X64 -b DEBUG -p SREPPkg/SmokelessRuntimeEFIPatcher.dsc -t VS2019
  ```
- Для релизного варианта:
  ```bat
  build -a X64 -b RELEASE -p SREPPkg/SmokelessRuntimeEFIPatcher.dsc -t VS2019
  ```
- Для 32-битного UEFI:
  ```bat
  build -a IA32 -b RELEASE -p SREPPkg/SmokelessRuntimeEFIPatcher.dsc -t VS2019
  ```

Результат появится в каталоге `Build\SmokelessRuntimeEFIPatcher\<TARGET>_<TOOLCHAIN>\SREPPkg\SmokelessRuntimeEFIPatcher\SmokelessRuntimeEFIPatcher\OUTPUT\SmokelessRuntimeEFIPatcher.efi`.

## 5. Проверка и упаковка

1. Перейдите в каталог сборки и скопируйте `SmokelessRuntimeEFIPatcher.efi` на загрузочный FAT32-том (USB).
2. Добавьте `Oniguruma.efi` и конфигурационный файл `*.cfg` в тот же каталог.
3. (Опционально) Скопируйте `SREP_DEBUG.log` (если нужен шаблон пустого файла) или удалите старый перед тестом.
4. Запустите UEFI Shell и выполните:
   ```
   fs0:
   set SREP_DEBUG=1          # для автоматического логирования
   SmokelessRuntimeEFIPatcher.efi -d
   ```
5. После завершения убедитесь, что создан `SREP_DEBUG.log` и в нём отражены DEBUG-сообщения.

## 6. Сборка в Linux/WSL (коротко)

EDK II поддерживает GNU- и Clang-тулчейны. Для Ubuntu (или WSL) установите зависимости:
```bash
sudo apt install build-essential nasm uuid-dev python3-distutils
```
Используйте тулчейн `GCC5`:
```bash
source edksetup.sh
make -C BaseTools
build -a X64 -b RELEASE -p SREPPkg/SmokelessRuntimeEFIPatcher.dsc -t GCC5
```
При этом заголовки EDK1 также придётся разместить в `SREPPkg/Include/Protocol` или прописать дополнительный путь в `package.dsc`.

## 7. Советы по устранению ошибок

- **`Protocol/DisplayProtocol.h: No such file or directory`** — проверьте, что заголовок скопирован из EDK1 и лежит в пути, доступном компилятору.
- **`Oniguruma` не найден при запуске** — убедитесь, что `Oniguruma.efi` находится в том же разделе, что и патчер. Либо удалите соответствующий функционал из конфигурации, если регулярные выражения не используются.
- **Проблемы с `BaseCryptLib`/`OpensslLib`** — убедитесь, что установлен NASM и его путь добавлен в `%PATH%`/`$PATH`.
- **`build.exe` не найден** — запустите `edksetup.bat Rebuild` или `make -C BaseTools` для генерации BaseTools.

После успешной сборки рекомендуется сохранить рабочее окружение (например, с помощью скрипта `SetEnv.cmd`), чтобы ускорить повторное восстановление.


