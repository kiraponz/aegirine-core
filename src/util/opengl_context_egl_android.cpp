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
  // Android has the one display. Going through eglGetPlatformDisplayEXT gives a display that
  // eglInitialize refuses (EGL_BAD_DISPLAY).
  EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (dpy == EGL_NO_DISPLAY)
  {
    const EGLint err = eglGetError();
    Error::SetStringFmt(error, "eglGetDisplay() failed: {} (0x{:X})", err, err);
  }

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
