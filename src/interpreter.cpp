#include "interpreter.hpp"
#include "errors.hpp"
#include "filesystem"
#include "format.hpp"
#include "literals.hpp"
#include "oslayer.hpp"
#include "pipeline.hpp"
#include "static_verify.hpp"
#include "tracking.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <filesystem>
#include <memory>
#include <ranges>
#include <thread>

#define DEPENDS "depends"
#define DEPENDS_PARALLEL "depends_parallel"
#define RUN "run"
#define RUN_PARALLEL "run_parallel"

#define IMMUTABLE true
#define MUTABLE false

template <typename T> bool is_immutable(T);
template <typename T> T autocast_strict(IValue);

template <> bool is_immutable(IValue value) {
  if (std::holds_alternative<IString>(value))
    return std::get<IString>(value).immutable;
  else if (std::holds_alternative<IBool>(value))
    return std::get<IBool>(value).immutable;
  else if (std::holds_alternative<IList<IString>>(value))
    return std::get<IList<IString>>(value).immutable;
  else if (std::holds_alternative<IList<IBool>>(value))
    return std::get<IList<IBool>>(value).immutable;
  assert(false && "invalid type for immutability checking");
}

template <> IList<IString> autocast_strict(IValue in) {
  if (std::holds_alternative<IList<IString>>(in))
    return std::get<IList<IString>>(in);
  else if (std::holds_alternative<IString>(in)) {
    return IList<IString>{{std::get<IString>(in)},
                          std::get<IString>(in).reference,
                          is_immutable(in)};
  }
  ErrorHandler::halt(EVariableTypeMismatch{in, "string or list<string>"});
}

template <> IString autocast_strict(IValue in) {
  if (std::holds_alternative<IString>(in))
    return std::get<IString>(in);
  ErrorHandler::halt(EVariableTypeMismatch{in, "string"});
}

template <> IList<IBool> autocast_strict(IValue in) {
  if (std::holds_alternative<IList<IBool>>(in))
    return std::get<IList<IBool>>(in);
  else if (std::holds_alternative<IBool>(in)) {
    return IList<IBool>{
        {std::get<IBool>(in)}, std::get<IBool>(in).reference, is_immutable(in)};
  }
  ErrorHandler::halt(EVariableTypeMismatch{in, "bool or list<bool>"});
}

// template <typename T> bool is_immutable(T value) { return value.immutable; }

template <> IBool autocast_strict(IValue in) {
  if (std::holds_alternative<IBool>(in))
    return std::get<IBool>(in);
  ErrorHandler::halt(EVariableTypeMismatch{in, "bool"});
}

// constructors & casts for internal data types.
IString::IString(Token token, bool immutable) : ICoreType(immutable) {
  assert(token.type == TokenType::Literal &&
         "attempt to construct IString from non-literal token");
  this->reference = token.reference;
  this->content = std::get<CTX_STR>(*token.context);
};
IString::IString(std::string content, StreamReference reference, bool immutable)
    : ICoreType(immutable) {
  this->reference = reference;
  this->content = content;
}
std::string IString::to_string() const { return (this->content); };
bool IString::operator==(IString const other) const {
  return this->content == other.content;
}

IBool::IBool(Token token, bool immutable) : ICoreType(immutable) {
  assert((token.type == TokenType::True || token.type == TokenType::False) &&
         "attempt to construct IBool from non-boolean token");
  this->reference = token.reference;
  this->content = (token.type == TokenType::True);
}
IBool::IBool(bool content, StreamReference reference, bool immutable)
    : ICoreType(immutable) {
  this->reference = reference;
  this->content = content;
}
bool IBool::operator==(IBool const other) const {
  return this->content == other.content;
}
IBool::operator bool() const { return (this->content); };

template <typename T>
IList<T>::IList(std::vector<T> contents, StreamReference reference,
                bool immutable)
    : ICoreType(immutable) {
  this->contents = contents;
  this->reference = reference;
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
  IValue operator()(Identifier const &identifier);
  IValue operator()(Literal const &literal);
  IValue operator()(FormattedLiteral const &formatted_literal);
  IValue operator()(List const &list);
  IValue operator()(Boolean const &boolean);
  IValue operator()(Replace const &replace);
};

IValue Interpreter::evaluate_ast_object(ASTObject ast_object,
                                        EvaluationContext context) {
  // evaluation visitor can amend shared data in the state.
  std::lock_guard<std::mutex> guard(evaluation_lock);
  IValue value = std::visit(ASTEvaluate{context, this->state}, ast_object);
  return value;
}

