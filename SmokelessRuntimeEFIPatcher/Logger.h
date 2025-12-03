#pragma once

#include <Uefi.h>

#define LOGGER_LEVEL_ERROR  BIT0
#define LOGGER_LEVEL_WARN   BIT1
#define LOGGER_LEVEL_INFO   BIT2
#define LOGGER_LEVEL_DEBUG  BIT3

EFI_STATUS
LoggerInit(
  IN EFI_HANDLE ImageHandle
  );

EFI_STATUS
LoggerEnable(
  VOID
  );

VOID
LoggerShutdown(
  VOID
  );

VOID
LoggerSetMask(
  IN UINT32 LevelMask
  );

UINT32
LoggerGetMask(
  VOID
  );

BOOLEAN
LoggerIsEnabled(
  VOID
  );

VOID
LoggerWrite(
  IN UINT32 Level,
  IN CONST CHAR16 *Format,
  ...
  );

VOID
LoggerWriteRaw(
  IN UINT32 Level,
  IN CONST CHAR16 *String
  );

VOID
LogToFile(
  IN EFI_FILE *LegacyFileHandle,
  IN CHAR16 *String
  );

#define SREP_LOG_ERROR(...) LoggerWrite(LOGGER_LEVEL_ERROR, __VA_ARGS__)
#define SREP_LOG_WARN(...)  LoggerWrite(LOGGER_LEVEL_WARN, __VA_ARGS__)
#define SREP_LOG_INFO(...)  LoggerWrite(LOGGER_LEVEL_INFO, __VA_ARGS__)
#define SREP_LOG_DEBUG(...) LoggerWrite(LOGGER_LEVEL_DEBUG, __VA_ARGS__)


