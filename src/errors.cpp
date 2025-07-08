#include "errors.hpp"
#include "format.hpp"
#include "interpreter.hpp"
#include "tracking.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <iostream>
#include <memory>
#include <thread>
#include <variant>

std::unordered_map<size_t, std::vector<std::shared_ptr<Frame>>>
    ContextStack::stack = {};
std::mutex ContextStack::stack_lock;
bool ContextStack::frozen = false;

void ContextStack::freeze() { ContextStack::frozen = true; }
bool ContextStack::is_frozen() { return ContextStack::frozen; }
std::unordered_map<size_t, std::vector<std::shared_ptr<Frame>>>
ContextStack::dump_stack() {
  return stack;
}
std::vector<std::shared_ptr<Frame>> ContextStack::export_local_stack() {
  std::thread::id thread_id = std::this_thread::get_id();
  size_t thread_hash = std::hash<std::thread::id>{}(thread_id);
  std::unique_lock<std::mutex> guard(ContextStack::stack_lock);
  return ContextStack::stack[thread_hash];
}
void ContextStack::import_local_stack(
    std::vector<std::shared_ptr<Frame>> local_stack) {
  std::thread::id thread_id = std::this_thread::get_id();
  size_t thread_hash = std::hash<std::thread::id>{}(thread_id);
  std::unique_lock<std::mutex> guard(ContextStack::stack_lock);
  ContextStack::stack[thread_hash] = local_stack;
}

// frameguard implementation.
template <typename F> FrameGuard::FrameGuard(F frame) {
  if (ContextStack::is_frozen())
    return;
  std::shared_ptr<F> frame_ptr = std::make_shared<F>(frame);
  std::thread::id thread_id = std::this_thread::get_id();
  thread_hash = std::hash<std::thread::id>{}(thread_id);
  std::unique_lock<std::mutex> guard(ContextStack::stack_lock);
  ContextStack::stack[thread_hash].push_back(std::move(frame_ptr));
}

FrameGuard::~FrameGuard() {
  if (ContextStack::is_frozen())
    return;
  std::unique_lock<std::mutex> guard(ContextStack::stack_lock);
  guard.unlock();
  assert(ContextStack::stack.size() != 0 &&
         "attempt to erase a non-existent context frame pointer");
  guard.lock();
  ContextStack::stack[thread_hash].pop_back();
}

// specific build frames.
std::string EntryBuildFrame::render_frame(std::vector<unsigned char> config) {
  ReferenceView task_view =
      ErrorRenderer::get_reference_view(config, reference);
  return std::format("building task '{}' {}(defined on line {}){}", task,
                     ITALIC, task_view.line_num, RESET);
};
EntryBuildFrame::EntryBuildFrame(std::string task, StreamReference reference) {
  this->task = task;
  this->reference = reference;
}

std::string
DependencyBuildFrame::render_frame(std::vector<unsigned char> config) {
  ReferenceView task_view =
      ErrorRenderer::get_reference_view(config, reference);
  return std::format(
      "building task '{}' as a dependency {}(defined on line {}){}", task,
      ITALIC, task_view.line_num, RESET);
}
DependencyBuildFrame::DependencyBuildFrame(std::string task,
                                           StreamReference reference) {
  this->task = task;
  this->reference = reference;
}

std::string
IdentifierEvaluateFrame::render_frame(std::vector<unsigned char> config) {
  ReferenceView identifier_view =
      ErrorRenderer::get_reference_view(config, reference);
  return std::format("evaluating variable '{}' {}(referred to on line {}){}",
                     identifier, ITALIC, identifier_view.line_num, RESET);
}
IdentifierEvaluateFrame::IdentifierEvaluateFrame(std::string identifier,
                                                 StreamReference reference) {
  this->identifier = identifier;
  this->reference = reference;
}

// internal standardized methods.
ReferenceView
ErrorRenderer::get_reference_view(std::vector<unsigned char> config,
                                  StreamReference reference) {
  // get line from config file & find line number
  size_t line_start = 0;
  size_t line_end = 0;
  size_t line_num = 1;
  for (size_t i = 0; i < reference.index && i < config.size(); i++) {
    if (config[i] == '\n') {
      line_start = i + 1;
      line_num++;
    }
  }
  line_end = line_start;
  for (size_t i = line_start; config[i] != '\n' && i < config.size(); i++)
    line_end = i;
  std::vector<unsigned char> line_prefix_vec(config.begin() + line_start,
                                             config.begin() + reference.index);
  std::string line_prefix(line_prefix_vec.begin(), line_prefix_vec.end());
  std::vector<unsigned char> line_ref_vec(config.begin() + reference.index,
                                          config.begin() + reference.index +
                                              reference.length);
  std::string line_ref(line_ref_vec.begin(), line_ref_vec.end());
  std::vector<unsigned char> line_suffix_vec(config.begin() + reference.index +
                                                 reference.length,
                                             config.begin() + line_end + 1);
  std::string line_suffix(line_suffix_vec.begin(), line_suffix_vec.end());
  return {line_prefix, line_ref, line_suffix, line_num};
}

