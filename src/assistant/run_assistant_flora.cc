/*
Copyright 2017 Google Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <grpc++/grpc++.h>
#include <flora-agent.h>

#include <getopt.h>

#include <chrono>  // NOLINT
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>  // NOLINT

#include "google/assistant/embedded/v1alpha2/embedded_assistant.grpc.pb.h"
#include "google/assistant/embedded/v1alpha2/embedded_assistant.pb.h"

#include "assistant/audio_output_flora.h"
#include "assistant/assistant_config.h"
#include "assistant/base64_encode.h"
#include "assistant/json_util.h"

namespace assistant = google::assistant::embedded::v1alpha2;

using assistant::AssistRequest;
using assistant::AssistResponse;
using assistant::AssistResponse_EventType_END_OF_UTTERANCE;
using assistant::AudioInConfig;
using assistant::AudioOutConfig;
using assistant::EmbeddedAssistant;
using assistant::ScreenOutConfig;

using grpc::CallCredentials;
using grpc::Channel;
using grpc::ClientReaderWriter;

static const char kCredentialsTypeUserAccount[] = "USER_ACCOUNT";
static const char kLanguageCode[] = "en-US";
static const char kDeviceModelId[] = "default";
static const char kDeviceInstanceId[] = "default";

bool verbose = false;

// Creates a channel to be connected to Google.
std::shared_ptr<Channel> CreateChannel(const std::string& host) {
  std::ifstream file("robots.pem");
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string roots_pem = buffer.str();

  if (verbose) {
    std::clog << "assistant_sdk robots_pem: " << roots_pem << std::endl;
  }
  ::grpc::SslCredentialsOptions ssl_opts = {roots_pem, "", ""};
  auto creds = ::grpc::SslCredentials(ssl_opts);
  std::string server = host + ":443";
  if (verbose) {
    std::clog << "assistant_sdk CreateCustomChannel(" << server
              << ", creds, arg)" << std::endl
              << std::endl;
  }
  ::grpc::ChannelArguments channel_args;
  return CreateCustomChannel(server, creds, channel_args);
}

std::shared_ptr<CallCredentials> ReadCredentialsFile(const std::string& credentials_file_path) {
  std::ifstream credentials_file(credentials_file_path);
  if (!credentials_file) {
    std::cerr << "Credentials file \"" << credentials_file_path
              << "\" does not exist." << std::endl;
    return nullptr;
  }
  std::stringstream credentials_buffer;
  credentials_buffer << credentials_file.rdbuf();
  std::string credentials = credentials_buffer.str();
  std::shared_ptr<CallCredentials> call_credentials;
  call_credentials = grpc::GoogleRefreshTokenCredentials(credentials);
  if (call_credentials.get() == nullptr) {
    std::cerr << "Credentials file \"" << credentials_file_path
              << "\" is invalid. Check step 5 in README for how to get valid "
              << "credentials." << std::endl;
    return nullptr;
  }
  return call_credentials;
}

void PrintUsage() {
  std::cerr << "Usage: ./run_assistant_flora "
            << "--url <url> "
            << "--credentials <credentials_file> "
            << "[--api_endpoint <API endpoint>] "
            << "[--locale <locale>]" << std::endl;
}

bool GetCommandLineFlags(int argc, char** argv,
                         std::string* url,
                         std::string* credentials_file_path,
                         std::string* api_endpoint, std::string* locale) {
  const struct option long_options[] = {
      {"url", required_argument, nullptr, 'u'},
      {"credentials", required_argument, nullptr, 'c'},
      {"api_endpoint", required_argument, nullptr, 'e'},
      {"locale", required_argument, nullptr, 'l'},
      {"verbose", no_argument, nullptr, 'v'},
      {nullptr, 0, nullptr, 0}};
  *api_endpoint = ASSISTANT_ENDPOINT;
  while (true) {
    int option_index;
    int option_char =
        getopt_long(argc, argv, "u:c:e:l:v", long_options, &option_index);
    if (option_char == -1) {
      break;
    }
    switch (option_char) {
      case 'u':
        *url = optarg;
        break;
      case 'c':
        *credentials_file_path = optarg;
        break;
      case 'e':
        *api_endpoint = optarg;
        break;
      case 'l':
        *locale = optarg;
        break;
      case 'v':
        verbose = true;
        break;
      default:
        PrintUsage();
        return false;
    }
  }
  return true;
}

int main(int argc, char** argv) {
  std::string url, credentials_file_path, api_endpoint, locale, text_input_source;
  // Initialize gRPC and DNS resolvers
  // https://github.com/grpc/grpc/issues/11366#issuecomment-328595941
  grpc_init();
  if (!GetCommandLineFlags(argc, argv, &url, &credentials_file_path,
                           &api_endpoint, &locale)) {
    return -1;
  }
  if (locale.empty()) {
    locale = kLanguageCode;  // Default locale
  }

  auto floraAgent = new flora::Agent();
  floraAgent->config(FLORA_AGENT_CONFIG_URI, url.c_str());
  floraAgent->start();

  std::shared_ptr<ClientReaderWriter<AssistRequest, AssistResponse>> gstream;
  AssistRequest* greq;
  bool voiceStarted = false;
  int32_t turenId;

  std::mutex m;
  std::condition_variable cv;
  auto thd = new std::thread([&]() {
    while (true) {
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk);

      AssistRequest request;
      greq = &request;
      auto* assist_config = request.mutable_config();

      if (verbose) {
        std::clog << "Using locale " << locale << std::endl;
      }
      // Set the DialogStateIn of the AssistRequest
      assist_config->mutable_dialog_state_in()->set_language_code(locale);

      // Set the DeviceConfig of the AssistRequest
      assist_config->mutable_device_config()->set_device_id(kDeviceInstanceId);
      assist_config->mutable_device_config()->set_device_model_id(kDeviceModelId);

      // Set parameters for audio output
      assist_config->mutable_audio_out_config()->set_encoding(
          AudioOutConfig::LINEAR16);
      assist_config->mutable_audio_out_config()->set_sample_rate_hertz(24000);

      // Set the AudioInConfig of the AssistRequest
      assist_config->mutable_audio_in_config()->set_encoding(
          AudioInConfig::LINEAR16);
      assist_config->mutable_audio_in_config()->set_sample_rate_hertz(16000);
      // assist_config->set_text_query(text_input_source);

      // Read credentials file.
      auto call_credentials = ReadCredentialsFile(credentials_file_path);

      // Begin a stream.
      auto channel = CreateChannel(api_endpoint);
      std::unique_ptr<EmbeddedAssistant::Stub> assistant(
          EmbeddedAssistant::NewStub(channel));

      grpc::ClientContext context;
      context.set_fail_fast(false);
      context.set_credentials(call_credentials);

      std::shared_ptr<ClientReaderWriter<AssistRequest, AssistResponse>> stream(
          std::move(assistant->Assist(&context)));
      // Write config in first stream.
      if (verbose) {
        std::clog << "assistant_sdk wrote first request: "
                  << request.ShortDebugString() << std::endl;
      }
      stream->Write(request);

      gstream = stream;

      // Read responses.
      if (verbose) {
        std::clog << "assistant_sdk waiting for response ... " << std::endl;
      }
      AssistResponse response;

      AudioOutputFlora output(floraAgent);
      bool started = false;

      while (stream->Read(&response)) {  // Returns false when no more to read.
      std::clog << "Received response event_type " << response.event_type() << std::endl;
        if (response.has_audio_out() ||
            response.event_type() == AssistResponse_EventType_END_OF_UTTERANCE) {
          // Synchronously stops audio input if there is one.
          if (voiceStarted) {
            auto msg = Caps::new_instance();
            msg->write(turenId);
            floraAgent->post("rokid.speech.completed", msg);
          }
          voiceStarted = false;
        }
        if (response.has_audio_out()) {
          if (!started)
            output.Start();
          started = true;
          auto data = response.audio_out().audio_data();
          output.Send(data);
        }
        // CUSTOMIZE: render spoken request on screen
        for (int i = 0; i < response.speech_results_size(); i++) {
          auto result = response.speech_results(i);
          if (verbose) {
            std::clog << "assistant_sdk request: \n"
                      << result.transcript() << " ("
                      << std::to_string(result.stability()) << ")" << std::endl;
          }
        }
        if (response.device_action().device_request_json().size() > 0) {
          std::clog << "assistant_sdk device request:" << std::endl;
          std::cout << response.device_action().device_request_json()
                    << std::endl;
          std::shared_ptr<Caps> url_msg = Caps::new_instance();
          url_msg->write("yoda-app://goog-assist/handle-command");
          std::shared_ptr<Caps> param_msg = Caps::new_instance();
          param_msg->write("request");
          param_msg->write(response.device_action().device_request_json());
          url_msg->write(param_msg);
          floraAgent->call("yodaos.runtime.open-url-format", url_msg, "runtime", [](int32_t, flora::Response&) {});
        }
        if (response.dialog_state_out().supplemental_display_text().size() > 0) {
          // CUSTOMIZE: render spoken response on screen
          std::clog << "assistant_sdk response:" << std::endl;
          std::cout << response.dialog_state_out().supplemental_display_text()
                    << std::endl;
        }
      }
      output.Stop();

      greq = nullptr;
      gstream = nullptr;
      lk.unlock();

      grpc::Status status = stream->Finish();
      if (!status.ok()) {
        // Report the RPC failure.
        std::cerr << "assistant_sdk failed, error: " << status.error_message()
                  << std::endl;
      }
    }
  });

  std::vector<std::vector<uint8_t>> buffer;
  floraAgent->subscribe("rokid.turen.start_voice",
    [&](const char*, std::shared_ptr<Caps>& msg, uint32_t) {
      std::clog << "start voice" << std::endl;
      voiceStarted = true;
      buffer.clear();
      cv.notify_one();
    });

  floraAgent->subscribe("rokid.turen.voice",
    [&](const char*, std::shared_ptr<Caps>& msg, uint32_t) {
      std::vector<uint8_t> data;
      if (msg->read(data) != CAPS_SUCCESS) {
        return -1;
      }
      msg->read(turenId);

      if (greq == nullptr) {
        return -1;
      }
      buffer.push_back(data);

      if (gstream == nullptr) {
        return -1;
      }
      auto request = *greq;
      if (!buffer.empty()) {
        for (auto& buf : buffer) {
          request.set_audio_in((void*)&(buf[0]), buf.size());
          gstream->Write(request);
        }
        buffer.clear();
      }
      request.set_audio_in((void*)&(data[0]), data.size());
      gstream->Write(request);

      return 0;
    });

  thd->join();

  return 0;
}
