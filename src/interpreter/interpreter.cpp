#include "interpreter.hpp"
#include "../errors/errors.hpp"
#include "../lexer/tracking.hpp"
#include "../system/filesystem.hpp"
#include "../system/pipeline.hpp"
#include "../system/processes.hpp"
#include "literals.hpp"
#include "static_verify.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <ranges>

#define DEPENDS "depends"
#define DEPENDS_PARALLEL "depends_parallel"
#define RUN "run"
#define RUN_PARALLEL "run_parallel"
#define VISIBLE "visible"

#define IMMUTABLE true
#define MUTABLE false

/* -- type casting. -- */
template <> IString IValue::autocast() { return this->cast_to_istring(); }
template <> IBool IValue::autocast() { return this->cast_to_ibool(); }
template <> IList<IString> IValue::autocast() {
  return this->cast_to_ilist_istring();
}
template <> IList<IBool> IValue::autocast() {
  return this->cast_to_ilist_ibool();
}

IType IString::get_type() { return IType::IString; }
IType IBool::get_type() { return IType::IBool; }
template <> IType IList<IString>::get_type() { return IType::IList_IString; }
template <> IType IList<IBool>::get_type() { return IType::IList_IBool; }

std::unique_ptr<IValue> IString::clone() {
  return std::make_unique<IString>(*this);
}
std::unique_ptr<IValue> IBool::clone() {
  return std::make_unique<IBool>(*this);
}
template <> std::unique_ptr<IValue> IList<IString>::clone() {
  return std::make_unique<IList<IString>>(*this);
}
template <> std::unique_ptr<IValue> IList<IBool>::clone() {
  return std::make_unique<IList<IBool>>(*this);
}

/* istring. */
IString IString::cast_to_istring() { return *this; }
IBool IString::cast_to_ibool() {
  ErrorHandler::halt(EVariableTypeMismatch{*this, "bool"});
}
IList<IString> IString::cast_to_ilist_istring() {
  return IList<IString>{{*this}, this->reference, this->immutable};
}
IList<IBool> IString::cast_to_ilist_ibool() {
  ErrorHandler::halt(EVariableTypeMismatch{*this, "bool or list<bool>"});
}

/* ibool. */
IString IBool::cast_to_istring() {
  ErrorHandler::halt(EVariableTypeMismatch{*this, "string"});
}
IBool IBool::cast_to_ibool() { return *this; }
IList<IString> IBool::cast_to_ilist_istring() {
  ErrorHandler::halt(EVariableTypeMismatch{*this, "string or list<string>"});
}
IList<IBool> IBool::cast_to_ilist_ibool() {
  return IList<IBool>{{*this}, this->reference, this->immutable};
}

/* ilist<istring>. */
template <> IString IList<IString>::cast_to_istring() {
  if (this->contents.size() == 1)
    return this->contents[0];
  ErrorHandler::halt(EVariableTypeMismatch{*this, "string"});
}
template <> IBool IList<IString>::cast_to_ibool() {
  ErrorHandler::halt(EVariableTypeMismatch{*this, "bool"});
}
template <> IList<IString> IList<IString>::cast_to_ilist_istring() {
  return *this;
}
template <> IList<IBool> IList<IString>::cast_to_ilist_ibool() {
  ErrorHandler::halt(EVariableTypeMismatch{*this, "bool or list<bool>"});
}

/* ilist<ibool>. */
template <> IString IList<IBool>::cast_to_istring() {
  ErrorHandler::halt(EVariableTypeMismatch{*this, "string"});
}
template <> IBool IList<IBool>::cast_to_ibool() {
  if (this->contents.size() == 1)
    return this->contents[0];
  ErrorHandler::halt(EVariableTypeMismatch{*this, "bool"});
}
template <> IList<IString> IList<IBool>::cast_to_ilist_istring() {
  ErrorHandler::halt(EVariableTypeMismatch{*this, "string or list<string>"});
}
template <> IList<IBool> IList<IBool>::cast_to_ilist_ibool() { return *this; }

