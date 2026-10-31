#pragma once

#include "ConsoleBuffer.h"

namespace remote_console {
// Installs the IDF sink; Arduino's diagnostics wrapper calls our wrapped log_printfv.
void begin();
void setEnabled(bool enabled);
bool pop(Record &record);
void pushRemote(const Record &record);
void noteDropped(uint32_t count = 1);
} // namespace remote_console
