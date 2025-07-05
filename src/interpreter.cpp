#include "interpreter.hpp"
#include "errors.hpp"
#include "filesystem"
#include "format.hpp"
#include "oslayer.hpp"
#include "tracking.hpp"
#include <atomic>
#include <cassert>
#include <filesystem>
#include <memory>
#include <thread>

#define DEPENDS "depends"
#define DEPENDS_PARALLEL "depends_parallel"
#define RUN "run"
#define RUN_PARALLEL "run_parallel"

// constructors & casts for internal data types.
IString::IString(Token token) {
  assert(token.type == TokenType::Literal &&
         "attempt to construct IString from non-literal token");
  this->reference = token.reference;
  this->content = std::get<CTX_STR>(*token.context);
};
IString::IString(std::string content, StreamReference reference) {
  this->reference = reference;
  this->content = content;
}
std::string IString::toString() const { return (this->content); };
bool IString::operator==(IString const other) const {
  return this->content == other.content;
}

IBool::IBool(Token token) {
  assert((token.type == TokenType::True || token.type == TokenType::False) &&
         "attempt to construct IBool from non-boolean token");
  this->reference = token.reference;
  this->content = (token.type == TokenType::True);
}
IBool::IBool(bool content, StreamReference reference) {
  this->reference = reference;
  this->content = content;
}
bool IBool::operator==(IBool const other) const {
  return this->content == other.content;
}
IBool::operator bool() const { return (this->content); };

IList::IList(std::variant<std::vector<IString>, std::vector<IBool>> contents,
             StreamReference reference) {
  if (std::holds_alternative<std::vector<IString>>(contents)) {
    std::vector<IString> contents_istring =
        std::get<std::vector<IString>>(contents);
    this->contents = contents_istring;
    this->reference = reference;
  } else if (std::holds_alternative<std::vector<IBool>>(contents)) {
    std::vector<IBool> contents_ibool = std::get<std::vector<IBool>>(contents);
    this->contents = contents_ibool;
    this->reference = reference;
  }
}
bool IList::holds_istring() const {
  return (this->contents.index() == ILIST_STR);
}
bool IList::holds_ibool() const {
  return (this->contents.index() == ILIST_BOOL);
}
bool IList::operator==(IList const other) const {
  return this->contents == other.contents;
}

// returns true if, and only if, the passed context can reach the caller.
bool EvaluationContext::context_verify(EvaluationContext const other) const {
  if (this->task_scope == other.task_scope &&
      this->use_globbing == other.use_globbing)
    return true;
  if (this->task_scope == std::nullopt &&
      this->use_globbing == other.use_globbing)
    return true;
  return false;
}

// visitor that evaluates an AST object recursively.
struct ASTEvaluate {
  AST &ast;
  EvaluationContext context;
  std::shared_ptr<EvaluationState> state;
  IValue operator()(Identifier const &identifier);
  IValue operator()(Literal const &literal);
  IValue operator()(FormattedLiteral const &formatted_literal);
  IValue operator()(List const &list);
  IValue operator()(Boolean const &boolean);
  IValue operator()(Replace const &replace);
};

IValue
Interpreter::evaluate_ast_object(ASTObject ast_object, AST &ast,
                                 EvaluationContext context,
                                 std::shared_ptr<EvaluationState> state) {
  // evaluation visitor can amend shared data in the state.
  std::lock_guard<std::mutex> guard(evaluation_lock);
  IValue value = std::visit(ASTEvaluate{ast, context, state}, ast_object);
  return value;
}

IValue ASTEvaluate::operator()(Identifier const &identifier) {
  FrameGuard frame(
      IdentifierEvaluateFrame(identifier.content, identifier.reference));

  // check for any cached values.
  for (ValueInstance value : state->values) {
    if (value.identifier == identifier &&
        value.context.context_verify(context)) {
      return value.result;
    }
  }

  // task-specific fields.
  if (context.task_scope) {
    for (Field const &field : context.task_scope->fields) {
      if (field.identifier.content == identifier.content) {
        ASTEvaluate ast_visitor = {ast, context, state};
        IValue result = std::visit(ast_visitor, field.expression);
        if (result.immutable)
          state->values.push_back(ValueInstance{identifier, context, result});
        return result;
      }
    }
  }

  // task iteration variable - this isn't cached for obvious reasons.
  if (context.task_iteration && context.task_scope)
    if (context.task_scope->iterator.content == identifier.content)
      return {IString(*context.task_iteration, context.task_scope->reference),
              false};

  // global fields.
  for (Field const &field : ast.fields) {
    if (field.identifier.content == identifier.content) {
      ASTEvaluate ast_visitor = {
          ast, EvaluationContext{std::nullopt, std::nullopt}, state};
      IValue result = std::visit(ast_visitor, field.expression);
      EvaluationContext _context =
          EvaluationContext{std::nullopt, std::nullopt, context.use_globbing};
      if (result.immutable)
        state->values.push_back(ValueInstance{identifier, _context, result});
      return result;
    }
  }

  ErrorHandler::halt(ENoMatchingIdentifier{identifier});
}

