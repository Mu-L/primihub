/*
 Copyright 2022 Primihub

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
#include "src/primihub/task/semantic/scheduler/pir_scheduler.h"
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "src/primihub/node/server_config.h"

using primihub::rpc::ParamValue;
using primihub::rpc::TaskType;
using primihub::rpc::VirtualMachine;
using primihub::rpc::VarType;
using primihub::rpc::PirType;

namespace primihub::task {
retcode PIRScheduler::ScheduleTask(const std::string& party_name,
                                  const Node dest_node,
                                  const PushTaskRequest& request) {
  SET_THREAD_NAME("PIRScheduler");
  PushTaskReply reply;
  PushTaskRequest send_request;
  send_request.CopyFrom(request);
  auto task_ptr = send_request.mutable_task();
  task_ptr->set_party_name(party_name);
  if (party_name == PARTY_SERVER) {
    // remove client data when send data to server
    auto param_map_ptr = task_ptr->mutable_params()->mutable_param_map();
    auto it = param_map_ptr->find("clientData");
    if (it != param_map_ptr->end()) {
      it->second.Clear();
    }
  }
  // fill scheduler info
  {
    auto party_access_info_ptr = task_ptr->mutable_party_access_info();
    auto& local_node = getLocalNodeCfg();
    rpc::Node scheduler_node;
    node2PbNode(local_node, &scheduler_node);
    auto& schduler_node = (*party_access_info_ptr)[SCHEDULER_NODE];
    schduler_node = std::move(scheduler_node);
  }
  // send request
  std::string dest_node_address = dest_node.to_string();
  LOG(INFO) << "dest node " << dest_node_address;
  auto channel = this->getLinkContext()->getChannel(dest_node);
  auto ret = channel->executeTask(send_request, &reply);
  if (ret == retcode::SUCCESS) {
    VLOG(5) << "submit task to : " << dest_node_address << " reply success";
  } else {
    set_error();
    LOG(ERROR) << "submit task to : " << dest_node_address << " reply failed";
    return retcode::FAIL;
  }
  parseNotifyServer(reply);
  return retcode::SUCCESS;
}

retcode PIRScheduler::dispatch(const PushTaskRequest *pushTaskRequest) {
  PushTaskRequest push_request;
  push_request.CopyFrom(*pushTaskRequest);
  const auto& params = push_request.task().params().param_map();
  // if query request using query keyword instead of dataset,
  // add client access info as dispatch server info
  auto it = params.find("clientData");
  if (it != params.end()) {
    auto& pv_client_data = it->second;
    if (pv_client_data.is_array()) {
      auto party_access_info = push_request.mutable_task()->mutable_party_access_info();
      auto& party_info = (*party_access_info)[PARTY_CLIENT];
      auto& local_node = getLocalNodeCfg();
      node2PbNode(local_node, &party_info);
    }
  }
  int pirType = PirType::ID_PIR;
  auto param_it = params.find("pirType");
  if (param_it != params.end()) {
    pirType = param_it->second.value_int32();
  }

  LOG(INFO) << "begin to Dispatch SubmitTask to PIR task party node ...";
  const auto& participate_node = push_request.task().party_access_info();
  for (const auto& [party_name, node] : participate_node) {
    this->error_msg_.insert({party_name, ""});
  }
  std::vector<std::thread> thrds;
  for (const auto& [party_name, node] : participate_node) {
    Node dest_node;
    pbNode2Node(node, &dest_node);
    LOG(INFO) << "Dispatch SubmitTask to PIR party node: "
        << dest_node.to_string() << " "
        << "party_name: " << party_name;
    thrds.emplace_back(
      std::thread(
        &PIRScheduler::ScheduleTask,
        this,
        party_name,
        dest_node,
        std::ref(push_request)));
  }
  for (auto &t : thrds) {
    t.join();
  }
  if (has_error()) {
    return retcode::FAIL;
  }
  return retcode::SUCCESS;
}

}  // namespace primihub::task
