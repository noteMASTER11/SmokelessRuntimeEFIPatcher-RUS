#include "Logger.h"

#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/ShellParameters.h>

#define LOGGER_MESSAGE_BUFFER  0x400
#define LOGGER_FILE_NAME       L"SREP_DEBUG.log"

STATIC BOOLEAN                             mLoggerInitialized = FALSE;
STATIC BOOLEAN                             mLoggerEnabled     = FALSE;
STATIC EFI_HANDLE                          mLoggerImageHandle = NULL;
STATIC EFI_FILE_PROTOCOL                   *mLoggerFile       = NULL;
STATIC EFI_SHELL_PARAMETERS_PROTOCOL       *mShellParameters  = NULL;
STATIC UINT32                              mLoggerLevelMask   = LOGGER_LEVEL_ERROR | LOGGER_LEVEL_WARN | LOGGER_LEVEL_INFO;
STATIC UINT32                              mConsoleLevelMask  = 0;

STATIC
BOOLEAN
StringEqualsInsensitive(
  IN CONST CHAR16 *Left,
  IN CONST CHAR16 *Right
  )
{
  if ((Left == NULL) || (Right == NULL)) {
    return FALSE;
  }

  while ((*Left != L'\0') && (*Right != L'\0')) {
    CHAR16 LeftChar  = ((*Left >= L'a') && (*Left <= L'z')) ? (CHAR16)(*Left - (L'a' - L'A')) : *Left;
    CHAR16 RightChar = ((*Right >= L'a') && (*Right <= L'z')) ? (CHAR16)(*Right - (L'a' - L'A')) : *Right;

    if (LeftChar != RightChar) {
      return FALSE;
    }

    Left++;
    Right++;
  }

  return (*Left == L'\0') && (*Right == L'\0');
}

STATIC
VOID
LoggerDispatch(
  IN UINT32 Level,
  IN CONST CHAR16 *Message
  )
{
  if (Message == NULL) {
    return;
  }

  BOOLEAN ShouldPrint = FALSE;

  if ((Level & LOGGER_LEVEL_DEBUG) != 0) {
    ShouldPrint = mLoggerEnabled && ((mConsoleLevelMask & LOGGER_LEVEL_DEBUG) != 0);
  } else if ((Level & mConsoleLevelMask) != 0) {
    ShouldPrint = TRUE;
  }

  if (ShouldPrint) {
    Print(L"%s", Message);
  }

  if (!mLoggerEnabled || (mLoggerFile == NULL)) {
    return;
  }

  if ((Level & mLoggerLevelMask) == 0) {
    return;
  }

  UINTN SizeInBytes = StrLen(Message) * sizeof(CHAR16);
  if (SizeInBytes == 0) {
    return;
  }

  EFI_STATUS Status = mLoggerFile->Write(mLoggerFile, &SizeInBytes, (VOID *)Message);
  if (!EFI_ERROR(Status)) {
    mLoggerFile->Flush(mLoggerFile);
  }
}

STATIC
BOOLEAN
ShouldEnableFromValue(
  IN CONST CHAR16 *Value
  )
{
  if ((Value == NULL) || (*Value == L'\0')) {
    return FALSE;
  }

  if ((*Value == L'0') && (*(Value + 1) == L'\0')) {
    return FALSE;
  }

  if (StringEqualsInsensitive(Value, L"false") ||
      StringEqualsInsensitive(Value, L"off")   ||
      StringEqualsInsensitive(Value, L"disable") ||
      StringEqualsInsensitive(Value, L"disabled"))
  {
    return FALSE;
  }

  return TRUE;
}

STATIC
BOOLEAN
ParseEnvironmentFlag(
  VOID
  )
{
  if ((mShellParameters == NULL) || (mShellParameters->Env == NULL)) {
    return FALSE;
  }

  for (CHAR16 **Entry = mShellParameters->Env; *Entry != NULL; Entry++) {
    CONST CHAR16 *Variable = *Entry;
    CONST CHAR16 *Name     = L"SREP_DEBUG=";
    UINTN        PrefixLen = StrLen(Name);

    if (StrnCmp(Variable, Name, PrefixLen) == 0) {
      CONST CHAR16 *Value = Variable + PrefixLen;
      return ShouldEnableFromValue(Value);
    }
  }

  return FALSE;
}

