/** @file
  TODO

  Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

// TODO: Remove unnecessary headers
#include <Uefi.h>

#include <Guid/MemoryProfile.h>

#include <Library/BaseLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/HiiLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeCoffLib.h>
#include <Library/ShellCommandLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <PiDxe.h>

#include <Protocol/DeviceIo.h>
#include <Protocol/RamDisk.h>

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiSpec.h>


typedef enum  {
  ImageTypeVirtualCd,
  ImageTypeVirtualDisk,
} EMBEDDED_RAMDISK_TYPE_TYPE;

STATIC CONST CHAR16         mFileName[] = L"EmbeddedRamdiskCommand";
STATIC EFI_HII_HANDLE       gLinuxEmbeddedRamdiskShellCommandHiiHandle;
STATIC CONST SHELL_PARAM_ITEM ParamList[] = {
  {NULL, TypeMax}
  };

// Taken from NetworkPkg/HttpBootDxe/HttpBootSupport.c
/**
  This function register the RAM disk info to the system.

  @param[in]       BufferSize      The size of Buffer in bytes.
  @param[in]       Buffer          The base address of the RAM disk.
  @param[in]       ImageType       The image type of the file in Buffer.

  @retval EFI_SUCCESS              The RAM disk has been registered.
  @retval EFI_NOT_FOUND            No RAM disk protocol instances were found.
  @retval EFI_UNSUPPORTED          The ImageType is not supported.
  @retval Others                   Unexpected error happened.

**/
EFI_STATUS
HttpBootRegisterRamDisk (
  IN  UINTN                        BufferSize,
  IN  VOID                        *Buffer,
  IN  EMBEDDED_RAMDISK_TYPE_TYPE   ImageType
  )
{
  EFI_RAM_DISK_PROTOCOL      *RamDisk;
  EFI_STATUS                 Status;
  EFI_DEVICE_PATH_PROTOCOL   *DevicePath;
  EFI_GUID                   *RamDiskType;

  ASSERT (Buffer != NULL);
  ASSERT (BufferSize != 0);

  Status = gBS->LocateProtocol (&gEfiRamDiskProtocolGuid, NULL, (VOID**) &RamDisk);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "HTTP Boot: Couldn't find the RAM Disk protocol - %r\n", Status));
    return Status;
  }

  if (ImageType == ImageTypeVirtualCd) {
    RamDiskType = &gEfiVirtualCdGuid;
  } else if (ImageType == ImageTypeVirtualDisk) {
    RamDiskType = &gEfiVirtualDiskGuid;
  } else {
    return EFI_UNSUPPORTED;
  }

  Status = RamDisk->Register (
             (UINTN)Buffer,
             (UINT64)BufferSize,
             RamDiskType,
             NULL,
             &DevicePath
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "HTTP Boot: Failed to register RAM Disk - %r\n", Status));
  }

  return Status;
}

