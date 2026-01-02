#pragma once

void ApplyMemoryPatch(size_t targetOffset, const std::vector<BYTE>& patchBytes);
void PatchMemory();
