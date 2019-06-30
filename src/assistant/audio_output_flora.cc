#include "audio_output_flora.h"

#include <iostream>

#include <caps.h>

bool AudioOutputFlora::Start() {
  std::shared_ptr<Caps> url_msg = Caps::new_instance();
  url_msg->write("yoda-app://cloud-player/play-tts-stream");
  flora::Response resp;
  std::clog << "open cloud-player ... " << std::endl;
  auto retCode = agent->call("yodaos.runtime.open-url-format", url_msg, "runtime", resp, 60 * 1000);
  std::clog << "opened cloud-player with code " << retCode << std::endl;
  if (retCode != 0) {
    return false;
  }
  std::shared_ptr<Caps> get_stream_msg = Caps::new_instance();
  flora::Response get_stream_resp;

  uint8_t retry = 0;
  while (true) {
    std::clog << "query output stream ..." << std::endl;
    retCode = agent->call("yodaos.apps.cloud-player.get-stream-channel", get_stream_msg, "cloud-player", get_stream_resp, 60 * 1000);
    std::clog << "queried output stream with code " << retCode << std::endl;
    if (retCode == 0) {
      retCode = get_stream_resp.data->read(outputChannelName);
      std::clog << "queried output stream: " << outputChannelName << std::endl;
      if (retCode == 0) {
        return true;
      }
    }
    retry++;
  }
  return false;
}

void AudioOutputFlora::Stop() {
  if (outputChannelName.empty()) {
    return;
  }
  std::shared_ptr<Caps> data_msg = Caps::new_instance();
  data_msg->write(1);
  agent->post(outputChannelName.c_str(), data_msg);
  std::clog << "stop output" << std::endl;
}

void AudioOutputFlora::Send(const std::string& data) {
  std::copy(data.begin(), data.end(), std::back_inserter(audioData));
  if (audioData.size() < 8000) {
    return;
  }
  std::shared_ptr<Caps> data_msg = Caps::new_instance();
  data_msg->write(0);
  data_msg->write(audioData);
  std::clog << "write data " << audioData.size() << std::endl;
  agent->post(outputChannelName.c_str(), data_msg);
  audioData.clear();
}
