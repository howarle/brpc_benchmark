// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// A client sending requests to server in parallel by multiple threads.

#include <brpc/parallel_channel.h>
#include <brpc/server.h>
#include <bthread/bthread.h>
#include <butil/macros.h>
#include <butil/time.h>
#include <fmt/core.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

#include "brpc_client_sync.h"
#include "config.h"
#include "proto/echo.pb.h"
#include "util.h"

class ClientBenchmarker {
 public:
  explicit ClientBenchmarker(BenchmarkConfig config = BenchmarkConfig()) : config_(std::move(config)) {}

  void init() {
    brpc::ChannelOptions channel_options;
    channel_options.protocol = config_.rpc_protocol;
    if (config_.use_single_streaming) {
      CHECK_EQ(config_.rpc_protocol, "baidu_std") << "RPC protocol must be baidu_std when enable streaming";
    }
    channel_options.max_retry = config_.rpc_max_retry;
    channel_options.timeout_ms = config_.rpc_timeout_ms;

    if (config_.use_parallel_channel) {
      num_channel_ = config_.parallel_channel_num;
      // LOG(INFO) << "use parallel channel, channel_num=" << num_channel_;
      brpc::ParallelChannelOptions pchan_options;
      pchan_options.timeout_ms = config_.rpc_timeout_ms;
      if (p_channel_.Init(&pchan_options) != 0) {
        LOG(ERROR) << "Fail to init ParallelChannel";
        exit(-1);
      }

      for (int i = 0; i < num_channel_; ++i) {
        auto sub_channel = new brpc::Channel;
        // Initialize the channel, nullptr means using default options.
        // options, see `brpc/channel.h'.
        if (sub_channel->Init(config_.server_addr.c_str(), config_.load_balancer.c_str(), &channel_options) != 0) {
          LOG(ERROR) << "Fail to initialize sub_channel[" << i << "]";
          exit(-1);
        }
        if (p_channel_.AddChannel(sub_channel, brpc::OWNS_CHANNEL, nullptr, nullptr) != 0) {
          LOG(ERROR) << "Fail to AddChannel, i=" << i;
          exit(-1);
        }
      }
      for (int i = 0; i < num_channel_; ++i) {
        recorder_.sub_channel_latency.emplace_back(std::make_unique<bvar::LatencyRecorder>());
        recorder_.sub_channel_latency.back()->expose(fmt::format("client_sub_{}", i));
      }
    } else {
      num_channel_ = 1;
      // LOG(INFO) << "use single channel, channel_num=" << num_channel_;
      channel_ = std::make_unique<brpc::Channel>();
      if (channel_->Init(config_.server_addr.c_str(), config_.load_balancer.c_str(), &channel_options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        exit(-1);
      }
    }
  }

  void start(bool is_req_bench = true) {
    auto get_stub = [&]() {
      if (config_.use_parallel_channel) {
        return std::make_unique<example::EchoService_Stub>(&p_channel_);
      } else {
        return std::make_unique<example::EchoService_Stub>(channel_.get());
      }
    };
    auto benchmark_time = std::chrono::milliseconds(config_.benchmark_time);

    if (!config_.use_bthread) {
      for (auto i = 0; i < config_.parallelism; ++i) {
        clients_.emplace_back(std::make_unique<Client>(&config_, &recorder_));
        auto c = clients_.back().get();

        c->registeDoneCallBack([this]() { running_cnt_--; });
        running_cnt_++;
        if (is_req_bench) {
          jthreads_.emplace_back(
              [stub = get_stub(), c, benchmark_time]() mutable { c->runReqBench(std::move(stub), benchmark_time); });
        } else {
          jthreads_.emplace_back(
              [stub = get_stub(), c, benchmark_time]() mutable { c->runRespBench(std::move(stub), benchmark_time); });
        }
      }
    } else {
      LOG(INFO) << "QWQ not implemented";
      exit(-1);
    }
  }

  void output() { recorder_.output(); }

  auto getSentBytes() const {
    uint64_t bytes{0};
    for (const auto &c : clients_) {
      bytes += c->getSentBytes();
    }
    return bytes;
  }
  auto getRecievedBytes() const {
    uint64_t bytes{0};
    for (const auto &c : clients_) {
      bytes += c->getRecievedBytes();
    }
    return bytes;
  }

  auto getSentBPS() const {
    double bytes{0};
    for (const auto &c : clients_) {
      bytes += c->getSentBPS();
    }
    return bytes;
  }
  auto getRecievedBPS() const {
    double bytes{0};
    for (const auto &c : clients_) {
      bytes += c->getRecievedBPS();
    }
    return bytes;
  }

  auto &getRecorder() const { return recorder_; }

  auto &getConfig() const { return config_; }

  int getRunningSender() const { return running_cnt_; }

  void join() { jthreads_.clear(); }

 private:
  BenchmarkConfig config_;

  int num_channel_;
  PreformanceRecorder recorder_;
  std::unique_ptr<brpc::Channel> channel_;
  brpc::ParallelChannel p_channel_;

  std::atomic<int> running_cnt_{0};

  std::vector<bthread_t> bids_;
  std::vector<std::jthread> jthreads_;
  std::vector<std::unique_ptr<Client>> clients_;
};

class ResultArrayOutputer {
 public:
  void outputPython(const std::string &target_text, const std::string &x_tag, int num) {
    std::string prefix;
    std::string suffix;
    prefix = fmt::format("{}_", target_text);
    suffix = fmt::format(R"(["{}"]["{}"])", x_tag, shortTheNum(num));

    std::ofstream outfile("result/brpc_result.txt", std::ios::app);
    CHECK(outfile.is_open());

    outfile << fmt::format("# {}  payload size {}\n", target_text, num);

    static bool is_first = true;
    if (is_first) {
      is_first = false;
      outfile << fmt::format("x_axis[\"{}\"] = ", x_tag, prefix, suffix);
      StreamOutPythonArray(outfile, x_axis);
    }

    outfile << fmt::format("{}lantencys{} = ", prefix, suffix);
    StreamOutPythonArray(outfile, lantencys);
    outfile << fmt::format("{}speed{} = ", prefix, suffix);
    StreamOutPythonArray(outfile, speeds);
    outfile << std::endl;

    // Close the file
    outfile.close();
  }