STATIC
BOOLEAN
ParseCommandLineFlag(
  VOID
  )
{
  if ((mShellParameters == NULL) || (mShellParameters->Argc == 0) || (mShellParameters->Argv == NULL)) {
    return FALSE;
  }

  for (UINTN Index = 1; Index < mShellParameters->Argc; Index++) {
    CONST CHAR16 *Argument = mShellParameters->Argv[Index];
    if (Argument == NULL) {
      continue;
    }

    if (StringEqualsInsensitive(Argument, L"-d")     ||
        StringEqualsInsensitive(Argument, L"/d")     ||
        StringEqualsInsensitive(Argument, L"--debug"))
    {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
EFI_STATUS
OpenLogFile(
  VOID
  )
{
  EFI_LOADED_IMAGE_PROTOCOL          *LoadedImage;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL    *FileSystem;
  EFI_FILE_PROTOCOL                  *Root;
  EFI_STATUS                         Status;

  LoadedImage = NULL;
  FileSystem  = NULL;
  Root        = NULL;

  Status = gBS->HandleProtocol(
                  mLoggerImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = gBS->HandleProtocol(
                  LoadedImage->DeviceHandle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID **)&FileSystem
                  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = FileSystem->OpenVolume(FileSystem, &Root);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = Root->Open(
                  Root,
                  &mLoggerFile,
                  LOGGER_FILE_NAME,
                  EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                  0
                  );
  if (EFI_ERROR(Status)) {
    Root->Close(Root);
    return Status;
  }

  //
  // Truncate the file by deleting and recreating it.
  //
  mLoggerFile->Delete(mLoggerFile);

  Status = Root->Open(
                  Root,
                  &mLoggerFile,
                  LOGGER_FILE_NAME,
                  EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                  0
                  );

  Root->Close(Root);

  return Status;
}

EFI_STATUS
LoggerInit(
  IN EFI_HANDLE ImageHandle
  )
{
  if (mLoggerInitialized) {
    return EFI_SUCCESS;
  }

  mLoggerImageHandle = ImageHandle;
  mLoggerInitialized = TRUE;

  if (ImageHandle != NULL) {
    EFI_STATUS Status = gBS->OpenProtocol(
                            ImageHandle,
                            &gEfiShellParametersProtocolGuid,
                            (VOID **)&mShellParameters,
                            ImageHandle,
                            NULL,
                            EFI_OPEN_PROTOCOL_GET_PROTOCOL
                            );
    if (EFI_ERROR(Status)) {
      mShellParameters = NULL;
    }
  }

  BOOLEAN EnableDebug = ParseCommandLineFlag() || ParseEnvironmentFlag();

  if (EnableDebug) {
    LoggerSetMask(mLoggerLevelMask | LOGGER_LEVEL_DEBUG);
    EFI_STATUS Status = LoggerEnable();
    if (!EFI_ERROR(Status)) {
      LoggerWriteRaw(LOGGER_LEVEL_INFO, L"[LOGGER] DEBUG logging enabled automatically.\n\r");
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
LoggerEnable(
  VOID
  )
{
  if (mLoggerEnabled && (mLoggerFile != NULL)) {
    return EFI_SUCCESS;
  }

  if (!mLoggerInitialized) {
    return EFI_NOT_READY;
  }

  EFI_STATUS Status = OpenLogFile();
  if (EFI_ERROR(Status)) {
    return Status;
  }

  mLoggerEnabled    = TRUE;
  mConsoleLevelMask |= LOGGER_LEVEL_DEBUG;
  LoggerSetMask(mLoggerLevelMask | LOGGER_LEVEL_DEBUG);

  return EFI_SUCCESS;
}

VOID
LoggerShutdown(
  VOID
  )
{
  if (mLoggerFile != NULL) {
    mLoggerFile->Close(mLoggerFile);
    mLoggerFile = NULL;
  }

  mLoggerEnabled = FALSE;
}

VOID
LoggerSetMask(
  IN UINT32 LevelMask
  )
{
  mLoggerLevelMask = LevelMask;
}

UINT32
LoggerGetMask(
  VOID
  )
{
  return mLoggerLevelMask;
}

BOOLEAN
LoggerIsEnabled(
  VOID
  )
{
  return mLoggerEnabled;
}

VOID
LoggerWrite(
  IN UINT32 Level,
  IN CONST CHAR16 *Format,
  ...
  )
{
  if (Format == NULL) {
    return;
  }

  CHAR16  Buffer[LOGGER_MESSAGE_BUFFER];
  VA_LIST Marker;

  VA_START(Marker, Format);
  UnicodeVSPrint(Buffer, sizeof(Buffer), Format, Marker);
  VA_END(Marker);

  LoggerDispatch(Level, Buffer);
}

VOID
LoggerWriteRaw(
  IN UINT32 Level,
  IN CONST CHAR16 *String
  )
{
  LoggerDispatch(Level, String);
}

VOID
LogToFile(
  IN EFI_FILE *LegacyFileHandle,
  IN CHAR16 *String
  )
{
  (VOID)LegacyFileHandle;
  LoggerWriteRaw(LOGGER_LEVEL_DEBUG, String);
}


