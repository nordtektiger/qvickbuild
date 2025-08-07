#ifndef INTERPRETER_H
#define INTERPRETER_H

struct IString;
struct IBool;
template <typename T> struct IList;
#include <variant>
using IValue = std::variant<IString, IBool, IList<IString>, IList<IBool>>;

#include "driver.hpp"
#include "parser.hpp"
#include "tracking.hpp"
#include <mutex>
#include <vector>

struct ICoreType {
  bool immutable;
  ICoreType() = delete;
  ICoreType(bool immutable) : immutable(immutable) {};
};

struct IString : public ICoreType {
  StreamReference reference;
  std::string content;

  std::string to_string() const;
  IString() = delete;
  IString(Token, bool);
  IString(std::string, StreamReference, bool);
  bool operator==(IString const other) const;
};

struct IBool : public ICoreType {
  StreamReference reference;
  bool content;
  IBool() = delete;
  IBool(Token, bool);
  IBool(bool, StreamReference, bool);
  operator bool() const;
  bool operator==(IBool const other) const;
};

template <typename T> struct IList : public ICoreType {
  StreamReference reference;
  std::vector<T> contents;
  IList() = delete;
  IList(std::vector<T>, StreamReference reference, bool);
  bool operator==(IList const other) const;
};

// struct IValue : public ICoreType {
//   std::variant<IString, IBool, IList<IString>, IList<IBool>> data;
//   bool immutable = true;
//   // IValue() = delete; // todo: consider implementation
// };

struct IVisitReference {
  StreamReference operator()(IString istring) { return istring.reference; };
  StreamReference operator()(IBool ibool) { return ibool.reference; };
  StreamReference operator()(IList<IString> ilist) { return ilist.reference; };
  StreamReference operator()(IList<IBool> ilist) { return ilist.reference; };
};

struct EvaluationContext {
  std::optional<Task> task_scope;
  std::optional<std::string> task_iteration;
  bool use_globbing = true;
  bool is_reachable_by(EvaluationContext const) const;
};

struct ValueInstance {
  Identifier identifier;
  EvaluationContext context;
  IValue result;
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

#include "errors.hpp"

class Interpreter {
private:
  std::shared_ptr<EvaluationState> state;
  std::mutex evaluation_lock;

  IValue evaluate_ast_object(ASTObject ast_object, EvaluationContext context);
  std::optional<Task> find_task(std::string identifier);
  std::optional<Field> find_field(std::string identifier,
                                  std::optional<Task> task);
  std::optional<IValue> evaluate_field_optional(std::string identifier,
                                                EvaluationContext context);
  template <typename T>
  std::optional<T> evaluate_field_optional_strict(std::string identifier,
                                                  EvaluationContext context);
  std::optional<IValue>
  evaluate_field_default(std::string identifier, EvaluationContext context,
                         std::optional<IValue> default_value);
  template <typename T>
  std::optional<T>
  evaluate_field_default_strict(std::string identifier,
                                EvaluationContext context,
                                std::optional<T> default_value);

  void t_run_task(Task task, std::string task_iteration,
                  std::shared_ptr<std::atomic<bool>> error,
                  std::vector<std::shared_ptr<Frame>> local_stack);
  void run_task(Task task, std::string task_iteration);
  DependencyStatus _solve_dependencies_parallel(IValue dependencies);
  DependencyStatus _solve_dependencies_sync(IValue dependencies);
  DependencyStatus solve_dependencies(IValue dependencies, bool parallel);

public:
  Interpreter(AST &ast, Setup &setup);
  void build();
};

#endif
