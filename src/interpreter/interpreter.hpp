#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "../driver/driver.hpp"
#include "../errors/types.hpp"
#include "../parser/types.hpp"
#include "../cli/cli.hpp"
#include "types.hpp"
#include <memory>
#include <mutex>
#include <vector>

struct EvaluationContext {
  std::optional<Task> task_scope;
  std::optional<std::string> task_iteration;
  bool use_globbing = true;
  bool is_reachable_by(EvaluationContext const) const;
};

struct ValueInstance {
  Identifier identifier;
  EvaluationContext context;
  std::unique_ptr<IValue> result;
};
struct EvaluationState {
  std::unique_ptr<AST> ast;
  Setup setup;
  std::vector<ValueInstance> cached_variables;
  std::map<std::string, std::shared_ptr<Task>> cached_tasks;
  std::optional<Task> topmost_task;
};

struct DependencyStatus {
  bool success;
  std::optional<size_t> modified;
};

struct RunContext {
  Task task;
  std::string task_iteration;
  std::optional<std::shared_ptr<CLIEntryHandle>> parent_handle;
  std::vector<std::shared_ptr<Frame>> parent_frame_stack;
};

class Interpreter {
private:
  std::shared_ptr<EvaluationState> state;
  std::mutex evaluation_lock;

  std::unique_ptr<IValue> evaluate_ast_object(ASTObject ast_object,
                                              EvaluationContext context);
  std::optional<Task> find_task(std::string identifier);
  std::optional<Field> find_field(std::string identifier,
                                  std::optional<Task> task);
  std::optional<std::unique_ptr<IValue>>
  evaluate_field_optional(std::string identifier, EvaluationContext context);
  template <typename T>
  std::optional<T> evaluate_field_optional_strict(std::string identifier,
                                                  EvaluationContext context);
  std::optional<std::unique_ptr<IValue>>
  evaluate_field_default(std::string identifier, EvaluationContext context,
                         std::optional<std::unique_ptr<IValue>> default_value);
  template <typename T>
  std::optional<T>
  evaluate_field_default_strict(std::string identifier,
                                EvaluationContext context,
                                std::optional<T> default_value);

  void run_task(RunContext);
  size_t compute_latest_dependency_change(IList<IString> dependencies);
  void solve_dependencies(IList<IString> dependencies,
                          std::shared_ptr<CLIEntryHandle> parent_iteration, bool parallel);

public:
  Interpreter(AST &ast, Setup &setup);
  void build();
};

#endif
