/** @file
  Build FV related hobs for platform.

  Copyright (c) 2019, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
  Copyright (c) 2006 - 2013, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "PiPei.h"
#include "Platform.h"
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PeiServicesLib.h>

/**
  Publish PEI & DXE (Decompressed) Memory based FVs to let PEI
  and DXE know about them.

  @retval EFI_SUCCESS   Platform PEI FVs were initialized successfully.

**/
EFI_STATUS
PeiFvInitialization (
  VOID
  )
{
  DEBUG ((DEBUG_INFO, "Platform PEI Firmware Volume Initialization\n"));
  //
  // Let DXE know about the DXE FV
  //
  BuildFvHob (PcdGet32 (PcdRiscVDxeFvBase), PcdGet32 (PcdRiscVDxeFvSize));
  DEBUG ((DEBUG_INFO, "Platform builds DXE FV at %x, size %x.\n",
    PcdGet32 (PcdRiscVDxeFvBase),
    PcdGet32 (PcdRiscVDxeFvSize)));

  //
  // Let PEI know about the DXE FV so it can find the DXE Core
  //
  PeiServicesInstallFvInfoPpi (
    NULL,
    (VOID *)(UINTN) PcdGet32 (PcdRiscVDxeFvBase),
    PcdGet32 (PcdRiscVDxeFvSize),
    NULL,
    NULL
    );

  //
  // Let DXE know about the ESP Ramdisk FV
  //
  BuildFvHob (PcdGet32 (PcdRiscVEmbeddedFvBase), PcdGet32 (PcdRiscVEmbeddedFvSize));
  DEBUG ((DEBUG_INFO, "Platform builds Embedded FV at %x, size %x.\n",
    PcdGet32 (PcdRiscVEmbeddedFvBase),
    PcdGet32 (PcdRiscVEmbeddedFvSize)));

  //
  // Let PEI know about the ESP Ramdisk FV so it can find the Embedded image
  //
  PeiServicesInstallFvInfoPpi (
    &gEfiFirmwareFileSystem3Guid,
    (VOID *)(UINTN) PcdGet32 (PcdRiscVEmbeddedFvBase),
    PcdGet32 (PcdRiscVEmbeddedFvSize),
    NULL,
    NULL
    );

  return EFI_SUCCESS;
}