// note: we handle globbing **after** evaluating a formatted literal.
IValue ASTEvaluate::operator()(Literal const &literal) {
  return {IString(literal.content, literal.reference)};
}

// helper method: handles globbing.
IValue expand_literal(IString input_istring, bool immutable) {
  size_t i_asterisk = input_istring.content.find('*');
  if (i_asterisk == std::string::npos) // no globbing.
    return {input_istring};

  // globbing is required.
  std::string prefix = input_istring.content.substr(0, i_asterisk);
  std::string suffix = input_istring.content.substr(i_asterisk + 1);

  // acts as a string vector.
  std::vector<IString> contents;

  for (std::filesystem::directory_entry const &dir_entry :
       std::filesystem::recursive_directory_iterator(".")) {
    std::string dir_path = dir_entry.path().string();
    size_t i_prefix = dir_path.find(prefix);
    size_t i_suffix = dir_path.find(suffix);
    if (prefix.empty() && i_suffix != std::string::npos)
      contents.push_back(IString(dir_path, input_istring.reference));
    else if (suffix.empty() && i_prefix != std::string::npos)
      contents.push_back(IString(dir_path, input_istring.reference));
    else if (!prefix.empty() && !suffix.empty() &&
             i_prefix != std::string::npos && i_suffix != std::string::npos &&
             i_prefix < i_suffix)
      contents.push_back(IString(dir_path, input_istring.reference));
  }

  if (contents.size() == 1)
    return {contents[0]};
  return {IList{contents, input_istring.reference}, immutable};
}

// note: if literal includes a `*`, globbing will be used - this is
// expensive.
IValue ASTEvaluate::operator()(FormattedLiteral const &formatted_literal) {
  // IString out;
  std::string out;
  bool immutable = true;
  for (ASTObject const &ast_obj : formatted_literal.contents) {
    ASTEvaluate ast_visitor = {ast, context, state};
    IValue obj_result = std::visit(ast_visitor, ast_obj);
    // append a string.
    if (std::holds_alternative<IString>(obj_result.value)) {
      out += std::get<IString>(obj_result.value).content;
      immutable &= obj_result.immutable;
    }
    // append a bool.
    else if (std::holds_alternative<IBool>(obj_result.value)) {
      out += (std::get<IBool>(obj_result.value) ? "true" : "false");
      immutable &= obj_result.immutable;
    }
    // append a list of strings.
    else if (std::holds_alternative<IList>(obj_result.value) &&
             std::get<IList>(obj_result.value).contents.index() == ILIST_STR) {
      IList obj_result_list = std::get<IList>(obj_result.value);
      for (size_t i = 0;
           i < std::get<ILIST_STR>(obj_result_list.contents).size(); i++) {
        out += std::get<ILIST_STR>(obj_result_list.contents)[i].content;
        immutable &= obj_result.immutable;
        if (i < std::get<ILIST_STR>(obj_result_list.contents).size() - 1)
          out += " ";
      }
    }
    // append a list of bools.
    else if (std::holds_alternative<IList>(obj_result.value) &&
             std::get<IList>(obj_result.value).contents.index() == ILIST_BOOL) {
      IList obj_result_list = std::get<IList>(obj_result.value);
      for (size_t i = 0;
           i < std::get<ILIST_BOOL>(obj_result_list.contents).size(); i++) {
        out += (std::get<ILIST_BOOL>(obj_result_list.contents)[i] ? "true"
                                                                  : "false");
        immutable &= obj_result.immutable;
        if (i < std::get<ILIST_BOOL>(obj_result_list.contents).size() - 1)
          out += " ";
      }
    }
  }

  if (context.use_globbing)
    return expand_literal(IString{out, formatted_literal.reference}, immutable);
  else
    return {IString{out, formatted_literal.reference}, immutable};
}

