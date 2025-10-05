#ifndef ERRORS_H
#define ERRORS_H

#include "../lexer/tracking.hpp"
#include "types.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct ReferenceView {
  std::string line_prefix;
  std::string line_ref;
  std::string line_suffix;
  size_t line_num;
};

class ErrorRenderer {
public:
  static ReferenceView get_reference_view(std::vector<unsigned char> config,
                                          StreamReference reference);
  static std::string get_rendered_view(ReferenceView reference_view,
                                       std::string msg);
  static std::string prefix_rendered_view(std::string view, std::string prefix);
  template <typename T> static std::string stringify_type(T);
};

// api-facing error handler.
class ErrorHandler {
private:
  // we have multiple threads running, and thus more than one
  // error may be reported for a single build pass.
  static std::unordered_map<size_t, std::shared_ptr<BuildError>> error_state;
  static std::mutex error_lock;

public:
  template <typename B> static void halt [[noreturn]] (B build_error);
  template <typename B> static void soft_report(B build_error);
  static void trigger_report [[noreturn]] ();
  static std::unordered_map<size_t, std::shared_ptr<BuildError>> get_errors();
};

// internal exception.
class BuildException : public std::exception {
private:
  const char *details;

public:
  BuildException(const char *details) : details(details) {}
  const char *what() const noexcept override { return details; };
};

// api-facing context stack getter.
class ContextStack {
  friend class FrameGuard;

private:
  // thread hash, frames
  static std::mutex stack_lock;
  static std::unordered_map<size_t, std::vector<std::shared_ptr<Frame>>> stack;
  static std::unordered_map<size_t, bool> frozen;

public:
  static std::unordered_map<size_t, std::vector<std::shared_ptr<Frame>>>
  dump_stack();
  static std::vector<std::shared_ptr<Frame>> export_local_stack();
  static void import_local_stack(std::vector<std::shared_ptr<Frame>>);

  static void freeze();
  static bool is_frozen();
};

// api-facing context stack frame handler.
class FrameGuard {
private:
  size_t thread_hash;

public:
  FrameGuard() = delete;
  template <typename F> FrameGuard(F frame);
  ~FrameGuard();
};

#endif
