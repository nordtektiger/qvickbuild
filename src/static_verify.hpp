#ifndef STATIC_VERIFY_HPP
#define STATIC_VERIFY_HPP

#include "errors.hpp"
#include "parser.hpp"

class StaticVerify {
public:
  static bool find_recursive_task(std::vector<std::shared_ptr<Frame>>, std::string);
  static bool find_recursive_variable(std::vector<std::shared_ptr<Frame>>, std::string);
};

#endif
