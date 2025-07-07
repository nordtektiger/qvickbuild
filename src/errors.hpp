#ifndef ERRORS_H
#define ERRORS_H

struct ReferenceView;
class BuildError;

#include "interpreter.hpp"
#include "lexer.hpp"
#include "oslayer.hpp"
#include "parser.hpp"
#include "tracking.hpp"
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class ErrorRenderer {
public:
  static ReferenceView get_reference_view(std::vector<unsigned char> config,
                                          StreamReference reference);
  static std::string get_rendered_view(ReferenceView reference_view);
  static std::string get_rendered_view(ReferenceView reference_view,
                                       std::string msg);
  static std::string prefix_rendered_view(std::string view, std::string prefix);
  static std::string
      stringify_type(std::variant<ASTObject, IValue, IString, IBool, IList>);
};

struct ReferenceView {
  std::string line_prefix;
  std::string line_ref;
  std::string line_suffix;
  size_t line_num;
};

class BuildError {
public:
  virtual std::string render_error(std::vector<unsigned char> config) = 0;
  virtual char const *get_exception_msg() = 0;
  virtual ~BuildError() = default;
};

class ENoMatchingIdentifier : public BuildError {
private:
  Identifier identifier;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoMatchingIdentifier() = delete;
  ENoMatchingIdentifier(Identifier);
};

class EListTypeMismatch : public BuildError {
private:
  IList list;
  IValue faulty_ivalue;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EListTypeMismatch() = delete;
  EListTypeMismatch(IList, IValue);
};

class EReplaceTypeMismatch : public BuildError {
private:
  Replace replace;
  IValue faulty_ivalue;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EReplaceTypeMismatch() = delete;
  EReplaceTypeMismatch(Replace, IValue);
};

class EReplaceChunksLength : public BuildError {
private:
  IValue replacement;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EReplaceChunksLength() = delete;
  EReplaceChunksLength(IValue);
};

// class ENoFieldNorDefault : public BuildError {
// private:
//   std::string field_name;
//
// public:
//   std::string render_error(std::vector<unsigned char> config) override;
//   char const *get_exception_msg() override;
//   ENoFieldNorDefault() = delete;
//   ENoFieldNorDefault(std::string);
// };

class EVariableTypeMismatch : public BuildError {
private:
  IValue variable;
  std::string expected_type;

public:
  std::string render_error(std::vector<unsigned char> config);
  char const *get_exception_msg();
  EVariableTypeMismatch(IValue, std::string);
};

class ENonZeroProcess : public BuildError {
private:
  std::string cmdline;
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENonZeroProcess() = delete;
  ENonZeroProcess(std::string, StreamReference);
};

class ETaskNotFound : public BuildError {
private:
  std::string task_name;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ETaskNotFound() = delete;
  ETaskNotFound(std::string);
};

class ENoTasks : public BuildError {
public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
};

class EAmbiguousTask : public BuildError {
private:
  Task task;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EAmbiguousTask() = delete;
  EAmbiguousTask(Task);
};

class EDependencyStatus : public BuildError {
private:
  Task task;
  IString dependency_where;
  std::string dependency_value;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EDependencyStatus() = delete;
  EDependencyStatus(Task, IString, std::string);
};

class EDependencyFailed : public BuildError {
private:
  IValue dependency;
  std::string dependency_value;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EDependencyFailed() = delete;
  EDependencyFailed(IValue, std::string);
};

class EInvalidSymbol : public BuildError {
private:
  StreamReference reference;
  std::string symbol;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EInvalidSymbol() = delete;
  EInvalidSymbol(StreamReference, std::string);
};

class EInvalidGrammar : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EInvalidGrammar() = delete;
  EInvalidGrammar(StreamReference);
};

class EInvalidLiteral : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EInvalidLiteral() = delete;
  EInvalidLiteral(StreamReference);
};

class ENoValue : public BuildError {
private:
  Identifier identifier;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoValue() = delete;
  ENoValue(Identifier);
};

class ENoLinestop : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoLinestop() = delete;
  ENoLinestop(StreamReference);
};

class ENoIterator : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoIterator() = delete;
  ENoIterator(StreamReference);
};

class ENoTaskOpen : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoTaskOpen() = delete;
  ENoTaskOpen(StreamReference);
};

class ENoTaskClose : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoTaskClose() = delete;
  ENoTaskClose(StreamReference);
};

// todo: revisit this.
class EInvalidListEnd : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EInvalidListEnd() = delete;
  EInvalidListEnd(StreamReference);
};

class ENoReplacementIdentifier : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoReplacementIdentifier() = delete;
  ENoReplacementIdentifier(StreamReference);
};

class ENoReplacementOriginal : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoReplacementOriginal() = delete;
  ENoReplacementOriginal(StreamReference);
};

class ENoReplacementArrow : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoReplacementArrow() = delete;
  ENoReplacementArrow(StreamReference);
};

class ENoReplacementReplacement : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoReplacementReplacement() = delete;
  ENoReplacementReplacement(StreamReference);
};

class EInvalidEscapedExpression : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EInvalidEscapedExpression() = delete;
  EInvalidEscapedExpression(StreamReference);
};

class ENoExpressionClose : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ENoExpressionClose() = delete;
  ENoExpressionClose(StreamReference);
};

class EEmptyExpression : public BuildError {
private:
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EEmptyExpression() = delete;
  EEmptyExpression(StreamReference);
};

class EInvalidInputFile : public BuildError {
private:
  std::string path;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EInvalidInputFile() = delete;
  EInvalidInputFile(std::string);
};

class EInvalidEscapeCode : public BuildError {
private:
  unsigned char code;
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EInvalidEscapeCode() = delete;
  EInvalidEscapeCode(unsigned char, StreamReference);
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

// a single frame in the context stack.
class Frame {
public:
  virtual std::string render_frame(std::vector<unsigned char> config) = 0;
  virtual ~Frame() = default;
};

class EntryBuildFrame : public Frame {
private:
  std::string task;
  StreamReference reference;

public:
  std::string render_frame(std::vector<unsigned char> config) override;
  EntryBuildFrame() = delete;
  EntryBuildFrame(std::string task, StreamReference reference);
};

class DependencyBuildFrame : public Frame {
private:
  std::string task;
  StreamReference reference;

public:
  std::string render_frame(std::vector<unsigned char> config) override;
  DependencyBuildFrame() = delete;
  DependencyBuildFrame(std::string task, StreamReference reference);
};

class IdentifierEvaluateFrame : public Frame {
private:
  std::string identifier;
  StreamReference reference;

public:
  std::string render_frame(std::vector<unsigned char> config) override;
  IdentifierEvaluateFrame() = delete;
  IdentifierEvaluateFrame(std::string identifier, StreamReference reference);
};

// api-facing context stack getter.
class ContextStack {
  friend class FrameGuard;

private:
  // thread hash, frames
  static std::unordered_map<size_t, std::vector<std::unique_ptr<Frame>>> stack;
  static std::mutex stack_lock;
  static bool frozen;

public:
  static std::unordered_map<size_t, std::vector<std::unique_ptr<Frame>>>
  dump_stack();
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
