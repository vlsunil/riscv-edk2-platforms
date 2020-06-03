/** @file
The module to pass the device tree to DXE via HOB.

Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <libfdt.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/RiscVEdk2SbiLib.h>
#include <IndustryStandard/RiscVOpensbi.h>

/**
  TODO
**/
EFI_STATUS
EFIAPI
BuildFdtHob (
  IN  UINT64 FdtPtr
  )
{
  VOID *HobData;

  if (fdt_check_header ((VOID*)FdtPtr) != 0) {
    DEBUG ((DEBUG_INFO, "FDT header is invalid\n"));
    return EFI_LOAD_ERROR;
  }

  DEBUG ((DEBUG_INFO, "Installing FDT Hob with length: %d\n", fdt_totalsize(FdtPtr)));
  HobData = BuildGuidDataHob (
              &gRiscVFlattenedDeviceTreeHobGuid,
              (VOID *)FdtPtr,
              fdt_totalsize(FdtPtr)
              );
  if (HobData == NULL) {
    return EFI_LOAD_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  The entrypoint of the module, it will pass the FDT via a HOB.

  @param  FileHandle             Handle of the file being invoked.
  @param  PeiServices            Describes the list of possible PEI Services.

  @retval TODO
**/
EFI_STATUS
EFIAPI
PeimPassFdt (
  IN EFI_PEI_FILE_HANDLE        FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                          Status;
  EFI_RISCV_OPENSBI_FIRMWARE_CONTEXT *FirmwareContext;

  Status = SbiGetFirmwareContext (&FirmwareContext);
  ASSERT_EFI_ERROR (Status);

  Status = BuildFdtHob (FirmwareContext->FlattenedDeviceTree);
  ASSERT_EFI_ERROR (Status);

	return EFI_SUCCESS;
}