// std::string ErrorRenderer::get_rendered_view(ReferenceView reference_view) {
//   std::string line_str = reference_view.line_str;
//   size_t line_num = reference_view.line_num;
//   size_t line_num_length = std::log(line_num) / std::log(10);
//   std::string underline_prefix(' ',
//                                line_num_length + 2 +
//                                reference_view.start_ref);
//   std::string underline("^", reference_view.end_ref -
//   reference_view.start_ref); return std::format("{} | {}\n{}{}", line_num,
//   line_str, underline_prefix,
//                      underline);
// }

std::string ErrorRenderer::get_rendered_view(ReferenceView reference_view,
                                             std::string msg) {
  std::string line_prefix = reference_view.line_prefix;
  std::string line_ref = reference_view.line_ref;
  std::string line_suffix = reference_view.line_suffix;
  size_t line_num = reference_view.line_num;
  size_t line_num_length = std::ceil((float)std::log(line_num) / std::log(10));
  size_t left_ref_position = line_num_length + 1;
  size_t right_ref_position = line_prefix.size() + 1;
  std::string left_pad(left_ref_position, ' ');
  std::string underline(right_ref_position, ' ');
  return std::format("{} | {}{}{}{}{}{}\n{}|{}{}⤷ {}{}", line_num, line_prefix,
                     UNDERLINE, line_ref, RESET, line_suffix, RESET, left_pad,
                     BOLD, underline, msg, RESET);
}

std::string ErrorRenderer::prefix_rendered_view(std::string view,
                                                std::string prefix) {
  std::string out;
  for (size_t i = 0; i < view.size(); i++) {
    out += view[i];
    if (view[i] == '\n')
      out += prefix;
  }
  return out;
}

std::string ErrorRenderer::stringify_type(
    std::variant<ASTObject, IValue, IString, IBool, IList> object) {
  if (std::holds_alternative<ASTObject>(object)) {
    ASTObject ast_object = std::get<ASTObject>(object);
    if (std::holds_alternative<Identifier>(ast_object)) {
      return "variable<?>";
    } else if (std::holds_alternative<Literal>(ast_object)) {
      return "string";
    } else if (std::holds_alternative<FormattedLiteral>(ast_object)) {
      return "string";
    } else if (std::holds_alternative<List>(ast_object)) {
      return "list<?>";
    } else if (std::holds_alternative<Boolean>(ast_object)) {
      return "bool";
    } else if (std::holds_alternative<Replace>(ast_object)) {
      return "string";
    } else {
      assert(false && "attempt to stringify nonexistent type");
    }
  } else if (std::holds_alternative<IValue>(object)) {
    IValue ivalue = std::get<IValue>(object);
    if (std::holds_alternative<IString>(ivalue.value)) {
      return "string";
    } else if (std::holds_alternative<IBool>(ivalue.value)) {
      return "bool";
    } else if (std::holds_alternative<IList>(ivalue.value)) {
      IList list = std::get<IList>(ivalue.value);
      if (list.holds_istring())
        return "list<string>";
      else if (list.holds_ibool())
        return "list<bool>";
      else {
        assert(false && "attempt to stringify nonexistent type");
      }
    } else {
      assert(false && "attempt to stringify nonexistent type");
    }
  } else if (std::holds_alternative<IString>(object)) {
    return "string";
  } else if (std::holds_alternative<IBool>(object)) {
    return "bool";
  } else if (std::holds_alternative<IList>(object)) {
    IList list = std::get<IList>(object);
    if (list.holds_istring())
      return "list<string>";
    else if (list.holds_ibool())
      return "list<bool>";
    else {
      assert(false && "attempt to stringify nonexistent type");
    }
  } else {
    assert(false && "attempt to stringify nonexistent type");
  }
}

// specific errors.
ENoMatchingIdentifier::ENoMatchingIdentifier(Identifier identifier) {
  this->identifier = identifier;
}