IValue ASTEvaluate::operator()(Identifier const &identifier) {
  FrameGuard frame(
      IdentifierEvaluateFrame(identifier.content, identifier.reference));

  // check whether we're shooting ourselves and the stack in the foot.
  bool recursive = StaticVerify::find_recursive_variable(
      ContextStack::dump_flattened_stack(), identifier.content);
  if (recursive)
    ErrorHandler::halt(ERecursiveVariable{identifier});

  // any identifier will *always* have globbing enabled. if a replacement
  // operator attempts to evaluate an ast object, it will override the globbing
  // to false, but if the value it's referencing is behind another variable
  // initialization, globbing should be enabled to avoid unintuitive errors.
  EvaluationContext id_context = {context.task_scope, context.task_iteration,
                                  true};

  // check for any cached values.
  for (ValueInstance value : state->cached_variables) {
    if (value.identifier == identifier &&
        value.context.is_reachable_by(context)) {
      return value.result;
    }
  }

  // task-specific fields.
  if (context.task_scope) {
    auto local_it = this->context.task_scope->fields.find(identifier.content);
    if (local_it != this->context.task_scope->fields.end()) {
      ASTEvaluate ast_visitor = {id_context, state};
      IValue result = std::visit(ast_visitor, local_it->second.expression);
      if (is_immutable(result))
        state->cached_variables.push_back(
            ValueInstance{identifier, id_context, result});
      return result;
    }
  }

  // task iteration variable - this isn't cached for obvious reasons.
  if (context.task_iteration && context.task_scope)
    if (context.task_scope->iterator.content == identifier.content)
      return IString(*context.task_iteration, context.task_scope->reference,
                     MUTABLE);

  // global fields.
  auto global_it = this->state->ast->fields.find(identifier.content);
  if (global_it != this->state->ast->fields.end()) {
    ASTEvaluate ast_visitor = {EvaluationContext{std::nullopt, std::nullopt},
                               state};
    IValue result = std::visit(ast_visitor, global_it->second.expression);
    // allow globbing: see comment above.
    EvaluationContext _context =
        EvaluationContext{std::nullopt, std::nullopt, true};
    if (is_immutable(result))
      state->cached_variables.push_back(
          ValueInstance{identifier, _context, result});
    return result;
  }

  ErrorHandler::halt(ENoMatchingIdentifier{identifier});
}

// note: we handle globbing **after** evaluating a formatted literal.
IValue ASTEvaluate::operator()(Literal const &literal) {
  return IString(literal.content, literal.reference, IMMUTABLE);
}

// helper method: handles globbing.
IValue expand_literal(IString input_istring) {
  size_t i_asterisk = input_istring.content.find('*');
  if (i_asterisk == std::string::npos) // no globbing.
    return {input_istring};

  // globbing is required.
  std::vector<std::string> paths;
  try {
    paths = Globbing::compute_paths(input_istring.content);
  } catch (LiteralsAdjacentWildcards &_) {
    ErrorHandler::halt(EAdjacentWildcards{input_istring});
  }

  // convert to interpreter strings.
  std::vector<IString> contents;
  for (const std::string &str : paths) {
    contents.push_back(
        IString{str, input_istring.reference, input_istring.immutable});
  }

  if (contents.size() == 1)
    return {contents[0]};
  return IList{contents, input_istring.reference, input_istring.immutable};
}

// note: if literal includes a `*`, globbing will be used - this is
// expensive.
IValue ASTEvaluate::operator()(FormattedLiteral const &formatted_literal) {
  // IString out;
  std::string out;
  bool immutable = true;
  for (ASTObject const &ast_obj : formatted_literal.contents) {
    ASTEvaluate ast_visitor = {context, state};
    IValue obj_result = std::visit(ast_visitor, ast_obj);
    // append a string.
    if (std::holds_alternative<IString>(obj_result)) {
      out += std::get<IString>(obj_result).content;
      immutable &= is_immutable(obj_result);
    }
    // append a bool.
    else if (std::holds_alternative<IBool>(obj_result)) {
      out += (std::get<IBool>(obj_result) ? "true" : "false");
      immutable &= is_immutable(obj_result);
    }
    // append a list of strings.
    else if (std::holds_alternative<IList<IString>>(obj_result)) {
      IList<IString> obj_result_list = std::get<IList<IString>>(obj_result);
      for (size_t i = 0; i < obj_result_list.contents.size(); i++) {
        out += obj_result_list.contents[i].content;
        immutable &= is_immutable(obj_result);
        if (i < obj_result_list.contents.size() - 1)
          out += " ";
      }
    }
    // append a list of bools.
    else if (std::holds_alternative<IList<IBool>>(obj_result)) {
      IList<IBool> obj_result_list = std::get<IList<IBool>>(obj_result);
      for (size_t i = 0; i < obj_result_list.contents.size(); i++) {
        out += obj_result_list.contents[i] ? "true" : "false";
        immutable &= is_immutable(obj_result);
        if (i < obj_result_list.contents.size() - 1)
          out += " ";
      }
    }
  }

  if (context.use_globbing)
    return expand_literal(IString{out, formatted_literal.reference, immutable});
  else
    return IString{out, formatted_literal.reference, immutable};
}