/**
 * Mount the ramdisk
 *
 * Mostly taken from MdeModulePkg/Universal/Acpi/BootScriptExecutorDxe/ScriptExecute.c
 *
 * @param[in] Guid The GUID of the RAM disk file.
 *
 * @retval EFI_SUCCESS   The RAM disk was successfully mounted.
 * @retval EFI_NOT_FOUND The file with the given GUID was not found.
 * TODO: Asserts else
**/
EFI_STATUS
EFIAPI
MountRamdisk (
  IN  EFI_GUID Guid
  )
{
  EFI_STATUS                                    Status;
  UINT8                                         *Buffer;
  UINTN                                         BufferSize;
  EFI_HANDLE                                    NewImageHandle;

  //
  // A workaround: Here we install a dummy handle
  //
  NewImageHandle = NULL;
  Status = gBS->InstallProtocolInterface (
                  &NewImageHandle,
                  &Guid,
                  EFI_NATIVE_INTERFACE,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Load BootScriptExecutor image itself to RESERVED mem
  //
  Status = GetSectionFromAnyFv  (
             &Guid,
             EFI_SECTION_RAW,
             0,
             (VOID **) &Buffer,
             &BufferSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Loading image of size %d into ramdisk\n", BufferSize));
    return EFI_NOT_FOUND;
  }

  DEBUG ((EFI_D_INFO, "Loading image of size %d into ramdisk\n", BufferSize));

  Status = HttpBootRegisterRamDisk(BufferSize, Buffer, ImageTypeVirtualCd);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  Function for 'execembedded' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunEmbeddedRamdisk (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  UINTN               Size;
  LIST_ENTRY         *Package;
  CHAR16             *ProblemParam;
  SHELL_STATUS        ShellStatus;
  CONST CHAR16       *GuidStr;
  EFI_GUID            Guid;

  ShellStatus = SHELL_SUCCESS;
  Size = 1;

  //
  // Initialize the shell lib (we must be in non-auto-init...)
  //
  Status = ShellInitialize();
  ASSERT_EFI_ERROR(Status);

  //
  // Parse arguments
  //
  Status = ShellCommandLineParse (ParamList, &Package, &ProblemParam, TRUE);
  if (EFI_ERROR (Status)) {
    if (Status == EFI_VOLUME_CORRUPTED && ProblemParam != NULL) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GEN_PROBLEM), gLinuxEmbeddedRamdiskShellCommandHiiHandle, L"execembedded", ProblemParam);
      FreePool (ProblemParam);
      ShellStatus = SHELL_INVALID_PARAMETER;
      goto Done;
    } else {
      ASSERT (FALSE);
    }
  } else {
    if (ShellCommandLineGetCount(Package) > 2) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GEN_NO_VALUE), gLinuxEmbeddedRamdiskShellCommandHiiHandle, L"execembedded");
      ShellStatus = SHELL_INVALID_PARAMETER;
      goto Done;
    }

    GuidStr = ShellCommandLineGetRawValue(Package, 1);
    if (GuidStr == NULL) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GEN_PARAM_INV), gLinuxEmbeddedRamdiskShellCommandHiiHandle, L"execembedded");
      ShellStatus = SHELL_INVALID_PARAMETER;
      goto Done;
    }

    ShellPrintEx (-1, -1, L"Selected GUID to mount: %s\r\n", GuidStr);

    StrToGuid (GuidStr, &Guid);

    ShellPrintEx (-1, -1, L"Mounting selected section as ramdisk...\r\n");
    Status = MountRamdisk (Guid);
    if (EFI_ERROR (Status)) {
      if (Status == EFI_NOT_FOUND) {
        ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GEN_NOT_FOUND),
          gLinuxEmbeddedRamdiskShellCommandHiiHandle, L"execembedded", GuidStr);
        ShellStatus = SHELL_INVALID_PARAMETER;
        goto Done;
      } else {
        ASSERT (FALSE);
      }
    } else {
      ShellPrintEx (-1, -1, L"Successfully mounted the ramdisk. Use 'map -r' map it.\r\n");
    }

  }
  ASSERT (ShellStatus == SHELL_SUCCESS);

Done:
  if (Package != NULL) {
    ShellCommandLineFreeVarList (Package);
  }
  return ShellStatus;
}

/**
  Get the filename to get help text from if not using HII.

  @retval The filename.
**/
STATIC
CONST CHAR16*
EFIAPI
ShellCommandGetManFileNameEmbeddedRamdisk (
  VOID
  )
{
  return mFileName;
}

/**
  Constructor for the 'initrd' UEFI Shell command library

  @param ImageHandle    the image handle of the process
  @param SystemTable    the EFI System Table pointer

  @retval EFI_SUCCESS        the shell command handlers were installed sucessfully
  @retval EFI_UNSUPPORTED    the shell level required was not found.
**/
EFI_STATUS
EFIAPI
EmbeddedRamdiskCommandLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  //
  // Install the HII stuff
  //
  gLinuxEmbeddedRamdiskShellCommandHiiHandle = HiiAddPackages (
    &gShellEmbeddedRamdiskHiiGuid,
    gImageHandle, EmbeddedRamdiskCommandLibStrings, NULL);
  if (gLinuxEmbeddedRamdiskShellCommandHiiHandle == NULL) {
    return EFI_DEVICE_ERROR;
  }

  //
  // Install shell command handler
  //
  ShellCommandRegisterCommandName (L"embeddedramdisk", ShellCommandRunEmbeddedRamdisk,
    ShellCommandGetManFileNameEmbeddedRamdisk, 0, L"embeddedramdisk", TRUE,
    gLinuxEmbeddedRamdiskShellCommandHiiHandle,
    STRING_TOKEN(STR_GET_HELP_EMBEDDED_RAMDISK));

  return EFI_SUCCESS;
}

/**
  Destructor for the library.  free any resources.

  @param ImageHandle    The image handle of the process.
  @param SystemTable    The EFI System Table pointer.

  @retval EFI_SUCCESS   Always returned.
**/
EFI_STATUS
EFIAPI
EmbeddedRamdiskCommandLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (gLinuxEmbeddedRamdiskShellCommandHiiHandle != NULL) {
    HiiRemovePackages (gLinuxEmbeddedRamdiskShellCommandHiiHandle);
  }

  return EFI_SUCCESS;
}