/* -- constructors & casts for internal data types. */
IString::IString(Token token, bool immutable)
    : IValue(immutable, token.reference) {
  assert(token.type == TokenType::Literal &&
         "attempt to construct IString from non-literal token");
  this->content = std::get<CTX_STR>(*token.context);
};
IString::IString(std::string content, StreamReference reference, bool immutable)
    : IValue(immutable, reference) {
  this->content = content;
}
std::string IString::to_string() const { return (this->content); };
bool IString::operator==(IString const other) const {
  return this->content == other.content;
}

IBool::IBool(Token token, bool immutable) : IValue(immutable, token.reference) {
  assert((token.type == TokenType::True || token.type == TokenType::False) &&
         "attempt to construct IBool from non-boolean token");
  this->content = (token.type == TokenType::True);
}
IBool::IBool(bool content, StreamReference reference, bool immutable)
    : IValue(immutable, reference) {
  this->content = content;
}
bool IBool::operator==(IBool const other) const {
  return this->content == other.content;
}
IBool::operator bool() const { return (this->content); };

template <typename T>
IList<T>::IList(std::vector<T> contents, StreamReference reference,
                bool immutable)
    : IValue(immutable, reference) {
  this->contents = contents;
}

template <typename T> bool IList<T>::operator==(IList const other) const {
  return this->contents == other.contents;
}

// returns true if, and only if, the passed context can reach the caller.
bool EvaluationContext::is_reachable_by(EvaluationContext const other) const {
  // globbing is *not* verified here because only variables are cached, and
  // variables are by design forced to activate globbing
  if (this->task_scope == std::nullopt)
    return true;
  if (this->task_scope == other.task_scope)
    return true;
  return false;
}

// visitor that evaluates an AST object recursively.
struct ASTEvaluate {
  EvaluationContext context;
  std::shared_ptr<EvaluationState> state;
  std::unique_ptr<IValue> operator()(Identifier const &identifier);
  std::unique_ptr<IValue> operator()(Literal const &literal);
  std::unique_ptr<IValue> operator()(FormattedLiteral const &formatted_literal);
  std::unique_ptr<IValue> operator()(List const &list);
  std::unique_ptr<IValue> operator()(Boolean const &boolean);
  std::unique_ptr<IValue> operator()(Replace const &replace);
};

std::unique_ptr<IValue>
Interpreter::evaluate_ast_object(ASTObject ast_object,
                                 EvaluationContext context) {
  // evaluation visitor can amend shared data in the state.
  std::lock_guard<std::mutex> guard(evaluation_lock);
  std::unique_ptr<IValue> value =
      std::visit(ASTEvaluate{context, this->state}, ast_object);
  return value;
}