std::string
ENoMatchingIdentifier::render_error(std::vector<unsigned char> config) {
  ReferenceView identifier_view =
      ErrorRenderer::get_reference_view(config, identifier.reference);
  std::string rendered_view = ErrorRenderer::get_rendered_view(
      identifier_view, "variable referred to here");
  return std::format(
      "{}{}error:{}{} variable '{}' referred to on line {} does not "
      "exist.\n{}{}",
      RED, BOLD, RESET, BOLD, identifier.content, identifier_view.line_num,
      RESET, rendered_view);
};

char const *ENoMatchingIdentifier::get_exception_msg() {
  return "No matching identifier";
}

EListTypeMismatch::EListTypeMismatch(IList list, IValue ivalue)
    : list(list), faulty_ivalue(ivalue) {}

std::string EListTypeMismatch::render_error(std::vector<unsigned char> config) {
  ReferenceView obj_view = ErrorRenderer::get_reference_view(
      config, std::visit(IVisitReference{}, faulty_ivalue.value));
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(obj_view, "faulty type here");
  return std::format("{}{}error:{}{} an item of type '{}' cannot be stored in "
                     "a list of type '{}'.\n{}{}",
                     RED, BOLD, RESET, BOLD,
                     ErrorRenderer::stringify_type(faulty_ivalue),
                     ErrorRenderer::stringify_type(list), RESET, rendered_view);
}

char const *EListTypeMismatch::get_exception_msg() {
  return "List type mismatch";
}

EReplaceTypeMismatch::EReplaceTypeMismatch(Replace replace,
                                           IValue faulty_ivalue)
    : faulty_ivalue(faulty_ivalue) {
  this->replace = replace;
}

std::string
EReplaceTypeMismatch::render_error(std::vector<unsigned char> config) {
  ReferenceView obj_view = ErrorRenderer::get_reference_view(
      config, std::visit(IVisitReference{}, faulty_ivalue.value));
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(obj_view, "faulty type here");
  return std::format("{}{}error:{}{} the replacement operator can only operate "
                     "with strings.\n{}{}",
                     RED, BOLD, RESET, BOLD, RESET, rendered_view);
}

char const *EReplaceTypeMismatch::get_exception_msg() {
  return "Replace type mismatch";
}

EReplaceChunksLength::EReplaceChunksLength(IValue replacement)
    : replacement(replacement) {}

std::string
EReplaceChunksLength::render_error(std::vector<unsigned char> config) {
  StreamReference ref = std::visit(IVisitReference{}, replacement.value);
  ReferenceView repl_view = ErrorRenderer::get_reference_view(config, ref);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(repl_view, "too many wildcards here");
  return std::format("{}{}error:{}{} invalid combination of wildcards in "
                     "replacement operator on line {}.\n{}{}",
                     RED, BOLD, RESET, BOLD, repl_view.line_num, RESET,
                     rendered_view);
}

char const *EReplaceChunksLength::get_exception_msg() {
  return "Invalid replace chunks length";
}

EVariableTypeMismatch::EVariableTypeMismatch(IValue variable,
                                             std::string expected_type)
    : variable(variable) {
  this->expected_type = expected_type;
}

std::string
EVariableTypeMismatch::render_error(std::vector<unsigned char> config) {
  StreamReference var_ref = std::visit(IVisitReference{}, variable.value);
  ReferenceView var_view = ErrorRenderer::get_reference_view(config, var_ref);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(var_view, "variable defined here");
  return std::format("{}{}error:{}{} expected variable defined on line {} to "
                     "be of type '{}', but was '{}'\n{}{}",
                     RED, BOLD, RESET, BOLD, var_view.line_num, expected_type,
                     ErrorRenderer::stringify_type(variable), RESET,
                     rendered_view);
}

char const *EVariableTypeMismatch::get_exception_msg() {
  return "Variable type mismatch";
}

ENonZeroProcess::ENonZeroProcess(std::string cmdline,
                                 StreamReference reference) {
  this->cmdline = cmdline;
  this->reference = reference;
}

std::string ENonZeroProcess::render_error(std::vector<unsigned char> config) {
  ReferenceView ref_view = ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(ref_view, "command defined here");
  return std::format("{}{}error:{}{} the command '{}' failed.\n{}{}", RED, BOLD,
                     RESET, BOLD, cmdline, RESET, rendered_view);
}

char const *ENonZeroProcess::get_exception_msg() { return "Command failed"; }

