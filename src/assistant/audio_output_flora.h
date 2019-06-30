#ifndef SRC_ASSISTANT_AUDIO_OUTPUT_FLORA_H_
#define SRC_ASSISTANT_AUDIO_OUTPUT_FLORA_H_

#include <flora-agent.h>

#include <condition_variable>  // NOLINT
#include <memory>
#include <mutex>   // NOLINT
#include <thread>  // NOLINT
#include <vector>

// Audio output using Flora.
class AudioOutputFlora {
 public:
  AudioOutputFlora(flora::Agent* agent) : agent(agent) {};

  bool Start();

  void Stop();

  void Send(const std::string& data);

  friend void fill_audio(void* userdata, unsigned char* stream, int len);

 private:
  flora::Agent* agent;
  std::string outputChannelName;
  std::vector<uint8_t> audioData;
};

#endif  // SRC_ASSISTANT_AUDIO_OUTPUT_FLORA_H_