std::unique_ptr<IValue> ASTEvaluate::operator()(Identifier const &identifier) {
  FrameGuard frame(
      IdentifierEvaluateFrame(identifier.content, identifier.reference));

  // check whether we're shooting ourselves and the stack in the foot.
  bool recursive = StaticVerify::find_recursive_variable(
      ContextStack::export_local_stack(), identifier.content);
  if (recursive)
    ErrorHandler::halt(ERecursiveVariable{identifier});

  // any identifier will *always* have globbing enabled. if a replacement
  // operator attempts to evaluate an ast object, it will override the globbing
  // to false, but if the value it's referencing is behind another variable
  // initialization, globbing should be enabled to avoid unintuitive errors.
  EvaluationContext id_context = {context.task_scope, context.task_iteration,
                                  true};

  // check for any cached values.
  for (ValueInstance &value : state->cached_variables) {
    if (value.identifier == identifier &&
        value.context.is_reachable_by(context)) {
      return value.result->clone();
    }
  }

  // task-specific fields.
  if (context.task_scope) {
    auto local_it = this->context.task_scope->fields.find(identifier.content);
    if (local_it != this->context.task_scope->fields.end()) {
      ASTEvaluate ast_visitor = {id_context, state};
      std::unique_ptr<IValue> result =
          std::visit(ast_visitor, local_it->second.expression);
      if (result->immutable)
        state->cached_variables.push_back(
            ValueInstance{identifier, id_context, std::move(result->clone())});
      return result;
    }
  }

  // task iteration variable - this isn't cached for obvious reasons.
  if (context.task_iteration && context.task_scope)
    if (context.task_scope->iterator.content == identifier.content)
      return std::make_unique<IString>(*context.task_iteration,
                                       context.task_scope->reference, MUTABLE);

  // global fields.
  auto global_it = this->state->ast->fields.find(identifier.content);
  if (global_it != this->state->ast->fields.end()) {
    ASTEvaluate ast_visitor = {EvaluationContext{std::nullopt, std::nullopt},
                               state};
    std::unique_ptr<IValue> result =
        std::visit(ast_visitor, global_it->second.expression);
    // allow globbing: see comment above.
    EvaluationContext _context =
        EvaluationContext{std::nullopt, std::nullopt, true};
    if (result->immutable)
      state->cached_variables.push_back(
          ValueInstance{identifier, _context, std::move(result->clone())});
    return result;
  }

  ErrorHandler::halt(ENoMatchingIdentifier{identifier});
}

// note: we handle globbing **after** evaluating a formatted literal.
std::unique_ptr<IValue> ASTEvaluate::operator()(Literal const &literal) {
  return std::make_unique<IString>(literal.content, literal.reference,
                                   IMMUTABLE);
}

// helper method: handles globbing.
std::unique_ptr<IValue> expand_literal(IString input_istring) {
  size_t i_asterisk = input_istring.content.find('*');
  if (i_asterisk == std::string::npos) // no globbing.
    return std::make_unique<IString>(input_istring);

  // globbing is required.
  std::vector<std::string> paths;
  try {
    paths = Globbing::compute_paths(input_istring.content);
  } catch (LiteralsAdjacentWildcards &) {
    ErrorHandler::halt(EAdjacentWildcards{input_istring});
  }

  // convert to interpreter strings.
  std::vector<IString> contents;
  for (const std::string &str : paths) {
    contents.push_back(
        IString{str, input_istring.reference, input_istring.immutable});
  }

  if (contents.size() == 1)
    return std::make_unique<IString>(contents[0]);
  return std::make_unique<IList<IString>>(contents, input_istring.reference,
                                          input_istring.immutable);
}

// note: if literal includes a `*`, globbing will be used - this is
// expensive.
std::unique_ptr<IValue>
ASTEvaluate::operator()(FormattedLiteral const &formatted_literal) {
  // IString out;
  std::string out;
  bool immutable = true;
  for (ASTObject const &ast_obj : formatted_literal.contents) {
    ASTEvaluate ast_visitor = {context, state};
    std::unique_ptr<IValue> obj_result = std::visit(ast_visitor, ast_obj);
    // append a string.
    if (obj_result->get_type() == IType::IString) {
      out += dynamic_cast<IString &>(*obj_result).content;
      immutable &= obj_result->immutable;
    }
    // append a bool.
    else if (obj_result->get_type() == IType::IBool) {
      out += dynamic_cast<IBool &>(*obj_result) ? "true" : "false";
      immutable &= obj_result->immutable;
    }
    // append a list of strings.
    else if (obj_result->get_type() == IType::IList_IString) {
      IList<IString> obj_result_list =
          dynamic_cast<IList<IString> &>(*obj_result);
      for (size_t i = 0; i < obj_result_list.contents.size(); i++) {
        out += obj_result_list.contents[i].content;
        immutable &= obj_result->immutable;
        if (i < obj_result_list.contents.size() - 1)
          out += " ";
      }
    }
    // append a list of bools.
    else if (obj_result->get_type() == IType::IList_IBool) {
      IList<IBool> obj_result_list = dynamic_cast<IList<IBool> &>(*obj_result);
      for (size_t i = 0; i < obj_result_list.contents.size(); i++) {
        out += obj_result_list.contents[i] ? "true" : "false";
        immutable &= obj_result->immutable;
        if (i < obj_result_list.contents.size() - 1)
          out += " ";
      }
    }
  }

  if (context.use_globbing)
    return expand_literal(IString{out, formatted_literal.reference, immutable});
  else
    return std::make_unique<IString>(out, formatted_literal.reference,
                                     immutable);
}

