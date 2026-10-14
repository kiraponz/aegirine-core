// SPDX-FileCopyrightText: 2026 Aegirine contributors
// SPDX-License-Identifier: GPL-3.0

// Audio output on Android through AAudio (API 26+). OpenSL ES is not provided.

#include "audio_stream.h"

#include "common/assert.h"
#include "common/error.h"
#include "common/log.h"

#include <aaudio/AAudio.h>

#include <algorithm>
#include <mutex>
#include <thread>

Log_SetChannel(AAudioStream);

namespace {
class AAudioAudioStream final : public AudioStream
{
public:
  AAudioAudioStream(u32 sample_rate, const AudioStreamParameters& parameters);
  ~AAudioAudioStream() override;

  void SetPaused(bool paused) override;

  bool Initialize(Error* error);

private:
  bool OpenStream(Error* error);
  void CloseStream();

  /// Opens the stream again on the new default device, after the old one went away (headphones
  /// unplugged, Bluetooth disconnected).
  void ReopenStream();

  static aaudio_data_callback_result_t DataCallback(AAudioStream* stream, void* user_data, void* audio_data,
                                                    int32_t num_frames);
  static void ErrorCallback(AAudioStream* stream, void* user_data, aaudio_result_t error);

  // Guards m_stream against the reopen thread.
  std::mutex m_stream_mutex;
  AAudioStream* m_stream = nullptr;
  std::thread m_reopen_thread;
  bool m_closing = false;
};
} // namespace

AAudioAudioStream::AAudioAudioStream(u32 sample_rate, const AudioStreamParameters& parameters)
  : AudioStream(sample_rate, parameters)
{
}

AAudioAudioStream::~AAudioAudioStream()
{
  {
    std::unique_lock lock(m_stream_mutex);
    m_closing = true;
  }
  if (m_reopen_thread.joinable())
    m_reopen_thread.join();

  std::unique_lock lock(m_stream_mutex);
  CloseStream();
}

bool AAudioAudioStream::Initialize(Error* error)
{
  BaseInitialize(&StereoSampleReaderImpl);

  std::unique_lock lock(m_stream_mutex);
  return OpenStream(error);
}

bool AAudioAudioStream::OpenStream(Error* error)
{
  DebugAssert(!m_stream);

  AAudioStreamBuilder* builder;
  aaudio_result_t result = AAudio_createStreamBuilder(&builder);
  if (result != AAUDIO_OK)
  {
    Error::SetStringFmt(error, "AAudio_createStreamBuilder() failed: {}", AAudio_convertResultToText(result));
    return false;
  }

  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
  AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_GAME);
  AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_MUSIC);
  AAudioStreamBuilder_setSampleRate(builder, static_cast<int32_t>(m_sample_rate));
  AAudioStreamBuilder_setChannelCount(builder, static_cast<int32_t>(m_output_channels));
  AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
  AAudioStreamBuilder_setDataCallback(builder, DataCallback, this);
  AAudioStreamBuilder_setErrorCallback(builder, ErrorCallback, this);

  result = AAudioStreamBuilder_openStream(builder, &m_stream);
  AAudioStreamBuilder_delete(builder);
  if (result != AAUDIO_OK)
  {
    m_stream = nullptr;
    Error::SetStringFmt(error, "AAudioStreamBuilder_openStream() failed: {}", AAudio_convertResultToText(result));
    return false;
  }

  // AAudio may not give the rate we asked for; it does not resample a shared stream in that case.
  const int32_t actual_rate = AAudioStream_getSampleRate(m_stream);
  if (actual_rate != static_cast<int32_t>(m_sample_rate))
  {
    Error::SetStringFmt(error, "AAudio opened at {} Hz instead of {} Hz", actual_rate, m_sample_rate);
    CloseStream();
    return false;
  }

  // Keep the device buffer near the configured latency, in whole bursts.
  const int32_t burst = std::max(AAudioStream_getFramesPerBurst(m_stream), 1);
  const u32 latency_ms = m_parameters.output_latency_minimal ? 0u : m_parameters.output_latency_ms;
  const int32_t wanted = static_cast<int32_t>(GetBufferSizeForMS(m_sample_rate, latency_ms));
  const int32_t bursts = std::max((wanted + burst - 1) / burst, 2);
  AAudioStream_setBufferSizeInFrames(m_stream, bursts * burst);
  DEV_LOG("AAudio stream: {} Hz, burst {} frames, buffer {} frames", actual_rate, burst,
          AAudioStream_getBufferSizeInFrames(m_stream));

  if (!m_paused)
  {
    result = AAudioStream_requestStart(m_stream);
    if (result != AAUDIO_OK)
    {
      Error::SetStringFmt(error, "AAudioStream_requestStart() failed: {}", AAudio_convertResultToText(result));
      CloseStream();
      return false;
    }
  }

  return true;
}