IValue ASTEvaluate::operator()(List const &list) {
  assert(list.contents.size() > 0 && "attempt to evaluate empty list");

  IList out = IList{{}, list.reference};
  bool immutable = true;
  // infer the list type. todo: consider a cleaner solution.
  ASTEvaluate ast_visitor = {ast, context, state};
  IValue _obj_result = std::visit(ast_visitor, list.contents[0]);
  out.reference = list.reference; // std::visit(QBVisitStreamReference{},
                                  // _obj_result.value);
  if (std::holds_alternative<IString>(_obj_result.value)) {
    out.contents = std::vector<IString>();
  } else if (std::holds_alternative<IBool>(_obj_result.value)) {
    out.contents = std::vector<IBool>();
  } else if (std::holds_alternative<IList>(_obj_result.value)) {
    IList __obj_ilist = std::get<IList>(_obj_result.value);
    if (std::holds_alternative<std::vector<IString>>(__obj_ilist.contents))
      out.contents = std::vector<IString>();
    else if (std::holds_alternative<std::vector<IBool>>(__obj_ilist.contents))
      out.contents = std::vector<IBool>();
  }

  // add the early evaluated first element.
  if (std::holds_alternative<IString>(_obj_result.value)) {
    std::get<ILIST_STR>(out.contents)
        .push_back(std::get<IString>(_obj_result.value));
    immutable &= _obj_result.immutable;
  } else if (std::holds_alternative<IBool>(_obj_result.value)) {
    std::get<ILIST_BOOL>(out.contents)
        .push_back(std::get<IBool>(_obj_result.value));
    immutable &= _obj_result.immutable;
  } else if (std::holds_alternative<IList>(_obj_result.value)) {
    IList obj_result_ilist = std::get<IList>(_obj_result.value);
    if (obj_result_ilist.holds_istring() && out.holds_istring()) {
      std::get<ILIST_STR>(out.contents)
          .insert(std::get<ILIST_STR>(out.contents).begin(),
                  std::get<ILIST_STR>(obj_result_ilist.contents).begin(),
                  std::get<ILIST_STR>(obj_result_ilist.contents).end());
      immutable &= _obj_result.immutable;
    } else if (obj_result_ilist.holds_ibool() && out.holds_ibool()) {
      std::get<ILIST_BOOL>(out.contents)
          .insert(std::get<ILIST_BOOL>(out.contents).begin(),
                  std::get<ILIST_BOOL>(obj_result_ilist.contents).begin(),
                  std::get<ILIST_BOOL>(obj_result_ilist.contents).end());
      immutable &= _obj_result.immutable;
    }
  }

  for (size_t i = 1; i < list.contents.size(); i++) {
    ASTObject ast_obj = list.contents[i];
    ASTEvaluate ast_visitor = {ast, context, state};
    IValue obj_result = std::visit(ast_visitor, ast_obj);
    if (std::holds_alternative<IString>(obj_result.value)) {
      if (out.holds_istring()) {
        std::get<ILIST_STR>(out.contents)
            .push_back(std::get<IString>(obj_result.value));
        immutable &= _obj_result.immutable;
      } else
        ErrorHandler::halt(EListTypeMismatch{out, obj_result});
    } else if (std::holds_alternative<IBool>(obj_result.value)) {
      if (out.holds_ibool()) {
        std::get<ILIST_BOOL>(out.contents)
            .push_back(std::get<IBool>(obj_result.value));
        immutable &= _obj_result.immutable;
      } else
        ErrorHandler::halt(EListTypeMismatch{out, obj_result});
    } else if (std::holds_alternative<IList>(obj_result.value)) {
      IList obj_result_ilist = std::get<IList>(obj_result.value);
      if (obj_result_ilist.holds_istring() && out.holds_istring()) {
        std::get<ILIST_STR>(out.contents)
            .insert(std::get<ILIST_STR>(out.contents).end(),
                    std::get<ILIST_STR>(obj_result_ilist.contents).begin(),
                    std::get<ILIST_STR>(obj_result_ilist.contents).end());
        immutable &= _obj_result.immutable;
      } else if (obj_result_ilist.holds_ibool() && out.holds_ibool()) {
        std::get<ILIST_BOOL>(out.contents)
            .insert(std::get<ILIST_BOOL>(out.contents).end(),
                    std::get<ILIST_BOOL>(obj_result_ilist.contents).begin(),
                    std::get<ILIST_BOOL>(obj_result_ilist.contents).end());
        immutable &= _obj_result.immutable;
      } else {
        ErrorHandler::halt(EListTypeMismatch{out, obj_result});
      }
    }
  }

  return {out, immutable};
}

