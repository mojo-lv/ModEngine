#pragma once

void ApplyMemoryPatch();
void PatchSaveFileCheck();
void PatchDebugMenuHook(uintptr_t hookAddress);
void PatchDebugNPCHook(uintptr_t hookAddress);
void PatchNPCDamageHook(uintptr_t hookAddress);
void PatchNpcAnimHook(uintptr_t hookAddress);
void PatchNpcAnimCancelHook(uintptr_t hookAddress);
