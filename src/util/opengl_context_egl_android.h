// SPDX-License-Identifier: GPL-3.0

#pragma once

#include "opengl_context_egl.h"

/// EGL on an Android surface. The window handle in the WindowInfo is the ANativeWindow*.
class ContextEGLAndroid final : public OpenGLContextEGL
{
public:
  ContextEGLAndroid(const WindowInfo& wi);
  ~ContextEGLAndroid() override;

  static std::unique_ptr<OpenGLContext> Create(const WindowInfo& wi, std::span<const Version> versions_to_try,
                                               Error* error);

  std::unique_ptr<OpenGLContext> CreateSharedContext(const WindowInfo& wi, Error* error) override;

protected:
  EGLDisplay GetPlatformDisplay(Error* error) override;
  EGLSurface CreatePlatformSurface(EGLConfig config, void* win, Error* error) override;
};