IValue ASTEvaluate::operator()(Boolean const &boolean) {
  return {IBool(boolean.content, boolean.reference)};
}

struct Wildcard {};
using StringComponent = std::variant<Wildcard, std::string>;

IValue ASTEvaluate::operator()(Replace const &replace) {
  // override use_globbing to false since the wildcards need to be handled here.
  EvaluationContext _context =
      EvaluationContext{context.task_scope, context.task_iteration, false};
  ASTEvaluate ast_visitor = {ast, _context, state};
  IValue input = std::visit(ast_visitor, *replace.input);
  IValue filter = std::visit(ast_visitor, *replace.filter);
  IValue product = std::visit(ast_visitor, *replace.product);

  bool immutable = input.immutable && filter.immutable && product.immutable;

  // verify types.
  if (!std::holds_alternative<IString>(filter.value))
    ErrorHandler::halt(EReplaceTypeMismatch{replace, filter});

  if (!std::holds_alternative<IString>(product.value))
    ErrorHandler::halt(EReplaceTypeMismatch{replace, product});

  // fetch input.
  IList input_parsed{{}, replace.reference};
  IList output_parsed{{}, replace.reference};
  if (std::holds_alternative<IList>(input.value) &&
      std::get<IList>(input.value).holds_istring())
    input_parsed = std::get<IList>(input.value);
  else if (std::holds_alternative<IString>(input.value))
    std::get<ILIST_STR>(input_parsed.contents)
        .push_back(std::get<IString>(input.value));
  else
    ErrorHandler::halt(EReplaceTypeMismatch{replace, input});

  std::string filter_str = std::get<IString>(filter.value).content;
  std::string product_str = std::get<IString>(product.value).content;

  // preprocess filter and product string to simplify the matching algorithm.
  std::vector<StringComponent> filter_parsed;
  std::string str_buf;
  for (size_t i = 0; i < filter_str.size(); i++) {
    if (filter_str[i] == '*') {
      if (!str_buf.empty()) {
        filter_parsed.push_back(str_buf);
        str_buf = "";
      }
      filter_parsed.push_back(Wildcard{});
    } else
      str_buf += filter_str[i];
  }
  if (!str_buf.empty()) {
    filter_parsed.push_back(str_buf);
    str_buf = "";
  }

  // preprocess product string
  std::vector<StringComponent> product_parsed;
  for (size_t i = 0; i < product_str.size(); i++) {
    if (product_str[i] == '*') {
      if (!str_buf.empty()) {
        product_parsed.push_back(str_buf);
        str_buf = "";
      }
      product_parsed.push_back(Wildcard{});
    } else
      str_buf += product_str[i];
  }
  if (!str_buf.empty()) {
    product_parsed.push_back(str_buf);
    str_buf = "";
  }

  if (product_parsed.size() > filter_parsed.size())
    ErrorHandler::halt(EReplaceChunksLength{product});

  // matching algorithm: traverse filter vector
  for (const IString &istring : std::get<ILIST_STR>(input_parsed.contents)) {
    size_t i = 0;
    bool elem_match = true;
    std::vector<std::string> reconstruction_vector;
    for (size_t i_comp = 0; i_comp < filter_parsed.size(); i_comp++) {
      StringComponent const &str_component = filter_parsed[i_comp];
      if (std::holds_alternative<std::string>(str_component)) {
        // match exact characters
        std::string match_criteria = std::get<std::string>(str_component);
        if (match_criteria.size() > istring.content.size()) {
          std::cerr << "false 1" << std::endl;
          elem_match = false;
          break;
        } else if (match_criteria !=
                   istring.content.substr(i, match_criteria.size())) {
          std::cerr << "false 2" << std::endl;
          elem_match = false;
          break;
        }
        i += match_criteria.size();
      } else {
        // wildcard
        if (i_comp >= filter_parsed.size() - 1) // final asterisk
          break;
        bool seg_match = false;
        std::string match_criteria =
            std::get<std::string>(filter_parsed[i_comp + 1]);
        for (size_t i_seg = 0;
             i_seg < istring.content.size() - i - match_criteria.size() + 1;
             i_seg++) {
          if (istring.content.substr(i + i_seg, match_criteria.size()) ==
              match_criteria) {
            seg_match = true;
            reconstruction_vector.push_back(istring.content.substr(i, i_seg));
            i += i_seg + match_criteria.size();
            break;
          }
        }
        if (!seg_match) {
          std::cerr << "false 3" << std::endl;
          elem_match = false;
          break;
        } else {
          // increment this twice because two components have been matched.
          i_comp++;
        }
      }
    }

    if (!elem_match)
      std::get<ILIST_STR>(output_parsed.contents).push_back(istring);
    else {
      std::string reconstructed;
      size_t i_reconstruction_vec = 0;
      for (StringComponent const &str_component : product_parsed) {
        if (std::holds_alternative<std::string>(str_component))
          reconstructed += std::get<std::string>(str_component);
        else {
          reconstructed += reconstruction_vector[i_reconstruction_vec];
          i_reconstruction_vec++;
        }
      }
      std::get<ILIST_STR>(output_parsed.contents)
          .push_back({reconstructed, istring.reference});
    }
  }

  return {output_parsed, immutable};
}