std::unique_ptr<IValue> ASTEvaluate::operator()(List const &list) {
  assert(list.contents.size() > 0 && "attempt to evaluate empty list");

  // the first element dictates the list type as lists only store one type.
  ASTEvaluate ast_visitor = {context, state};
  std::unique_ptr<IValue> first_value =
      std::visit(ast_visitor, list.contents[0]);

  if (first_value->get_type() == IType::IString) {
    // evaluate list<string>
    IList<IString> ilist{{}, list.reference, first_value->immutable};
    ilist.contents.push_back(dynamic_cast<IString &>(*std::move(first_value)));

    // evaluate the rest of the list
    for (ASTObject const &ast_obj : list.contents | std::views::drop(1)) {
      std::unique_ptr<IValue> value = std::visit(ast_visitor, ast_obj);
      ilist.immutable &= value->immutable;
      if (value->get_type() == IType::IString) {
        ilist.contents.push_back(dynamic_cast<IString &>(*value));
        continue;
      } else if (value->get_type() == IType::IList_IString) {
        std::vector<IString> value_contents =
            dynamic_cast<IList<IString> &>(*value).contents;
        ilist.contents.insert(ilist.contents.end(), value_contents.begin(),
                              value_contents.end());
        continue;
      }
      ErrorHandler::halt(EListTypeMismatch{ilist, *value});
    }

    return std::make_unique<IList<IString>>(ilist);

  } else if (first_value->get_type() == IType::IBool) {
    // evaluate list<bool>
    IList<IBool> ilist{{}, list.reference, first_value->immutable};
    ilist.contents.push_back(dynamic_cast<IBool &>(*first_value));

    // evaluate the rest of the list
    for (ASTObject const &ast_obj : list.contents | std::views::drop(1)) {
      std::unique_ptr<IValue> value = std::visit(ast_visitor, ast_obj);
      ilist.immutable &= value->immutable;
      if (value->get_type() == IType::IBool) {
        ilist.contents.push_back(dynamic_cast<IBool &>(*value));
        continue;
      } else if (value->get_type() == IType::IList_IBool) {
        std::vector<IBool> value_contents =
            dynamic_cast<IList<IBool> &>(*value).contents;
        ilist.contents.insert(ilist.contents.end(), value_contents.begin(),
                              value_contents.end());
        continue;
      }
      ErrorHandler::halt(EListTypeMismatch{ilist, *value});
    }

    return std::make_unique<IList<IBool>>(ilist);

  } else if (first_value->get_type() == IType::IList_IString) {
    // evaluate list<string>
    IList<IString> ilist{{}, list.reference, first_value->immutable};
    std::vector<IString> first_value_contents =
        dynamic_cast<IList<IString> &>(*first_value).contents;
    ilist.contents.insert(ilist.contents.end(), first_value_contents.begin(),
                          first_value_contents.end());

    // evaluate the rest of the list
    for (ASTObject const &ast_obj : list.contents | std::views::drop(1)) {
      std::unique_ptr<IValue> value = std::visit(ast_visitor, ast_obj);
      ilist.immutable &= value->immutable;
      if (value->get_type() == IType::IString) {
        ilist.contents.push_back(dynamic_cast<IString &>(*value));
        continue;
      } else if (value->get_type() == IType::IList_IString) {
        std::vector<IString> value_contents =
            dynamic_cast<IList<IString> &>(*value).contents;
        ilist.contents.insert(ilist.contents.end(), value_contents.begin(),
                              value_contents.end());
        continue;
      }
      ErrorHandler::halt(EListTypeMismatch{ilist, *value});
    }

    return std::make_unique<IList<IString>>(ilist);

  } else if (first_value->get_type() == IType::IList_IBool) {
    // evaluate list<bool>
    IList<IBool> ilist{{}, list.reference, first_value->immutable};
    std::vector<IBool> first_value_contents =
        dynamic_cast<IList<IBool> &>(*first_value).contents;
    ilist.contents.insert(ilist.contents.end(), first_value_contents.begin(),
                          first_value_contents.end());

    // evaluate the rest of the list
    for (ASTObject const &ast_obj : list.contents | std::views::drop(1)) {
      std::unique_ptr<IValue> value = std::visit(ast_visitor, ast_obj);
      ilist.immutable &= value->immutable;
      if (value->get_type() == IType::IBool) {
        ilist.contents.push_back(dynamic_cast<IBool &>(*value));
        continue;
      } else if (value->get_type() == IType::IList_IBool) {
        std::vector<IBool> value_contents =
            dynamic_cast<IList<IBool> &>(*value).contents;
        ilist.contents.insert(ilist.contents.end(), value_contents.begin(),
                              value_contents.end());
        continue;
      }
      ErrorHandler::halt(EListTypeMismatch{ilist, *value});
    }

    return std::make_unique<IList<IBool>>(ilist);
  }

  assert(false && "invalid list type");
}