IValue ASTEvaluate::operator()(List const &list) {
  assert(list.contents.size() > 0 && "attempt to evaluate empty list");

  // the first element dictates the list type as lists only store one type.
  ASTEvaluate ast_visitor = {context, state};
  IValue first_value = std::visit(ast_visitor, list.contents[0]);

  if (std::holds_alternative<IString>(first_value)) {
    // evaluate list<string>
    IList<IString> ilist{{}, list.reference, is_immutable(first_value)};
    ilist.contents.push_back(std::get<IString>(first_value));

    // evaluate the rest of the list
    for (ASTObject const &ast_obj : list.contents | std::views::drop(1)) {
      IValue value = std::visit(ast_visitor, ast_obj);
      ilist.immutable &= is_immutable(value);
      if (std::holds_alternative<IString>(value)) {
        ilist.contents.push_back(std::get<IString>(value));
        continue;
      } else if (std::holds_alternative<IList<IString>>(value)) {
        std::vector<IString> value_contents =
            std::get<IList<IString>>(value).contents;
        ilist.contents.insert(ilist.contents.end(), value_contents.begin(),
                              value_contents.end());
        continue;
      }
      ErrorHandler::halt(EListTypeMismatch{ilist, value});
    }

    return ilist;

  } else if (std::holds_alternative<IBool>(first_value)) {
    // evaluate list<bool>
    IList<IBool> ilist{{}, list.reference, is_immutable(first_value)};
    ilist.contents.push_back(std::get<IBool>(first_value));

    // evaluate the rest of the list
    for (ASTObject const &ast_obj : list.contents | std::views::drop(1)) {
      IValue value = std::visit(ast_visitor, ast_obj);
      ilist.immutable &= is_immutable(value);
      if (std::holds_alternative<IBool>(value)) {
        ilist.contents.push_back(std::get<IBool>(value));
        continue;
      } else if (std::holds_alternative<IList<IBool>>(value)) {
        std::vector<IBool> value_contents =
            std::get<IList<IBool>>(value).contents;
        ilist.contents.insert(ilist.contents.end(), value_contents.begin(),
                              value_contents.end());
        continue;
      }
      ErrorHandler::halt(EListTypeMismatch{ilist, value});
    }

    return ilist;

  } else if (std::holds_alternative<IList<IString>>(first_value)) {
    // evaluate list<string>
    IList<IString> ilist{{}, list.reference, is_immutable(first_value)};
    std::vector<IString> first_value_contents =
        std::get<IList<IString>>(first_value).contents;
    ilist.contents.insert(ilist.contents.end(), first_value_contents.begin(),
                          first_value_contents.end());

    // evaluate the rest of the list
    for (ASTObject const &ast_obj : list.contents | std::views::drop(1)) {
      IValue value = std::visit(ast_visitor, ast_obj);
      ilist.immutable &= is_immutable(value);
      if (std::holds_alternative<IString>(value)) {
        ilist.contents.push_back(std::get<IString>(value));
        continue;
      } else if (std::holds_alternative<IList<IString>>(value)) {
        std::vector<IString> value_contents =
            std::get<IList<IString>>(value).contents;
        ilist.contents.insert(ilist.contents.end(), value_contents.begin(),
                              value_contents.end());
        continue;
      }
      ErrorHandler::halt(EListTypeMismatch{ilist, value});
    }

    return ilist;

  } else if (std::holds_alternative<IList<IBool>>(first_value)) {
    // evaluate list<bool>
    IList<IBool> ilist{{}, list.reference, is_immutable(first_value)};
    std::vector<IBool> first_value_contents =
        std::get<IList<IBool>>(first_value).contents;
    ilist.contents.insert(ilist.contents.end(), first_value_contents.begin(),
                          first_value_contents.end());

    // evaluate the rest of the list
    for (ASTObject const &ast_obj : list.contents | std::views::drop(1)) {
      IValue value = std::visit(ast_visitor, ast_obj);
      ilist.immutable &= is_immutable(value);
      if (std::holds_alternative<IBool>(value)) {
        ilist.contents.push_back(std::get<IBool>(value));
        continue;
      } else if (std::holds_alternative<IList<IBool>>(value)) {
        std::vector<IBool> value_contents =
            std::get<IList<IBool>>(value).contents;
        ilist.contents.insert(ilist.contents.end(), value_contents.begin(),
                              value_contents.end());
        continue;
      }
      ErrorHandler::halt(EListTypeMismatch{ilist, value});
    }

    return ilist;
  }

  assert(false && "invalid list type");
}