Interpreter::Interpreter(AST &ast, Setup &setup)
    : m_ast(ast), m_setup(setup) {};

std::optional<Task> Interpreter::find_task(IString identifier) {
  for (Task task : m_ast.tasks) {
    IValue task_i = evaluate_ast_object(task.identifier, m_ast,
                                        {std::nullopt, std::nullopt}, state);
    if (std::holds_alternative<IString>(task_i.value) &&
        std::get<IString>(task_i.value) == identifier) {
      return task;
    } else if (std::holds_alternative<IList>(task_i.value) &&
               std::get<IList>(task_i.value).holds_istring()) {
      for (IString task_j :
           std::get<ILIST_STR>(std::get<IList>(task_i.value).contents)) {
        if (task_j == identifier)
          return task;
      }
    }
  }
  return std::nullopt;
}

std::optional<Field> Interpreter::find_field(std::string identifier,
                                             std::optional<Task> task) {
  // task-specific fields.
  if (task)
    for (Field const &field : task->fields)
      if (field.identifier.content == identifier)
        return field;

  // global fields.
  for (Field const &field : m_ast.fields) {
    if (field.identifier.content == identifier) {
      return field;
    }
  }
  return std::nullopt;
}

// if there is no default and field does not exist, return std::nullopt.
// it is up to the caller to handle a missing mandatory field.
std::optional<IValue>
Interpreter::evaluate_field_default(std::string identifier,
                                    EvaluationContext context,
                                    std::shared_ptr<EvaluationState> state,
                                    std::optional<IValue> default_value) {
  std::optional<Field> field = find_field(identifier, context.task_scope);
  if (!field) {
    return default_value;
  }
  return evaluate_ast_object(field->expression, m_ast, context, state);
}

std::optional<IValue>
Interpreter::evaluate_field_optional(std::string identifier,
                                     EvaluationContext context,
                                     std::shared_ptr<EvaluationState> state) {
  std::optional<Field> field = find_field(identifier, context.task_scope);
  if (!field)
    return std::nullopt;
  return evaluate_ast_object(field->expression, m_ast, context, state);
}

void Interpreter::t_run_task(Task task, std::string task_iteration,
                             std::shared_ptr<std::atomic<bool>> error) {
  try {
    FrameGuard frame{DependencyBuildFrame(task_iteration, task.reference)};
    if (0 > run_task(task, task_iteration))
      *error = true;
  } catch (...) {
    // it's ok to ignore the exception, since the task failure will throw an
    // EDependencyFailed regardless, which will unwind the combined error
    // stack.
    *error = true;
  }
}

