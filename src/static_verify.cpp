#include "static_verify.hpp"

bool StaticVerify::find_recursive_task(
    std::vector<std::shared_ptr<Frame>> stack, std::string task) {
  // it's ok if it appears one time, but more than two times = trouble.
  size_t count = 0;
  for (const std::shared_ptr<Frame> &frame_ptr : stack) {
    std::shared_ptr<EntryBuildFrame> entry_ptr =
        std::dynamic_pointer_cast<EntryBuildFrame>(frame_ptr);
    if (entry_ptr) {
      if (entry_ptr->get_unique_identifier() == task) {
        count++;
        continue;
      } else
        continue;
    }

    std::shared_ptr<DependencyBuildFrame> dep_ptr =
        std::dynamic_pointer_cast<DependencyBuildFrame>(frame_ptr);
    if (dep_ptr) {
      if (dep_ptr->get_unique_identifier() == task) {
        count++;
        continue;
      } else
        continue;
    }
  }
  return count >= 2;
}

bool StaticVerify::find_recursive_variable(
    std::vector<std::shared_ptr<Frame>> stack, std::string variable) {
  // it's ok if it appears one time, but more than two times = trouble.
  size_t count = 0;
  for (const std::shared_ptr<Frame> &frame_ptr : stack) {
    std::shared_ptr<IdentifierEvaluateFrame> identifier_ptr =
        std::dynamic_pointer_cast<IdentifierEvaluateFrame>(frame_ptr);
    if (identifier_ptr) {
      if (identifier_ptr->get_unique_identifier() == variable) {
        count++;
        continue;
      } else
        continue;
    }
  }
  return count >= 2;
}