ETaskNotFound::ETaskNotFound(std::string task_name) {
  this->task_name = task_name;
}

std::string ETaskNotFound::render_error(std::vector<unsigned char>) {
  return std::format("{}{}error:{}{} task '{}' does not exist.{}", RED, BOLD,
                     RESET, BOLD, task_name, RESET);
}

char const *ETaskNotFound::get_exception_msg() { return "Task not found"; }

std::string ENoTasks::render_error(std::vector<unsigned char>) {
  return std::format("{}{}error:{}{} no tasks are defined.{}", RED, BOLD, RESET,
                     BOLD, RESET);
}

char const *ENoTasks::get_exception_msg() { return "No tasks are defined"; }

EAmbiguousTask::EAmbiguousTask(Task task) { this->task = task; }

std::string EAmbiguousTask::render_error(std::vector<unsigned char> config) {
  ReferenceView task_view =
      ErrorRenderer::get_reference_view(config, task.reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(task_view, "task defined here");
  return std::format(
      "{}{}error:{}{} topmost task defined on line {} is ambiguous. specify a "
      "specific task to build or move the definition.\n{}{}",
      RED, BOLD, RESET, BOLD, task_view.line_num, RESET, rendered_view);
}

char const *EAmbiguousTask::get_exception_msg() {
  return "Ambiguous topmost task";
}

EDependencyStatus::EDependencyStatus(Task task, IString dependency_where,
                                     std::string dependency_value)
    : dependency_where(dependency_where) {
  this->task = task;
  this->dependency_value = dependency_value;
}

std::string EDependencyStatus::render_error(std::vector<unsigned char> config) {
  ReferenceView task_view =
      ErrorRenderer::get_reference_view(config, task.reference);
  StreamReference dep_where_ref = dependency_where.reference;
  ReferenceView ref_view =
      ErrorRenderer::get_reference_view(config, dep_where_ref);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(task_view, "task defined here");
  return std::format("{}{}error:{}{} dependency '{}' defined on line {} and "
                     "referred to on line {} "
                     "not met; building the task failed.\n{}{}",
                     RED, BOLD, RESET, BOLD, dependency_value,
                     task_view.line_num, ref_view.line_num, RESET,
                     rendered_view);
}

char const *EDependencyStatus::get_exception_msg() {
  return "Dependency status";
}

EDependencyFailed::EDependencyFailed(IValue dep, std::string dependency_value)
    : dependency(dep) {
  this->dependency_value = dependency_value;
}

std::string EDependencyFailed::render_error(std::vector<unsigned char> config) {
  StreamReference ref = std::visit(IVisitReference{}, dependency.value);
  ReferenceView dep_view = ErrorRenderer::get_reference_view(config, ref);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(dep_view, "dependency referred to here");
  return std::format("{}{}error:{}{} dependency '{}' referred to on line {} "
                     "not met; file does not "
                     "exist and no task was found.\n{}{}",
                     RED, BOLD, RESET, BOLD, dependency_value,
                     dep_view.line_num, RESET, rendered_view);
}

char const *EDependencyFailed::get_exception_msg() {
  return "Dependency not met";
}

EInvalidSymbol::EInvalidSymbol(StreamReference reference, std::string symbol) {
  this->reference = reference;
  this->symbol = symbol;
}

std::string EInvalidSymbol::render_error(std::vector<unsigned char> config) {
  ReferenceView ref_view = ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(ref_view, "symbol encountered here");
  return std::format(
      "{}{}error:{}{} invalid symbol '{}' encountered on line {}.\n{}{}", RED,
      BOLD, RESET, BOLD, symbol, ref_view.line_num, RESET, rendered_view);
}

char const *EInvalidSymbol::get_exception_msg() { return "Invalid symbol"; }

EInvalidLiteral::EInvalidLiteral(StreamReference reference) {
  this->reference = reference;
}

std::string EInvalidLiteral::render_error(std::vector<unsigned char> config) {
  ReferenceView ref_view = ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(ref_view, "invalid symbol here");
  return std::format(
      "{}{}error:{}{} invalid literal encountered on line {}.\n{}{}", RED, BOLD,
      RESET, BOLD, ref_view.line_num, rendered_view, RESET);
}

char const *EInvalidLiteral::get_exception_msg() { return "Invalid literal"; }

EInvalidGrammar::EInvalidGrammar(StreamReference reference) {
  this->reference = reference;
}

std::string EInvalidGrammar::render_error(std::vector<unsigned char> config) {
  ReferenceView ref_view = ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(ref_view, "syntax encountered here");
  return std::format(
      "{}{}error:{}{} invalid language syntax encountered on line {}.\n{}{}",
      RED, BOLD, RESET, BOLD, ref_view.line_num, RESET, rendered_view);
}

char const *EInvalidGrammar::get_exception_msg() { return "Invalid grammar"; }

ENoValue::ENoValue(Identifier identifier) { this->identifier = identifier; }

std::string ENoValue::render_error(std::vector<unsigned char> config) {
  ReferenceView decl_view =
      ErrorRenderer::get_reference_view(config, identifier.reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(decl_view, "variable declared here");
  return std::format("{}{}error:{}{} invalid value for variable '{}' declared "
                     "on line {}.\n{}{}",
                     RED, BOLD, RESET, BOLD, identifier.content,
                     decl_view.line_num, RESET, rendered_view);
}

char const *ENoValue::get_exception_msg() { return "No valid value"; }

ENoLinestop::ENoLinestop(StreamReference reference) {
  this->reference = reference;
}

std::string ENoLinestop::render_error(std::vector<unsigned char> config) {
  ReferenceView line_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view = ErrorRenderer::get_rendered_view(
      line_view, "semicolon expected after this expression");
  return std::format("{}{}error:{}{} missing semicolon or invalid expression "
                     "on line {}.\n{}{}",
                     RED, BOLD, RESET, BOLD, line_view.line_num, RESET,
                     rendered_view);
}

char const *ENoLinestop::get_exception_msg() { return "No linestop"; }

ENoIterator::ENoIterator(StreamReference reference) {
  this->reference = reference;
}

std::string ENoIterator::render_error(std::vector<unsigned char> config) {
  ReferenceView task_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view = ErrorRenderer::get_rendered_view(
      task_view, "explicit iterator required because of this");
  return std::format("{}{}error:{}{} task defined on line {} doesn't have a "
                     "valid explicit iterator.\n{}{}",
                     RED, BOLD, RESET, BOLD, task_view.line_num, RESET,
                     rendered_view);
}

char const *ENoIterator::get_exception_msg() { return "No task iterator"; }

ENoTaskOpen::ENoTaskOpen(StreamReference reference) {
  this->reference = reference;
}

std::string ENoTaskOpen::render_error(std::vector<unsigned char> config) {
  ReferenceView task_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(task_view, "task defined here");
  return std::format("{}{}error:{}{} task defined on line {} doesn't have an "
                     "opening curly bracket.\n{}{}",
                     RED, BOLD, RESET, BOLD, task_view.line_num, RESET,
                     rendered_view);
}

char const *ENoTaskOpen::get_exception_msg() {
  return "No task open curly bracket";
}

ENoTaskClose::ENoTaskClose(StreamReference reference) {
  this->reference = reference;
}

std::string ENoTaskClose::render_error(std::vector<unsigned char> config) {
  ReferenceView task_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(task_view, "task defined here");
  return std::format("{}{}error:{}{} task defined on line {} doesn't have a "
                     "closing curly bracket.\n{}{}",
                     RED, BOLD, RESET, BOLD, task_view.line_num, RESET,
                     rendered_view);
}

char const *ENoTaskClose::get_exception_msg() {
  return "No task close curly bracket";
}

EInvalidListEnd::EInvalidListEnd(StreamReference reference) {
  this->reference = reference;
}

std::string EInvalidListEnd::render_error(std::vector<unsigned char> config) {
  ReferenceView separator_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view = ErrorRenderer::get_rendered_view(
      separator_view, "item expected after this separator");
  return std::format("{}{}error:{}{} list defined on line {} contains an "
                     "invalid ending.\n{}{}",
                     RED, BOLD, RESET, BOLD, separator_view.line_num, RESET,
                     rendered_view);
}

char const *EInvalidListEnd::get_exception_msg() { return "Invalid list end"; }

ENoReplacementIdentifier::ENoReplacementIdentifier(StreamReference reference) {
  this->reference = reference;
}

std::string
ENoReplacementIdentifier::render_error(std::vector<unsigned char> config) {
  ReferenceView modify_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view = ErrorRenderer::get_rendered_view(
      modify_view, "expression expected before this colon");
  return std::format("{}{}error:{}{} replacement operator on line {} does not "
                     "contain a valid input expression.\n{}{}",
                     RED, BOLD, RESET, BOLD, modify_view.line_num, RESET,
                     rendered_view);
}

char const *ENoReplacementIdentifier::get_exception_msg() {
  return "No replacement identifier";
}

ENoReplacementOriginal::ENoReplacementOriginal(StreamReference reference) {
  this->reference = reference;
}

std::string
ENoReplacementOriginal::render_error(std::vector<unsigned char> config) {
  ReferenceView modify_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view = ErrorRenderer::get_rendered_view(
      modify_view, "expression expected after this colon");
  return std::format("{}{}error:{}{} replacement operator on line {} does not "
                     "contain a valid matching expression.\n{}{}",
                     RED, BOLD, RESET, BOLD, modify_view.line_num, RESET,
                     rendered_view);
}

char const *ENoReplacementOriginal::get_exception_msg() {
  return "No replacement original";
}

ENoReplacementArrow::ENoReplacementArrow(StreamReference reference) {
  this->reference = reference;
}

std::string
ENoReplacementArrow::render_error(std::vector<unsigned char> config) {
  ReferenceView original_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view = ErrorRenderer::get_rendered_view(
      original_view, "arrow expected after this expression");
  return std::format("{}{}error:{}{} expected an arrow in the replacement "
                     "operator on line {}.\n{}{}",
                     RED, BOLD, RESET, BOLD, original_view.line_num, RESET,
                     rendered_view);
}

char const *ENoReplacementArrow::get_exception_msg() {
  return "No replacement arrow";
}

ENoReplacementReplacement::ENoReplacementReplacement(
    StreamReference reference) {
  this->reference = reference;
}

std::string
ENoReplacementReplacement::render_error(std::vector<unsigned char> config) {
  ReferenceView arrow_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view = ErrorRenderer::get_rendered_view(
      arrow_view, "expression expected after this arrow");
  return std::format("{}{}error:{}{} replacement operator on line {} does not "
                     "contain a valid output expression.\n{}{}",
                     RED, BOLD, RESET, BOLD, arrow_view.line_num, RESET,
                     rendered_view);
}

char const *ENoReplacementReplacement::get_exception_msg() {
  return "No replacement replacement";
}

EInvalidEscapedExpression::EInvalidEscapedExpression(
    StreamReference reference) {
  this->reference = reference;
}

std::string
EInvalidEscapedExpression::render_error(std::vector<unsigned char> config) {
  ReferenceView expr_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(expr_view, "invalid expression here");
  return std::format("{}{}error:{}{} string defined on line {} contains an "
                     "invalid expression.\n{}{}",
                     RED, BOLD, RESET, BOLD, expr_view.line_num, RESET,
                     rendered_view);
}

char const *EInvalidEscapedExpression::get_exception_msg() {
  return "Invalid escaped expression";
}

ENoExpressionClose::ENoExpressionClose(StreamReference reference) {
  this->reference = reference;
}

std::string
ENoExpressionClose::render_error(std::vector<unsigned char> config) {
  ReferenceView expr_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view = ErrorRenderer::get_rendered_view(
      expr_view, "expected closing bracket after this");
  return std::format("{}{}error:{}{} expected a closing bracket after "
                     "expression on line {}.\n{}{}",
                     RED, BOLD, RESET, BOLD, expr_view.line_num, RESET,
                     rendered_view);
}

char const *ENoExpressionClose::get_exception_msg() {
  return "No expression close";
}

EEmptyExpression::EEmptyExpression(StreamReference reference) {
  this->reference = reference;
}

std::string EEmptyExpression::render_error(std::vector<unsigned char> config) {
  ReferenceView expr_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view = ErrorRenderer::get_rendered_view(
      expr_view, "expected expression after this bracket");
  return std::format("{}{}error:{}{} expected an expression after an opening "
                     "bracket on line {}.\n{}{}",
                     RED, BOLD, RESET, BOLD, expr_view.line_num, RESET,
                     rendered_view);
}

char const *EEmptyExpression::get_exception_msg() { return "Empty expression"; }

EInvalidInputFile::EInvalidInputFile(std::string path) { this->path = path; }

std::string EInvalidInputFile::render_error(std::vector<unsigned char>) {
  return std::format("{}{}error:{}{} config file '{}' is unreachable.{}", RED,
                     BOLD, RESET, BOLD, path, RESET);
}

char const *EInvalidInputFile::get_exception_msg() {
  return "Invalid input file";
}

EInvalidEscapeCode::EInvalidEscapeCode(unsigned char code,
                                       StreamReference reference) {
  this->code = code;
  this->reference = reference;
}

std::string
EInvalidEscapeCode::render_error(std::vector<unsigned char> config) {
  ReferenceView code_view =
      ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(code_view, "escape code here");
  return std::format(
      "{}{}error:{}{} escape code '\\{}' on line {} is invalid.\n{}{}", RED,
      BOLD, RESET, BOLD, std::string(1, code), code_view.line_num, RESET,
      rendered_view);
}

char const *EInvalidEscapeCode::get_exception_msg() {
  return "Invalid escape code";
}

std::unordered_map<size_t, std::shared_ptr<BuildError>>
    ErrorHandler::error_state = {};
std::mutex ErrorHandler::error_lock;

std::unordered_map<size_t, std::shared_ptr<BuildError>>
ErrorHandler::get_errors() {
  return error_state;
};

template <typename B> void ErrorHandler::halt [[noreturn]] (B build_error) {
  ContextStack::freeze();
  std::thread::id thread_id = std::this_thread::get_id();
  size_t thread_hash = std::hash<std::thread::id>{}(thread_id);

  std::unique_lock<std::mutex> guard(ErrorHandler::error_lock);
  ErrorHandler::error_state[thread_hash] = std::make_unique<B>(build_error);
  throw BuildException(build_error.get_exception_msg());
}

template <typename B> void ErrorHandler::soft_report(B build_error) {
  ContextStack::freeze();
  std::thread::id thread_id = std::this_thread::get_id();
  size_t thread_hash = std::hash<std::thread::id>{}(thread_id);

  std::unique_lock<std::mutex> guard(ErrorHandler::error_lock);
  ErrorHandler::error_state[thread_hash] = std::make_unique<B>(build_error);
}

void ErrorHandler::trigger_report [[noreturn]] () {
  std::unique_lock<std::mutex> guard(ErrorHandler::error_lock);
  assert(ErrorHandler::error_state.size() > 0 &&
         "attempt to trigger a report on an empty error state");
  std::shared_ptr<BuildError> build_error =
      ErrorHandler::error_state.begin()->second;
  throw BuildException(build_error->get_exception_msg());
}

template void ErrorHandler::halt<ENoMatchingIdentifier>(ENoMatchingIdentifier);
template void ErrorHandler::halt<EInvalidSymbol>(EInvalidSymbol);
template void ErrorHandler::halt<EInvalidLiteral>(EInvalidLiteral);
template void ErrorHandler::halt<EInvalidGrammar>(EInvalidGrammar);
template void ErrorHandler::halt<ENoValue>(ENoValue);
template void ErrorHandler::halt<ENoLinestop>(ENoLinestop);
template void ErrorHandler::halt<ENoIterator>(ENoIterator);
template void ErrorHandler::halt<ENoTaskOpen>(ENoTaskOpen);
template void ErrorHandler::halt<ENoTaskClose>(ENoTaskClose);
template void ErrorHandler::halt<EInvalidListEnd>(EInvalidListEnd);
template void
    ErrorHandler::halt<ENoReplacementIdentifier>(ENoReplacementIdentifier);
template void
    ErrorHandler::halt<ENoReplacementOriginal>(ENoReplacementOriginal);
template void ErrorHandler::halt<ENoReplacementArrow>(ENoReplacementArrow);
template void
    ErrorHandler::halt<ENoReplacementReplacement>(ENoReplacementReplacement);
template void
    ErrorHandler::halt<EInvalidEscapedExpression>(EInvalidEscapedExpression);
template void ErrorHandler::halt<ENoExpressionClose>(ENoExpressionClose);
template void ErrorHandler::halt<EEmptyExpression>(EEmptyExpression);
template void ErrorHandler::halt<EListTypeMismatch>(EListTypeMismatch);
template void ErrorHandler::halt<EReplaceTypeMismatch>(EReplaceTypeMismatch);
template void ErrorHandler::halt<EReplaceChunksLength>(EReplaceChunksLength);
template void ErrorHandler::halt<EDependencyStatus>(EDependencyStatus);
template void ErrorHandler::halt<EVariableTypeMismatch>(EVariableTypeMismatch);
template void ErrorHandler::halt<ENoTasks>(ENoTasks);
template void ErrorHandler::halt<ETaskNotFound>(ETaskNotFound);
template void ErrorHandler::halt<EAmbiguousTask>(EAmbiguousTask);
template void ErrorHandler::halt<ENonZeroProcess>(ENonZeroProcess);
template void ErrorHandler::halt<EInvalidInputFile>(EInvalidInputFile);
template void ErrorHandler::halt<EInvalidEscapeCode>(EInvalidEscapeCode);
template void
    ErrorHandler::soft_report<ENoMatchingIdentifier>(ENoMatchingIdentifier);
template void ErrorHandler::soft_report<EInvalidSymbol>(EInvalidSymbol);
template void ErrorHandler::soft_report<EInvalidLiteral>(EInvalidLiteral);
template void ErrorHandler::soft_report<EInvalidGrammar>(EInvalidGrammar);
template void ErrorHandler::soft_report<ENoValue>(ENoValue);
template void ErrorHandler::soft_report<ENoLinestop>(ENoLinestop);
template void ErrorHandler::soft_report<ENoIterator>(ENoIterator);
template void ErrorHandler::soft_report<ENoTaskOpen>(ENoTaskOpen);
template void ErrorHandler::soft_report<ENoTaskClose>(ENoTaskClose);
template void ErrorHandler::soft_report<EInvalidListEnd>(EInvalidListEnd);
template void ErrorHandler::soft_report<ENoReplacementIdentifier>(
    ENoReplacementIdentifier);
template void
    ErrorHandler::soft_report<ENoReplacementOriginal>(ENoReplacementOriginal);
template void
    ErrorHandler::soft_report<ENoReplacementArrow>(ENoReplacementArrow);
template void ErrorHandler::soft_report<ENoReplacementReplacement>(
    ENoReplacementReplacement);
template void ErrorHandler::soft_report<EInvalidEscapedExpression>(
    EInvalidEscapedExpression);
template void ErrorHandler::soft_report<ENoExpressionClose>(ENoExpressionClose);
template void ErrorHandler::soft_report<EEmptyExpression>(EEmptyExpression);
template void ErrorHandler::soft_report<EListTypeMismatch>(EListTypeMismatch);
template void
    ErrorHandler::soft_report<EReplaceTypeMismatch>(EReplaceTypeMismatch);
template void
    ErrorHandler::soft_report<EReplaceChunksLength>(EReplaceChunksLength);
template void ErrorHandler::soft_report<EDependencyStatus>(EDependencyStatus);
template void
    ErrorHandler::soft_report<EVariableTypeMismatch>(EVariableTypeMismatch);
template void ErrorHandler::soft_report<ENoTasks>(ENoTasks);
template void ErrorHandler::soft_report<ETaskNotFound>(ETaskNotFound);
template void ErrorHandler::soft_report<EAmbiguousTask>(EAmbiguousTask);
template void ErrorHandler::soft_report<ENonZeroProcess>(ENonZeroProcess);
template void ErrorHandler::soft_report<EInvalidInputFile>(EInvalidInputFile);
template void ErrorHandler::soft_report<EInvalidEscapeCode>(EInvalidEscapeCode);
template FrameGuard::FrameGuard(IdentifierEvaluateFrame);
template FrameGuard::FrameGuard(EntryBuildFrame);
template FrameGuard::FrameGuard(DependencyBuildFrame);

//
// ErrorContext::ErrorContext(Origin const &origin) {
//   if (std::holds_alternative<InputStreamPos>(origin)) {
//     this->stream_pos = std::get<InputStreamPos>(origin);
//     this->ref = std::nullopt;
//   } else if (std::holds_alternative<ObjectReference>(origin)) {
//     this->stream_pos = std::nullopt;
//     this->ref = std::get<ObjectReference>(origin);
//   } else {
//     this->stream_pos = std::nullopt;
//     this->ref = std::nullopt;
//   }
// }
//
// ErrorContext::ErrorContext(InternalNode const &) {
//   this->stream_pos = std::nullopt;
//   this->ref = std::nullopt;
// }
//
// ErrorContext::ErrorContext(ObjectReference const &ref) {
//   this->stream_pos = std::nullopt;
//   this->ref = ref;
// }
//
// ErrorContext::ErrorContext(Origin const &origin, ObjectReference const
// &ref)
// {
//   if (std::holds_alternative<InputStreamPos>(origin)) {
//     this->stream_pos = std::get<InputStreamPos>(origin);
//     this->ref = std::nullopt;
//   } else if (std::holds_alternative<ObjectReference>(origin)) {
//     this->stream_pos = std::nullopt;
//     this->ref = std::get<ObjectReference>(origin);
//   } else {
//     this->stream_pos = std::nullopt;
//     this->ref = std::nullopt;
//   }
//   this->ref = ref;
// }