DependencyStatus
Interpreter::_solve_dependencies_parallel(IValue dependencies) {
  if (std::holds_alternative<IString>(dependencies.value)) {
    // only one dependency - no reason to use a separate thread.
    IString task_iteration = std::get<IString>(dependencies.value);
    std::optional<Task> _task = find_task(task_iteration);
    std::optional<size_t> modified =
        OSLayer::get_file_timestamp(task_iteration.toString());
    if (!_task) {
      return {true, modified};
    }
    FrameGuard frame{
        DependencyBuildFrame(task_iteration.toString(), _task->reference)};
    int run_status = run_task(*_task, task_iteration.toString());
    if (0 > run_status)
      ErrorHandler::halt(
          EDependencyStatus{*_task, task_iteration, task_iteration.content});
    return {0 == run_status, modified};
  }
  if (!std::holds_alternative<IList>(dependencies.value) ||
      !std::get<IList>(dependencies.value).holds_istring()) {
    ErrorHandler::halt(
        EVariableTypeMismatch{dependencies, "string or list<string>"});
  }

  std::vector<std::thread> pool;
  IList dependencies_list = std::get<IList>(dependencies.value);
  std::shared_ptr<std::atomic<bool>> error =
      std::make_shared<std::atomic<bool>>();
  *error = false;
  std::optional<size_t> modified;

  for (IString task_iteration :
       std::get<ILIST_STR>(std::get<IList>(dependencies.value).contents)) {
    std::optional<Task> _task = find_task(task_iteration);
    std::optional<size_t> modified_i =
        OSLayer::get_file_timestamp(task_iteration.toString());
    if (!modified || (modified_i && modified < modified_i))
      modified = modified_i;
    if (!_task) {
      continue;
    }
    pool.push_back(std::thread(&Interpreter::t_run_task, this, *_task,
                               task_iteration.toString(), error));
  }

  for (std::thread &thread : pool) {
    thread.join();
  }

  if (*error) {
    // todo: halt in appropriate thread, perhaps?
    // this is not good, because the calling context will have no
    // reference as to what went wrong.
    return {false, modified};
  } else
    return {true, modified};
}

DependencyStatus Interpreter::_solve_dependencies_sync(IValue dependencies) {
  if (std::holds_alternative<IString>(dependencies.value)) {
    IString task_iteration = std::get<IString>(dependencies.value);
    std::optional<Task> _task = find_task(task_iteration);
    std::optional<size_t> modified =
        OSLayer::get_file_timestamp(task_iteration.toString());
    if (_task) {
      FrameGuard frame{
          DependencyBuildFrame(task_iteration.toString(), _task->reference)};
      int run_status = run_task(*_task, task_iteration.toString());
      if (0 > run_status)
        ErrorHandler::halt(
            EDependencyStatus{*_task, task_iteration, task_iteration.content});
      return {0 == run_status, modified};
    }
    return {true, modified};
  } else if (std::holds_alternative<IList>(dependencies.value) &&
             std::get<IList>(dependencies.value).holds_istring()) {
    std::optional<size_t> modified;
    for (IString task_iteration :
         std::get<ILIST_STR>(std::get<IList>(dependencies.value).contents)) {
      std::optional<Task> _task = find_task(task_iteration);
      std::optional<size_t> modified_i =
          OSLayer::get_file_timestamp(task_iteration.toString());
      if (!modified || (modified_i && modified < modified_i))
        modified = modified_i;
      if (!_task) {
        continue;
      }
      FrameGuard frame{
          DependencyBuildFrame(task_iteration.toString(), _task->reference)};
      int run_status = run_task(*_task, task_iteration.toString());
      if (0 > run_status) {
        ErrorHandler::halt(
            EDependencyStatus{*_task, task_iteration, task_iteration.content});
      }
      if (0 > run_status) { // what?
        return {false, modified};
      }
    }
    return {true, modified};
  } else {
    ErrorHandler::halt(
        EVariableTypeMismatch{dependencies, "string or list<string>"});
  }
}

DependencyStatus Interpreter::solve_dependencies(IValue dependencies,
                                                 bool parallel) {
  if (parallel)
    return _solve_dependencies_parallel(dependencies);
  else
    return _solve_dependencies_sync(dependencies);
}

