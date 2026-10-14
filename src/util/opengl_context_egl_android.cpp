// SPDX-License-Identifier: GPL-3.0

#include "opengl_context_egl_android.h"

#include "common/error.h"
#include "common/log.h"

#include <android/native_window.h>

Log_SetChannel(OpenGLContext);

ContextEGLAndroid::ContextEGLAndroid(const WindowInfo& wi) : OpenGLContextEGL(wi)
{
}

ContextEGLAndroid::~ContextEGLAndroid() = default;

std::unique_ptr<OpenGLContext> ContextEGLAndroid::Create(const WindowInfo& wi, std::span<const Version> versions_to_try,
                                                         Error* error)
{
  std::unique_ptr<ContextEGLAndroid> context = std::make_unique<ContextEGLAndroid>(wi);
  if (!context->Initialize(versions_to_try, error))
    return nullptr;

  return context;
}

std::unique_ptr<OpenGLContext> ContextEGLAndroid::CreateSharedContext(const WindowInfo& wi, Error* error)
{
  std::unique_ptr<ContextEGLAndroid> context = std::make_unique<ContextEGLAndroid>(wi);
  context->m_display = m_display;

  if (!context->CreateContextAndSurface(m_version, m_context, false))
    return nullptr;

  return context;
}

EGLDisplay ContextEGLAndroid::GetPlatformDisplay(Error* error)
{
  EGLDisplay dpy = TryGetPlatformDisplay(EGL_PLATFORM_ANDROID_KHR, "EGL_KHR_platform_android");
  if (dpy == EGL_NO_DISPLAY)
    dpy = GetFallbackDisplay(error);

  return dpy;
}

EGLSurface ContextEGLAndroid::CreatePlatformSurface(EGLConfig config, void* win, Error* error)
{
  // The window's buffers have to be in the config's format before a surface goes on it.
  EGLint format;
  if (eglGetConfigAttrib(m_display, config, EGL_NATIVE_VISUAL_ID, &format))
    ANativeWindow_setBuffersGeometry(static_cast<ANativeWindow*>(win), 0, 0, format);
  else
    WARNING_LOG("Could not read the config's native visual ID");

  return CreateFallbackSurface(config, win, error);
}