std::unique_ptr<IValue> ASTEvaluate::operator()(Boolean const &boolean) {
  return std::make_unique<IBool>(boolean.content, boolean.reference, true);
}

std::unique_ptr<IValue> ASTEvaluate::operator()(Replace const &replace) {
  // override use_globbing to false since the wildcards need to be handled here.
  EvaluationContext _context =
      EvaluationContext{context.task_scope, context.task_iteration, false};
  ASTEvaluate ast_visitor = {_context, state};
  std::unique_ptr<IValue> input = std::visit(ast_visitor, *replace.input);
  std::unique_ptr<IValue> filter = std::visit(ast_visitor, *replace.filter);
  std::unique_ptr<IValue> product = std::visit(ast_visitor, *replace.product);

  bool immutability =
      input->immutable && filter->immutable && product->immutable;

  // verify types.
  if (filter->get_type() != IType::IString)
    ErrorHandler::halt(EReplaceTypeMismatch{replace, *filter});

  if (product->get_type() != IType::IString)
    ErrorHandler::halt(EReplaceTypeMismatch{replace, *product});

  // fetch input.
  IList<IString> input_parsed = input->autocast<IList<IString>>();
  IList<IString> output_parsed{{}, replace.reference, immutability};

  std::string filter_str = dynamic_cast<IString &>(*filter).content;
  std::string product_str = dynamic_cast<IString &>(*product).content;

  // convert to pure strings first...
  std::vector<std::string> algorithm_input;
  for (const IString &istring : input_parsed.contents) {
    algorithm_input.push_back(istring.content);
  }

  std::vector<std::string> algorithm_output;
  try {
    algorithm_output =
        Wildcards::compute_replace(algorithm_input, filter_str, product_str);
  } catch (LiteralsAdjacentWildcards &) {
    ErrorHandler::halt(EAdjacentWildcards{dynamic_cast<IString &>(*filter)});
  } catch (LiteralsChunksLength &) {
    ErrorHandler::halt(EReplaceChunksLength{*product});
  }

  // convert back to interpreter types for tracking
  for (const std::string &str : algorithm_output) {
    output_parsed.contents.push_back(
        IString{str, replace.reference, immutability});
  }

  return std::make_unique<IList<IString>>(output_parsed);
}

