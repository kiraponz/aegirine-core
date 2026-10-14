// SPDX-License-Identifier: GPL-3.0

#include "platform_misc.h"

#include <unistd.h>

// The Android frontend keeps the screen on and plays sounds itself.

bool PlatformMisc::InitializeSocketSupport(Error* error)
{
  return true;
}

void PlatformMisc::SuspendScreensaver()
{
}

void PlatformMisc::ResumeScreensaver()
{
}

size_t PlatformMisc::GetRuntimePageSize()
{
  const long res = sysconf(_SC_PAGESIZE);
  return (res > 0) ? static_cast<size_t>(res) : 0;
}

bool PlatformMisc::PlaySoundAsync(const char* path)
{
  return false;
}
