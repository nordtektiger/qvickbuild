#include "errors.hpp"
#include "format.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <iostream>
#include <memory>
#include <thread>
#include <variant>

std::unordered_map<size_t, std::vector<std::unique_ptr<Frame>>>
    ContextStack::stack = {};
std::mutex ContextStack::stack_lock;
bool ContextStack::frozen = false;

void ContextStack::freeze() { ContextStack::frozen = true; }
bool ContextStack::is_frozen() { return ContextStack::frozen; }
std::unordered_map<size_t, std::vector<std::unique_ptr<Frame>>>
ContextStack::dump_stack() {
  return std::move(stack);
}

// frameguard implementation.
template <typename F> FrameGuard::FrameGuard(F frame) {
  if (ContextStack::is_frozen())
    return;
  std::unique_ptr<F> frame_ptr = std::make_unique<F>(frame);
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
  return std::format("{} | {}{}{}{}{}{}\n{}|{}{}{}⤷ {}{}", line_num,
                     line_prefix, UNDERLINE, line_ref, RESET, line_suffix,
                     RESET, left_pad, ITALIC, BOLD, underline, msg, RESET);
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
    std::variant<ASTObject, IString, IBool, IList> object) {
  if (std::holds_alternative<ASTObject>(object)) {
    ASTObject ast_object = std::get<ASTObject>(object);
    if (std::holds_alternative<Identifier>(ast_object)) {
      return "variable";
    } else if (std::holds_alternative<Literal>(ast_object)) {
      return "string";
    } else if (std::holds_alternative<FormattedLiteral>(ast_object)) {
      return "string";
    } else if (std::holds_alternative<List>(ast_object)) {
      return "list";
    } else if (std::holds_alternative<Boolean>(ast_object)) {
      return "bool";
    } else if (std::holds_alternative<Replace>(ast_object)) {
      return "string";
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
      return "string";
    else if (list.holds_ibool())
      return "bool";
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

EListTypeMismatch::EListTypeMismatch(List list, ASTObject ast_obj) {
  this->list = list;
  this->faulty_ast_object = ast_obj;
}

std::string EListTypeMismatch::render_error(std::vector<unsigned char> config) {
  ReferenceView obj_view = ErrorRenderer::get_reference_view(
      config, std::visit(ASTVisitReference{}, faulty_ast_object));
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(obj_view, "faulty type here");
  return std::format("{}{}error:{}{} an item of type '{}' cannot be stored in "
                     "a list of type '{}'.\n{}{}",
                     RED, BOLD, RESET, BOLD,
                     ErrorRenderer::stringify_type(faulty_ast_object),
                     ErrorRenderer::stringify_type(list), RESET, rendered_view);
}

char const *EListTypeMismatch::get_exception_msg() {
  return "List type mismatch";
}

EReplaceTypeMismatch::EReplaceTypeMismatch(Replace replace,
                                           ASTObject faulty_ast_object) {
  this->replace = replace;
  this->faulty_ast_object = faulty_ast_object;
}

std::string
EReplaceTypeMismatch::render_error(std::vector<unsigned char> config) {
  ReferenceView obj_view = ErrorRenderer::get_reference_view(
      config, std::visit(ASTVisitReference{}, faulty_ast_object));
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(obj_view, "faulty type here");
  return std::format("{}{}error:{}{} the replacement operator can only operate "
                     "on strings.\n{}{}",
                     RED, BOLD, RESET, BOLD, RESET, rendered_view);
}

char const *EReplaceTypeMismatch::get_exception_msg() {
  return "Replace type mismatch";
}

ENonZeroProcess::ENonZeroProcess(StreamReference reference,
                                 std::string command) {
  this->reference = reference;
  this->command = command;
}

std::string ENonZeroProcess::render_error(std::vector<unsigned char> config) {
  ReferenceView ref_view = ErrorRenderer::get_reference_view(config, reference);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(ref_view, "command defined here");
  return std::format("{}{}error:{}{} the command '{}' failed.\n{}{}", RED, BOLD,
                     RESET, BOLD, command, RESET, rendered_view);
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

EDependencyFailed::EDependencyFailed(IValue dep, std::string dependency_value)
    : dependency(dep) {
  this->dependency_value = dependency_value;
}

std::string EDependencyFailed::render_error(std::vector<unsigned char> config) {
  StreamReference ref = std::visit(IVisitReference{}, dependency.value);
  ReferenceView dep_view = ErrorRenderer::get_reference_view(config, ref);
  std::string rendered_view =
      ErrorRenderer::get_rendered_view(dep_view, "dependency referred to here");
  return std::format("{}{}error:{}{} dependency '{}' reffered to on line {} "
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
      ErrorRenderer::get_rendered_view(ref_view, "literal defined here");
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
      "{}{}error:{}{} invalid language syntax encountered on line {}.\n{}{}", RED, BOLD,
      RESET, BOLD, ref_view.line_num, rendered_view, RESET);
}

char const *EInvalidGrammar::get_exception_msg() { return "Invalid grammar"; }

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

template void ErrorHandler::halt<ENoMatchingIdentifier>(ENoMatchingIdentifier);
template void ErrorHandler::halt<EInvalidSymbol>(EInvalidSymbol);
template void ErrorHandler::halt<EInvalidLiteral>(EInvalidLiteral);
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