IValue ASTEvaluate::operator()(Boolean const &boolean) {
  return {IBool(boolean.content, boolean.reference, true)};
}

IValue ASTEvaluate::operator()(Replace const &replace) {
  // override use_globbing to false since the wildcards need to be handled here.
  EvaluationContext _context =
      EvaluationContext{context.task_scope, context.task_iteration, false};
  ASTEvaluate ast_visitor = {_context, state};
  IValue input = std::visit(ast_visitor, *replace.input);
  IValue filter = std::visit(ast_visitor, *replace.filter);
  IValue product = std::visit(ast_visitor, *replace.product);

  bool immutability =
      is_immutable(input) && is_immutable(filter) && is_immutable(product);

  // verify types.
  if (!std::holds_alternative<IString>(filter))
    ErrorHandler::halt(EReplaceTypeMismatch{replace, filter});

  if (!std::holds_alternative<IString>(product))
    ErrorHandler::halt(EReplaceTypeMismatch{replace, product});

  // fetch input.
  IList<IString> input_parsed = autocast_strict<IList<IString>>(input);
  IList<IString> output_parsed{{}, replace.reference, immutability};

  std::string filter_str = std::get<IString>(filter).content;
  std::string product_str = std::get<IString>(product).content;

  // convert to pure strings first...
  std::vector<std::string> algorithm_input;
  for (const IString &istring : input_parsed.contents) {
    algorithm_input.push_back(istring.content);
  }

  std::vector<std::string> algorithm_output;
  try {
    algorithm_output =
        Wildcards::compute_replace(algorithm_input, filter_str, product_str);
  } catch (LiteralsAdjacentWildcards &_) {
    ErrorHandler::halt(EAdjacentWildcards{std::get<IString>(filter)});
  } catch (LiteralsChunksLength &_) {
    ErrorHandler::halt(EReplaceChunksLength{product});
  }

  // convert back to interpreter types for tracking
  for (const std::string &str : algorithm_output) {
    output_parsed.contents.push_back(
        IString{str, replace.reference, immutability});
  }

  return output_parsed;
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
std::optional<IValue>
Interpreter::evaluate_field_default(std::string identifier,
                                    EvaluationContext context,
                                    std::optional<IValue> default_value) {
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
  std::optional<IValue> value =
      this->evaluate_field_default(identifier, context, default_value);
  if (!value)
    return std::nullopt;
  return autocast_strict<T>(*value);
}

std::optional<IValue>
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
  std::optional<IValue> value = evaluate_field_optional(identifier, context);
  if (!value)
    return std::nullopt;
  return autocast_strict<T>(*value);
}

using RunTaskType = std::function<void(Task, std::string)>;

namespace PipelineJobs {
template <typename F> class BuildJob : public PipelineJob {
private:
  F function_ptr;
  Task task;
  std::string task_iteration;

public:
  BuildJob(F function_ptr, Task task, std::string task_iteration)
      : function_ptr(function_ptr) {
    this->task = task;
    this->task_iteration = task_iteration;
  }
  void compute() noexcept {
    try {
      function_ptr(task, task_iteration);
    } catch (...) {
      this->report_error();
    }
  }
};
} // namespace PipelineJobs