  void outputCsv(const std::string &target_text, const std::string &x_tag) {
    std::string prefix;
    std::string suffix;

    std::ofstream outfile(fmt::format("result/brpc_{}_{}.csv", x_tag, target_text));
    CHECK(outfile.is_open());

    if (*std::max_element(lantencys.begin(), lantencys.end()) == 0) {
      lantencys.clear();
    }
    if (*std::max_element(speeds.begin(), speeds.end()) == 0) {
      speeds.clear();
    }
    if (*std::max_element(qps.begin(), qps.end()) == 0) {
      qps.clear();
    }
    outfile << "x_axis,";
    if (!lantencys.empty()) outfile << "lantencys,";
    if (!speeds.empty()) outfile << "speed,";
    if (!qps.empty()) outfile << "qps,";
    outfile << std::endl;

    for (auto i = 0; i < x_axis.size(); ++i) {
      outfile << fmt::format("{},", x_axis[i]);
      if (!lantencys.empty()) outfile << fmt::format("{},", lantencys[i]);
      if (!speeds.empty()) outfile << fmt::format("{},", speeds[i]);
      if (!qps.empty()) outfile << fmt::format("{}", qps[i]);
      outfile << std::endl;
    }

    // Close the file
    outfile.close();
  }

  std::vector<int> x_axis;
  std::vector<int> qps;
  std::vector<double> lantencys;
  std::vector<double> speeds;
};

void BenchmarkForParallel(std::string target_text, BenchmarkConfig config, int max_parallel = 50) {
  ResultArrayOutputer outer;
  target_text = fmt::format("{}_reqsz({})_para(1-{})_streamsz({})_prot({})", target_text, shortTheNum(config.req_size),
                            max_parallel, shortTheNum(config.single_stream_single_msg_size), config.rpc_protocol);
  LOG(INFO) << target_text;
  for (auto parallel = 1; parallel <= max_parallel && !brpc::IsAskedToQuit(); parallel += 4) {
    config.parallelism = parallel;

    ClientBenchmarker tester(config);
    tester.init();
    tester.start();

    tester.join();

    auto &recorder = tester.getRecorder();
    auto latency_ms = recorder.latency_recorder.latency_percentile(.99) / 1e3;
    int64_t qps = recorder.latency_recorder.qps();

    auto sent_mbps = tester.getSentBPS() / (1 << 20);
    // auto recieved_mbps = tester.getRecievedBPS() / (1 << 20);

    LOG(INFO) << fmt::format(
        "   parallelism {:2d}:   lantency: {:4.2f},  qps: {:4d} "
        "speed: {:4.2f}MB/S ",
        parallel, latency_ms, qps, sent_mbps);

    outer.x_axis.emplace_back(parallel);
    outer.lantencys.emplace_back(latency_ms);
    outer.speeds.emplace_back(sent_mbps);
    outer.qps.emplace_back(qps);
  }
  LOG(INFO);

  outer.outputCsv(target_text, "parallel");
}

void BenchmarkForReqSize(std::string target_text, BenchmarkConfig config, int max_size = (1 << 30)) {
  ResultArrayOutputer outer;
  int min_size = 256;
  target_text = fmt::format(
      "{}_reqsz({}-{})_para({})_streamsz({})_prot({})_stream({},{})", target_text, shortTheNum(min_size),
      shortTheNum(max_size), config.parallelism, shortTheNum(config.single_stream_single_msg_size), config.rpc_protocol,
      shortTheNum(config.continue_stream_messages_in_batch), shortTheNum(config.continue_stream_max_buf_size));
  LOG(INFO) << target_text;
  // int test_gap = (max_size - min_size) / 40 + 1;
  auto test_gap = std::pow(static_cast<long double>(max_size) / min_size, 1.0 / 40);
  for (long double req_size_r = min_size; req_size_r <= max_size && !brpc::IsAskedToQuit(); req_size_r *= test_gap) {
    int req_size = std::round(req_size_r);
    config.req_size = req_size;

    ClientBenchmarker tester(config);
    tester.init();
    tester.start();
    tester.join();

    auto &recorder = tester.getRecorder();
    auto latency_ms = recorder.latency_recorder.latency_percentile(.99) / 1e3;
    int64_t qps = recorder.latency_recorder.qps();

    auto sent_mbps = tester.getSentBPS() / (1 << 20);
    // auto recieved_mbps = tester.getRecievedBPS() / (1 << 20);

    LOG(INFO) << fmt::format(
        "   req_size {:2d}:   lantency: {:4.2f},  qps: {:4d} "
        "speed: {:4.2f}MB/S ",
        req_size, latency_ms, qps, sent_mbps);

    outer.x_axis.emplace_back(req_size);
    outer.lantencys.emplace_back(latency_ms);
    outer.speeds.emplace_back(sent_mbps);
    outer.qps.emplace_back(qps);
  }
  LOG(INFO);

  outer.outputCsv(target_text, "req-size");
}

void BenchmarkForStreamingSize(std::string target_text, BenchmarkConfig config, int max_msg_size = (1 << 13)) {
  ResultArrayOutputer outer;
  int min_size = 256;
  target_text = fmt::format("{}_reqsz({})_para({})_streamsz({}-{})_prot({})", target_text, shortTheNum(config.req_size),
                            config.parallelism, shortTheNum(min_size),
                            shortTheNum(config.single_stream_single_msg_size), config.rpc_protocol);

  LOG(INFO) << target_text;
  int test_gap = (max_msg_size - min_size) / 20 + 1;
  for (auto s_size = min_size; s_size <= max_msg_size && !brpc::IsAskedToQuit(); s_size += test_gap) {
    config.single_stream_single_msg_size = s_size;

    ClientBenchmarker tester(config);
    tester.init();
    tester.start();
    tester.join();

    auto &recorder = tester.getRecorder();
    auto latency_ms = recorder.latency_recorder.latency_percentile(.99) / 1e3;
    int64_t qps = recorder.latency_recorder.qps();

    auto sent_mbps = tester.getSentBPS() / (1 << 20);
    // auto recieved_mbps = tester.getRecievedBPS() / (1 << 20);

    LOG(INFO) << fmt::format(
        "   streaming_msg_size {:2d}:   lantency: {:4.2f},  qps: {:4d} "
        "speed: {:4.2f}MB/S ",
        s_size, latency_ms, qps, sent_mbps);

    outer.x_axis.emplace_back(s_size);
    outer.lantencys.emplace_back(latency_ms);
    outer.speeds.emplace_back(sent_mbps);
    outer.qps.emplace_back(qps);
  }
  LOG(INFO);

  outer.outputCsv(target_text, "streaming-size");
}

DEFINE_bool(for_parallelism, false, "");
DEFINE_bool(for_req_size, false, "");
DEFINE_bool(for_streaming_size, false, "");

DEFINE_bool(test_proto, true, "");
DEFINE_bool(test_attachment, true, "");
DEFINE_bool(test_cstreaming, true, "");
DEFINE_bool(test_sstreaming, false, "");

int main(int argc, char *argv[]) {
  auto config = parseCommandLine(argc, argv);

  if (FLAGS_for_parallelism) {
    if (FLAGS_test_attachment) {
      config.use_attachment = true;
      BenchmarkForParallel("attachment", config);
      config.use_attachment = false;
    }
    if (FLAGS_test_proto) {
      config.use_proto_bytes = true;
      BenchmarkForParallel("proto", config);
      config.use_proto_bytes = false;
    }
    if (FLAGS_test_sstreaming) {
      config.use_single_streaming = true;
      BenchmarkForParallel("sstreaming", config);
      config.use_single_streaming = false;
    }
  }

  if (FLAGS_for_req_size) {
    auto max_req_size = config.req_size;

    if (FLAGS_test_attachment) {
      config.use_attachment = true;
      BenchmarkForReqSize("attachment", config, max_req_size);
      config.use_attachment = false;
    }
    if (FLAGS_test_proto) {
      config.use_proto_bytes = true;
      BenchmarkForReqSize("proto", config, max_req_size);
      config.use_proto_bytes = false;
    }
    if (FLAGS_test_cstreaming) {
      config.use_continue_streaming = true;
      BenchmarkForReqSize("cstreaming", config, max_req_size);
      config.use_continue_streaming = false;
    }

    if (FLAGS_test_sstreaming) {
      config.use_single_streaming = true;
      BenchmarkForReqSize("sstreaming", config, max_req_size);
      config.use_single_streaming = false;
    }
  }

  if (FLAGS_for_streaming_size) {
    auto max_msg_size = config.req_size;
    if (FLAGS_test_cstreaming) {
      config.use_continue_streaming = true;
      BenchmarkForStreamingSize("cstreaming", config, max_msg_size);
      config.use_continue_streaming = false;
    }

    if (FLAGS_test_sstreaming) {
      config.use_single_streaming = true;
      BenchmarkForStreamingSize("sstreaming", config, max_msg_size);
      config.use_single_streaming = false;
    }
  }

  return 0;
}