void AAudioAudioStream::CloseStream()
{
  if (!m_stream)
    return;

  AAudioStream_requestStop(m_stream);
  AAudioStream_close(m_stream);
  m_stream = nullptr;
}

void AAudioAudioStream::ReopenStream()
{
  std::unique_lock lock(m_stream_mutex);
  if (m_closing)
    return;

  CloseStream();

  Error error;
  if (!OpenStream(&error))
    ERROR_LOG("Could not reopen the audio stream: {}", error.GetDescription());
}

void AAudioAudioStream::SetPaused(bool paused)
{
  std::unique_lock lock(m_stream_mutex);
  if (m_paused == paused)
    return;

  m_paused = paused;
  if (!m_stream)
    return;

  const aaudio_result_t result = paused ? AAudioStream_requestPause(m_stream) : AAudioStream_requestStart(m_stream);
  if (result != AAUDIO_OK)
    ERROR_LOG("Could not {} the audio stream: {}", paused ? "pause" : "start", AAudio_convertResultToText(result));
}

aaudio_data_callback_result_t AAudioAudioStream::DataCallback(AAudioStream* stream, void* user_data,
                                                              void* audio_data, int32_t num_frames)
{
  AAudioAudioStream* const this_ptr = static_cast<AAudioAudioStream*>(user_data);
  this_ptr->ReadFrames(static_cast<SampleType*>(audio_data), static_cast<u32>(num_frames));
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioAudioStream::ErrorCallback(AAudioStream* stream, void* user_data, aaudio_result_t error)
{
  WARNING_LOG("Audio stream error: {}", AAudio_convertResultToText(error));
  if (error != AAUDIO_ERROR_DISCONNECTED)
    return;

  // The stream cannot be closed from its own callback, so another thread opens it again.
  AAudioAudioStream* const this_ptr = static_cast<AAudioAudioStream*>(user_data);
  std::thread previous;
  {
    std::unique_lock lock(this_ptr->m_stream_mutex);
    if (this_ptr->m_closing)
      return;
    previous = std::move(this_ptr->m_reopen_thread);
  }

  // The thread that opened this stream; it has finished, or is about to.
  if (previous.joinable())
    previous.join();

  std::unique_lock lock(this_ptr->m_stream_mutex);
  if (!this_ptr->m_closing)
    this_ptr->m_reopen_thread = std::thread([this_ptr]() { this_ptr->ReopenStream(); });
}

std::unique_ptr<AudioStream> AudioStream::CreateAAudioAudioStream(u32 sample_rate,
                                                                const AudioStreamParameters& parameters, Error* error)
{
  std::unique_ptr<AAudioAudioStream> stream = std::make_unique<AAudioAudioStream>(sample_rate, parameters);
  if (!stream->Initialize(error))
    stream.reset();

  return stream;
}

std::unique_ptr<AudioStream> AudioStream::CreateOpenSLESAudioStream(u32 sample_rate,
                                                                  const AudioStreamParameters& parameters,
                                                                  Error* error)
{
  Error::SetStringView(error, "OpenSL ES is not supported; use AAudio");
  return {};
}