DependencyStatus Interpreter::solve_dependencies(IList<IString> dependencies,
                                                 bool parallel) {
  std::optional<size_t> latest_modification;
  auto topography = parallel ? PipelineSchedulingTopography::Parallel
                             : PipelineSchedulingTopography::Sequential;
  auto scheduler =
      PipelineScheduler<PipelineSchedulingMethod::Unbound>(topography);

  for (IString dependency : dependencies.contents) {
    std::optional<Task> task = find_task(dependency.to_string());
    std::optional<size_t> modified_i =
        OSLayer::get_file_timestamp(dependency.to_string());
    if (!latest_modification ||
        (modified_i && latest_modification < modified_i))
      latest_modification = modified_i;
    if (!task) {
      continue;
    }
    scheduler.schedule_job(
        std::make_shared<PipelineJobs::BuildJob<RunTaskType>>(
            [this](Task x, std::string y) { return this->run_task(x, y); },
            *task, dependency.to_string()));
  }

  scheduler.send_and_await();
  bool success = !scheduler.had_errors();

  return {success, latest_modification};
}

void Interpreter::run_task(Task task, std::string task_iteration) {

  // check for recursive dependencies.
  bool recursive = StaticVerify::find_recursive_task(
      ContextStack::dump_flattened_stack(), task_iteration);
  if (recursive)
    ErrorHandler::halt(ERecursiveTask{task, task_iteration});

  // solve dependencies.
  std::optional<IList<IString>> dependencies =
      evaluate_field_optional_strict<IList<IString>>(DEPENDS,
                                                     {task, task_iteration});
  std::optional<size_t> dep_modified;
  if (dependencies) {
    IBool parallel_default = IBool(false, task.reference, IMMUTABLE);
    // it is safe to unwrap the std::optional because we have a default value.
    IBool parallel = *evaluate_field_default_strict<IBool>(
        DEPENDS_PARALLEL, {task, task_iteration}, parallel_default);
    DependencyStatus dep_stat = solve_dependencies(*dependencies, parallel);
    dep_modified = dep_stat.modified;
    if (!dep_stat.success)
      ErrorHandler::trigger_report();
  }

  // check for changes.
  std::optional<size_t> this_modified =
      OSLayer::get_file_timestamp(task_iteration);
  if (this_modified && dep_modified && *this_modified >= *dep_modified) {
    LOG_STANDARD("•" << RESET << " skipped " << task_iteration);
    return;
  }

  // execution related fields.
  std::optional<IList<IString>> command_expr =
      evaluate_field_optional_strict<IList<IString>>(RUN,
                                                     {task, task_iteration});
  if (!command_expr) {
    return; // abstract task.
  }
  IBool run_parallel_default = IBool(false, task.reference, true);
  IBool run_parallel = *evaluate_field_default_strict<IBool>(
      RUN_PARALLEL, {task, task_iteration}, run_parallel_default);

  // execute task.
  LOG_STANDARD(CYAN << "»" << RESET << " starting " << task_iteration);
  if (this->state->setup.dry_run)
    return;

  auto topography = run_parallel ? PipelineSchedulingTopography::Parallel
                                 : PipelineSchedulingTopography::Sequential;
  auto scheduler =
      PipelineScheduler<PipelineSchedulingMethod::Managed>(topography);

  for (IString cmdline : command_expr->contents) {
    scheduler.schedule_job(std::make_shared<PipelineJobs::ExecuteJob>(
        cmdline.to_string(), cmdline.reference));
  }
  scheduler.send_and_await();

  if (scheduler.had_errors())
    ErrorHandler::trigger_report();

  LOG_STANDARD(GREEN << "✓" << RESET << " finished " << task_iteration);
}

void Interpreter::build() {

  // precompute and cache task identifiers
  for (Task const &task : this->state->ast->tasks) {
    if (!this->state->topmost_task)
      this->state->topmost_task = task;

    IValue identifier =
        evaluate_ast_object(task.identifier, {std::nullopt, std::nullopt});
    IList<IString> identifiers = autocast_strict<IList<IString>>(identifier);

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
    IValue task_iteration_ivalue =
        evaluate_ast_object(task->identifier, {std::nullopt, std::nullopt});
    if (!std::holds_alternative<IString>(task_iteration_ivalue)) {
      ErrorHandler::halt(EAmbiguousTask{*task});
    }
    task_iteration =
        std::get<IString>(
            evaluate_ast_object(task->identifier, {std::nullopt, std::nullopt}))
            .to_string();
  }

  LOG_STANDARD("⧗ building " << CYAN << task_iteration << RESET);
  // todo: error checking is also required here in case task doesn't exist.
  FrameGuard frame{EntryBuildFrame(task_iteration, task->reference)};
  run_task(*task, task_iteration);
}