int Interpreter::run_task(Task task, std::string task_iteration) {
  // solve dependencies.
  std::optional<IValue> dependencies =
      evaluate_field_optional(DEPENDS, {task, task_iteration}, this->state);
  std::optional<size_t> dep_modified;
  if (dependencies) {
    IValue parallel_default = {IBool(false, task.reference), true};
    // it is safe to unwrap the std::optional because we have a default value.
    IValue parallel =
        *evaluate_field_default(DEPENDS_PARALLEL, {task, task_iteration},
                                this->state, parallel_default);
    if (!std::holds_alternative<IBool>(parallel.value)) {
      ErrorHandler::halt(EVariableTypeMismatch{parallel, "bool"});
    }
    DependencyStatus dep_stat =
        solve_dependencies(*dependencies, std::get<IBool>(parallel.value));
    dep_modified = dep_stat.modified;
    if (!dep_stat.success)
      // todo: ErrorHandler
      return -1;
  }

  // check for changes.
  std::optional<size_t> this_modified =
      OSLayer::get_file_timestamp(task_iteration);
  if (this_modified && dep_modified && *this_modified >= *dep_modified) {
    LOG_STANDARD("•" << RESET << " skipped " << task_iteration);
    return 0;
  }

  // execution related fields.
  std::optional<IValue> command_expr =
      evaluate_field_optional(RUN, {task, task_iteration}, this->state);
  if (!command_expr) {
    return 0; // abstract task.
  }
  IValue run_parallel_default = {IBool(false, task.reference), true};
  IValue run_parallel = *evaluate_field_default(
      RUN_PARALLEL, {task, task_iteration}, this->state, run_parallel_default);
  if (!std::holds_alternative<IBool>(run_parallel.value)) {
    ErrorHandler::halt(EVariableTypeMismatch{run_parallel, "bool"});
  }
  if (!std::holds_alternative<IString>(command_expr->value) &&
      !(std::holds_alternative<IList>(command_expr->value) &&
        std::get<IList>(command_expr->value).holds_istring())) {
    ErrorHandler::halt(
        EVariableTypeMismatch{*command_expr, "string or list<string>"});
  }

  // execute task.
  LOG_STANDARD(CYAN << "»" << RESET << " starting " << task_iteration);
  if (std::holds_alternative<IString>(command_expr->value)) {
    // single command
    IString cmdline = std::get<IString>(command_expr->value);
    OSLayer os_layer(std::get<IBool>(run_parallel.value), false);
    os_layer.queue_command({cmdline.toString(), cmdline.reference});
    os_layer.execute_queue();

    if (!os_layer.get_non_zero_commands().empty()) {
      for (Command const &cmd : os_layer.get_non_zero_commands()) {
        ErrorHandler::soft_report(ENonZeroProcess{cmd.cmdline, cmd.reference});
      }
      ErrorHandler::trigger_report();
    }
  } else if (std::holds_alternative<IList>(command_expr->value) &&
             std::get<IList>(command_expr->value).holds_istring()) {
    // multiple commands
    OSLayer os_layer(std::get<IBool>(run_parallel.value), false);
    for (IString cmdline :
         std::get<ILIST_STR>(std::get<IList>(command_expr->value).contents)) {
      os_layer.queue_command({cmdline.toString(), cmdline.reference});
    }
    os_layer.execute_queue();

    if (!os_layer.get_non_zero_commands().empty()) {
      for (Command const &cmd : os_layer.get_non_zero_commands()) {
        ErrorHandler::soft_report(ENonZeroProcess{cmd.cmdline, cmd.reference});
      }
      ErrorHandler::trigger_report();
    }
  }

  LOG_STANDARD(GREEN << "✓" << RESET << " finished " << task_iteration);
  return 0;
}

int Interpreter::build() {

  this->state = std::make_shared<EvaluationState>();

  // find the task.
  if (m_ast.tasks.empty())
    ErrorHandler::halt(ENoTasks{});
  std::optional<Task> task;
  std::string task_iteration;
  if (m_setup.task) {
    task =
        find_task(IString(*m_setup.task, {})); // todo: fix undefined reference
    task_iteration = *m_setup.task;
    if (!task) {
      ErrorHandler::halt(ETaskNotFound{task_iteration});
    }
  } else {
    task = m_ast.tasks[0]; // we've already checked that it's not empty
    IValue task_iteration_ivalue = evaluate_ast_object(
        task->identifier, m_ast, {std::nullopt, std::nullopt}, state);
    if (!std::holds_alternative<IString>(task_iteration_ivalue.value)) {
      ErrorHandler::halt(EAmbiguousTask{*task});
    }
    task_iteration = std::get<IString>(evaluate_ast_object(
                                           m_ast.tasks[0].identifier, m_ast,
                                           {std::nullopt, std::nullopt}, state)
                                           .value)
                         .toString();
  }

  LOG_STANDARD("⧗ building " << CYAN << task_iteration << RESET);
  // todo: error checking is also required here in case task doesn't exist.
  FrameGuard frame{EntryBuildFrame(task_iteration, task->reference)};
  if (0 == run_task(*task, task_iteration)) {
    return 0;
  } else {
    // if the build failed without halting the interpreter, there isn't really
    // anything that can be done, so just inject a compiler crash.
    throw std::exception();
  }
}
