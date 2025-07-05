#ifndef INTERPRETER_H
#define INTERPRETER_H

struct IString;
struct IBool;
struct IList;
struct IValue;

#include "tracking.hpp"
#include "driver.hpp"
#include "parser.hpp"
#include <mutex>
#include <variant>
#include <vector>

struct IString {
  StreamReference reference;
  std::string content;

  std::string toString() const;
  IString() = delete;
  IString(Token);
  IString(std::string, StreamReference);
  bool operator==(IString const other) const;
};

struct IBool {
  StreamReference reference;
  bool content;
  IBool() = delete;
  IBool(Token);
  IBool(bool, StreamReference);
  operator bool() const;
  bool operator==(IBool const other) const;
};

#define ILIST_STR 0
#define ILIST_BOOL 1

struct IList {
  StreamReference reference;
  std::variant<std::vector<IString>, std::vector<IBool>> contents;
  bool holds_istring() const;
  bool holds_ibool() const;
  IList() = delete;
  IList(std::variant<std::vector<IString>, std::vector<IBool>>,
        StreamReference reference);
  bool operator==(IList const other) const;
};

struct IValue {
  std::variant<IString, IBool, IList> value;
  bool immutable = true;
  // IValue() = delete; // todo: consider implementation
};

struct IVisitReference {
  StreamReference operator()(IString istring) { return istring.reference; };
  StreamReference operator()(IBool ibool) { return ibool.reference; };
  StreamReference operator()(IList ilist) { return ilist.reference; };
};

struct EvaluationContext {
  std::optional<Task> task_scope;
  std::optional<std::string> task_iteration;
  bool use_globbing = true;
  bool context_verify(EvaluationContext const) const;
};

struct ValueInstance {
  Identifier identifier;
  EvaluationContext context;
  IValue result;
};
struct EvaluationState {
  std::vector<ValueInstance> values;
};

struct DependencyStatus {
  bool success;
  std::optional<size_t> modified;
};

class Interpreter {
private:
  AST &m_ast;
  Setup &m_setup;
  std::shared_ptr<EvaluationState> state;
  std::mutex evaluation_lock;

  IValue evaluate_ast_object(ASTObject ast_object, AST &ast,
                             EvaluationContext context,
                             std::shared_ptr<EvaluationState> state);
  std::optional<Task> find_task(IString identifier);
  std::optional<Field> find_field(std::string identifier,
                                  std::optional<Task> task);
  std::optional<IValue>
  evaluate_field_optional(std::string identifier, EvaluationContext context,
                          std::shared_ptr<EvaluationState> state);
  std::optional<IValue> evaluate_field_default(std::string identifier,
                                EvaluationContext context,
                                std::shared_ptr<EvaluationState> state,
                                std::optional<IValue> default_value);
  void t_run_task(Task task, std::string task_iteration,
                  std::shared_ptr<std::atomic<bool>> error);
  int run_task(Task task, std::string task_iteration);
  DependencyStatus _solve_dependencies_parallel(IValue dependencies);
  DependencyStatus _solve_dependencies_sync(IValue dependencies);
  DependencyStatus solve_dependencies(IValue dependencies, bool parallel);

public:
  Interpreter(AST &ast, Setup &setup);
  int build();
};

#endif
