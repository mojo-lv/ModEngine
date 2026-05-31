#pragma once

void ApplyMemoryPatch();
void PatchSaveFileCheck();

void PatchDebugMenuHook(uintptr_t hookAddress);
void PatchDebugNpcHook(uintptr_t hookAddress);
void PatchNpcDamageHook(uintptr_t hookAddress);
void PatchDbgCamNpcCtrlHook(uintptr_t hookAddress);
void PatchDbgCamFreeHook(uintptr_t hookAddress, bool arg2);

void PatchNpcAnimHook(uintptr_t hookAddress);
void PatchNpcAnimCancelHook(uintptr_t hookAddress);
void PatchNpcTurnHook(uintptr_t hookAddress);
void PatchNpcLifeHook(uintptr_t hookAddress);