Interpreter::Interpreter(AST &ast, Setup &setup) {
  this->state = std::make_shared<EvaluationState>();
  this->state->ast = std::make_unique<AST>(ast);
  this->state->setup = setup;
}
//     : m_ast(ast), m_setup(setup) {};

std::optional<Task> Interpreter::find_task(std::string identifier) {
  auto task_it = this->state->cached_tasks.find(identifier);
  if (task_it != this->state->cached_tasks.end())
    return *task_it->second;
  return std::nullopt;
}

std::optional<Field> Interpreter::find_field(std::string identifier,
                                             std::optional<Task> task) {
  // task-specific fields.
  if (task) {
    auto local_it = task->fields.find(identifier);
    if (local_it != task->fields.end())
      return local_it->second;
  }

  // global fields.
  auto global_it = this->state->ast->fields.find(identifier);
  if (global_it != this->state->ast->fields.end())
    return global_it->second;

  return std::nullopt;
}

// if there is no default and field does not exist, return std::nullopt.
// it is up to the caller to handle a missing mandatory field.
std::optional<std::unique_ptr<IValue>> Interpreter::evaluate_field_default(
    std::string identifier, EvaluationContext context,
    std::optional<std::unique_ptr<IValue>> default_value) {
  std::optional<Field> field = find_field(identifier, context.task_scope);
  if (!field) {
    return default_value;
  }
  return evaluate_ast_object(field->expression, context);
}

template <typename T>
std::optional<T>
Interpreter::evaluate_field_default_strict(std::string identifier,
                                           EvaluationContext context,
                                           std::optional<T> default_value) {
  std::optional<std::unique_ptr<IValue>> default_casted =
      default_value ? std::optional(std::make_unique<T>(*default_value))
                    : std::nullopt;
  std::optional<std::unique_ptr<IValue>> value =
      this->evaluate_field_default(identifier, context, std::move(default_casted));
  if (!value)
    return std::nullopt;
  return (*value)->autocast<T>();
}

std::optional<std::unique_ptr<IValue>>
Interpreter::evaluate_field_optional(std::string identifier,
                                     EvaluationContext context) {
  std::optional<Field> field = find_field(identifier, context.task_scope);
  if (!field)
    return std::nullopt;
  return evaluate_ast_object(field->expression, context);
}

template <typename T>
std::optional<T>
Interpreter::evaluate_field_optional_strict(std::string identifier,
                                            EvaluationContext context) {
  std::optional<std::unique_ptr<IValue>> value =
      evaluate_field_optional(identifier, context);
  if (!value)
    return std::nullopt;
  return (*value)->autocast<T>();
}

namespace PipelineJobs {
class BuildJob : public PipelineJob {
private:
  std::function<void(RunContext)> function_ptr;
  RunContext run_context;

public:
  BuildJob(std::function<void(RunContext)> function_ptr, RunContext run_context)
      : function_ptr(function_ptr) {
    this->run_context = run_context;
  }
  void compute() noexcept {
    try {
      ContextStack::import_local_stack(run_context.parent_frame_stack);
      FrameGuard frame{DependencyBuildFrame(run_context.task_iteration,
                                            run_context.task.reference)};
      function_ptr(run_context);
    } catch (...) {
      this->report_error();
    }
  }
};
} // namespace PipelineJobs

