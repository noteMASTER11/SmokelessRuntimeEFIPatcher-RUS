#ifndef __EFI_DISPLAY_PROTOCOL_H__
#define __EFI_DISPLAY_PROTOCOL_H__

/**
  Minimal stub of the legacy EFI Display Protocol definition.
  The original interface lived in Intel's EDK/UDK 1.x tree and is
  required only for compilation of the current project. The implementation
  of the protocol is not needed at build time, therefore the structure is
  left intentionally empty.
**/

#include <Uefi.h>

#define EFI_DISPLAY_PROTOCOL_GUID \
  { 0x3b9d0f4c, 0x3f78, 0x4b2b, { 0x8a, 0x07, 0xd1, 0x6f, 0xcc, 0x4e, 0xc4, 0x0b } }

typedef struct _EFI_DISPLAY_PROTOCOL EFI_DISPLAY_PROTOCOL;

struct _EFI_DISPLAY_PROTOCOL {
  UINT64 Reserved; // Placeholder to keep structure non-empty
};

#endif // __EFI_DISPLAY_PROTOCOL_H__

