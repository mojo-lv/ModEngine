#pragma once

void ApplyMemoryPatch();
void PatchSaveFileCheck();
void PatchDebugMenu(uintptr_t hookAddress);
void PatchNpcAnimHook(uintptr_t hookAddress);
void PatchDebugNPC(uintptr_t hookAddress);