size_t
Interpreter::compute_latest_dependency_change(IList<IString> dependencies) {
  size_t latest_modification = 0;
  for (IString dependency : dependencies.contents) {
    std::optional<Task> task = find_task(dependency.to_string());

    std::optional<size_t> modified_i =
        Filesystem::get_file_timestamp(dependency.to_string());
    if (modified_i && latest_modification < modified_i)
      latest_modification = *modified_i;
    if (!task && modified_i) {
      continue;
    } else if (!task) {
      // file does not exist, nor is there a task.
      ErrorHandler::halt(EDependencyFailed{dependency, dependency.to_string()});
    }

    // context stack and recursion detection.
    FrameGuard frame{
        DependencyBuildFrame(dependency.to_string(), task->reference)};
    // protects against unbound recursion.
    bool recursive = StaticVerify::find_recursive_task(
        ContextStack::export_local_stack(), dependency.to_string());
    if (recursive) {
      // this_entry_handle->set_status(CLIEntryStatus::Failed);
      ErrorHandler::halt(ERecursiveTask{*task, dependency.to_string()});
    }

    std::optional<IList<IString>> dependencies_nested =
        evaluate_field_optional_strict<IList<IString>>(
            DEPENDS, {task, dependency.to_string()});
    if (dependencies_nested) {
      size_t modification_nested =
          compute_latest_dependency_change(*dependencies_nested);
      if (latest_modification < modification_nested)
        latest_modification = modification_nested;
    } else if (task) {
      // task exists but doesn't have any dependencies.
      return SIZE_MAX;
    }
  }
  return latest_modification;
}

void Interpreter::solve_dependencies(IList<IString> dependencies,
                                     std::string parent_iteration,
                                     bool parallel) {
  auto topography = parallel ? PipelineSchedulingTopography::Parallel
                             : PipelineSchedulingTopography::Sequential;
  auto scheduler =
      PipelineScheduler<PipelineSchedulingMethod::Unbound>(topography);

  for (IString dependency : dependencies.contents) {
    std::optional<Task> task = find_task(dependency.to_string());
    if (!task) {
      continue;
    }
    scheduler.schedule_job(std::make_shared<PipelineJobs::BuildJob>(
        [this](RunContext x) { return this->run_task(x); },
        RunContext{*task, dependency.to_string(), parent_iteration,
                   ContextStack::export_local_stack()}));
  }

  scheduler.send_and_await();
  if (scheduler.had_errors())
    ErrorHandler::trigger_report();
}

void Interpreter::run_task(RunContext run_context) {
  Task task = run_context.task;
  std::string task_iteration = run_context.task_iteration;
  std::optional<std::string> parent_iteration = run_context.parent_iteration;

  // check for recursive dependencies.
  bool recursive = StaticVerify::find_recursive_task(
      ContextStack::export_local_stack(), task_iteration);
  if (recursive) {
    // this_entry_handle->set_status(CLIEntryStatus::Failed);
    ErrorHandler::halt(ERecursiveTask{task, task_iteration});
  }

  std::shared_ptr<CLIEntryHandle> this_entry_handle;

  std::optional<IList<IString>> dependencies =
      evaluate_field_optional_strict<IList<IString>>(DEPENDS,
                                                     {task, task_iteration});

  // check for cached dependencies.
  bool dependency_build_required = false;
  if (dependencies) {
    size_t latest_dependency_change =
        compute_latest_dependency_change(*dependencies);
    std::optional<size_t> latest_this_change =
        Filesystem::get_file_timestamp(task_iteration);
    if (latest_this_change && *latest_this_change >= latest_dependency_change) {
      CLI::increment_skipped_tasks();
      return;
    }
    // task needs to be rebuilt.
    dependency_build_required = true;
  }

  // handle is generated here because we know for a fact that the task will need
  // to be rebuilt - we have already checked that it isn't cached.
  std::optional<IBool> visible = evaluate_field_default_strict<IBool>(
      VISIBLE, {task, task_iteration}, IBool(true, task.reference, IMMUTABLE));
  if (parent_iteration) {
    auto parent_entry_handle =
        CLI::get_entry_from_description(*parent_iteration);
    this_entry_handle =
        CLI::derive_entry_from(parent_entry_handle, task_iteration,
                               CLIEntryStatus::Scheduled, *visible);
  } else {
    this_entry_handle = CLI::generate_entry(
        task_iteration, CLIEntryStatus::Scheduled, *visible);
    this_entry_handle->set_highlighted(true);
  }

  if (dependency_build_required) {
    IBool parallel_default = IBool(false, task.reference, IMMUTABLE);
    // it is safe to unwrap the std::optional because we have a default value.
    IBool parallel = *evaluate_field_default_strict<IBool>(
        DEPENDS_PARALLEL, {task, task_iteration}, parallel_default);
    solve_dependencies(*dependencies, task_iteration, parallel);
  }

  // execution related fields.
  std::optional<IList<IString>> command_expr =
      evaluate_field_optional_strict<IList<IString>>(RUN,
                                                     {task, task_iteration});
  if (!command_expr) {
    this_entry_handle->set_status(CLIEntryStatus::Finished);
    return; // abstract task.
  }
  IBool run_parallel_default = IBool(false, task.reference, true);
  IBool run_parallel = *evaluate_field_default_strict<IBool>(
      RUN_PARALLEL, {task, task_iteration}, run_parallel_default);

  // execute task.
  // LOG_STANDARD(CYAN << "»" << RESET << " starting " << task_iteration);
  if (this->state->setup.dry_run)
    return;

  auto topography = run_parallel ? PipelineSchedulingTopography::Parallel
                                 : PipelineSchedulingTopography::Sequential;
  auto scheduler =
      PipelineScheduler<PipelineSchedulingMethod::Managed>(topography);

  for (IString cmdline : command_expr->contents) {
    scheduler.schedule_job(std::make_shared<PipelineJobs::ExecuteJob>(
        cmdline.to_string(), cmdline.reference, this_entry_handle));
  }
  scheduler.send_and_await();

  if (scheduler.had_errors()) {
    this_entry_handle->set_status(CLIEntryStatus::Failed);
    ErrorHandler::trigger_report();
  }

  if (this_entry_handle)
    this_entry_handle->set_status(CLIEntryStatus::Finished);
  // LOG_STANDARD(GREEN << "✓" << RESET << " finished " << task_iteration);
}

