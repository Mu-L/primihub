#ifndef SRC_PRIMIHUB_MISSINGPROCESS_H_
#define SRC_PRIMIHUB_MISSINGPROCESS_H_

#include <math.h>
#include <stdlib.h>
#include <time.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "src/primihub/algorithm/base.h"
#include "src/primihub/common/defines.h"
#include "src/primihub/common/type/type.h"
#include "src/primihub/data_store/driver.h"
#include "src/primihub/executor/express.h"
#include "src/primihub/service/dataset/service.h"

namespace primihub {

class MissingProcess : public AlgorithmBase {
public:
  explicit MissingProcess(PartyConfig &config,
                          std::shared_ptr<DatasetService> dataset_service);
  int loadParams(primihub::rpc::Task &task) override;
  int loadDataset(void) override;
  int initPartyComm(void) override;
  int execute() override;
  int finishPartyComm(void) override;
  int saveModel(void);
  int set_task_info(std::string platform_type, std::string job_id,
                    std::string task_id);
  inline std::string platform() { return platform_type_; }
  inline std::string job_id() { return job_id_; }
  inline std::string task_id() { return task_id_; }

private:
  using NestedVectorI32 = std::vector<std::vector<uint32_t>>;

  inline int _strToInt64(const std::string &str, int64_t &i64_val);
  inline int _strToDouble(const std::string &str, double &d_val);
  inline int _avoidStringArray(std::shared_ptr<arrow::Array> array);
  inline void _buildNewColumn(std::vector<std::string> &col_val,
                              std::shared_ptr<arrow::Array> &array);
  inline void _buildNewColumn(std::shared_ptr<arrow::Table> table,
                              int col_index, const std::string &replace,
                              NestedVectorI32 &abnormal_index, bool need_double,
                              std::shared_ptr<arrow::Array> &new_array);
  inline void _buildNewColumn(std::shared_ptr<arrow::Table> table,
                              int col_index, const std::string &replace,
                              std::vector<int> both_index, bool need_double,
                              std::shared_ptr<arrow::Array> &new_array);
  int _LoadDatasetFromCSV(std::string &filename);

  int _LoadDatasetFromDB(std::string &source);

  void _spiltStr(string str, const string &split, std::vector<string> &strlist);

  std::unique_ptr<MPCOperator> mpc_op_exec_{nullptr};

  std::string job_id_{""};
  std::string task_id_{""};

#ifdef MPC_SOCKET_CHANNEL
  IOService ios_;
  Session ep_next_;
  Session ep_prev_;
  std::string next_ip_{""}, prev_ip_{""};
  uint16_t next_port_{0}, prev_port_{0};
#else
  ABY3PartyConfig party_config_;
  uint16_t local_party_id_{0};
  uint16_t next_party_id_{0};
  uint16_t prev_party_id_{0};

  primihub::Node local_node_;

  std::shared_ptr<network::IChannel> base_channel_next_{nullptr};
  std::shared_ptr<network::IChannel> base_channel_prev_{nullptr};

  std::shared_ptr<MpcChannel> mpc_channel_next_{nullptr};
  std::shared_ptr<MpcChannel> mpc_channel_prev_{nullptr};

  std::map<uint16_t, primihub::Node> partyid_node_map_;
#endif

  std::map<std::string, uint32_t> col_and_dtype_;
  std::vector<std::string> local_col_names;

  std::string data_file_path_{""};
  std::string replace_type_{""};
  std::string conn_info_{""};
  std::shared_ptr<arrow::Table> table{nullptr};
  std::map<std::string, std::vector<int>> db_both_index;

  bool use_db{false};
  std::string table_name{""};
  std::string node_id_{""};
  uint32_t party_id_{0};

  std::string new_dataset_id_{""};
  std::string new_dataset_path_{""};
  std::string platform_type_{""};

  template <class T>
  void replaceValue(map<std::string, uint32_t>::iterator &iter,
                    std::shared_ptr<arrow::Table> &table, int &col_index,
                    T &col_value,
                    std::vector<std::vector<unsigned int>> &abnormal_index,
                    bool &use_db, bool need_double) {
    std::shared_ptr<arrow::Array> new_array;

    if (use_db) {
      _buildNewColumn(table, col_index, std::to_string(col_value),
                      db_both_index[iter->first], need_double, new_array);
    } else {
      _buildNewColumn(table, col_index, std::to_string(col_value),
                      abnormal_index, need_double, new_array);
    }
    std::shared_ptr<arrow::ChunkedArray> chunk_array =
        std::make_shared<arrow::ChunkedArray>(new_array);

    std::shared_ptr<arrow::Field> field;
    if (!need_double) {
      field = std::make_shared<arrow::Field>(iter->first, arrow::int64());
    } else {
      field = std::make_shared<arrow::Field>(iter->first, arrow::float64());
    }

    LOG(INFO) << "Replace column " << iter->first
              << " with new array in table.";

    auto result = table->SetColumn(col_index, field, chunk_array);
    if (!result.ok()) {
      std::stringstream ss;
      ss << "Replace content of column " << iter->first << " failed, "
         << result.status();
      LOG(ERROR) << ss.str();
      throw std::runtime_error(ss.str());
    }

    table = result.ValueOrDie();
    LOG(INFO) << "Finish.";
  }
};

} // namespace primihub
#endif
