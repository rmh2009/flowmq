#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace flowmq {

struct LogEntryMetaData {
  int last_committed;

  std::string serialize() const { return std::to_string(last_committed); }

  static LogEntryMetaData deserialize(const std::string& str) {
    LogEntryMetaData metadata;
    metadata.last_committed = stoi(str);
    return metadata;
  }
};

inline bool operator==(const LogEntryMetaData& l, const LogEntryMetaData& r) {
  return l.last_committed == r.last_committed;
}

}  // namespace flowmq