void Interpreter::build() {
  // precompute and cache task identifiers
  for (Task const &task : this->state->ast->tasks) {
    if (!this->state->topmost_task)
      this->state->topmost_task = task;

    std::unique_ptr<IValue> identifier =
        evaluate_ast_object(task.identifier, {std::nullopt, std::nullopt});
    IList<IString> identifiers = identifier->autocast<IList<IString>>();

    std::shared_ptr<Task> task_ptr = std::make_shared<Task>(task);
    std::vector<IString> keys = identifiers.contents;
    for (IString const &key_istr : keys) {
      auto duplicate_it = this->state->cached_tasks.find(key_istr.content);
      if (duplicate_it != this->state->cached_tasks.end())
        ErrorHandler::halt(
            EDuplicateTask{*duplicate_it->second, task, key_istr.content});
      this->state->cached_tasks[key_istr.content] = task_ptr;
    }
  }

  // find the task.
  if (this->state->ast->tasks.empty())
    ErrorHandler::halt(ENoTasks{});
  std::optional<Task> task;
  std::string task_iteration;
  if (this->state->setup.task) {
    task = find_task(*this->state->setup.task);
    task_iteration = *this->state->setup.task;
    if (!task) {
      ErrorHandler::halt(ETaskNotFound{task_iteration});
    }
  } else {
    task =
        this->state->topmost_task; // we've already checked that it's not empty
    std::unique_ptr<IValue> task_iteration_ivalue =
        evaluate_ast_object(task->identifier, {std::nullopt, std::nullopt});
    if (task_iteration_ivalue->get_type() != IType::IString) {
      ErrorHandler::halt(EAmbiguousTask{*task});
    }
    task_iteration = dynamic_cast<IString &>(
                         *evaluate_ast_object(task->identifier,
                                              {std::nullopt, std::nullopt}))
                         .to_string();
  }

  FrameGuard frame{EntryBuildFrame(task_iteration, task->reference)};
  run_task(RunContext{*task, task_iteration, std::nullopt, {}});
}
