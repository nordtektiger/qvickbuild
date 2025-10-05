#ifndef ERRORS_TYPES_HPP
#define ERRORS_TYPES_HPP

#include "../interpreter/types.hpp"
#include "../lexer/tracking.hpp"
#include "../parser/types.hpp"
#include <string>
#include <vector>

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
  std::variant<IList<IString>, IList<IBool>> list;
  std::unique_ptr<IValue> faulty_ivalue;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EListTypeMismatch() = delete;
  EListTypeMismatch(std::variant<IList<IString>, IList<IBool>>, IValue &);
};

class EReplaceTypeMismatch : public BuildError {
private:
  Replace replace;
  std::unique_ptr<IValue> faulty_ivalue;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EReplaceTypeMismatch() = delete;
  EReplaceTypeMismatch(Replace, IValue &);
};

class EReplaceChunksLength : public BuildError {
private:
  std::unique_ptr<IValue> replacement;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EReplaceChunksLength() = delete;
  EReplaceChunksLength(IValue &);
};

class EVariableTypeMismatch : public BuildError {
private:
  std::unique_ptr<IValue> variable;
  std::string expected_type;

public:
  std::string render_error(std::vector<unsigned char> config);
  char const *get_exception_msg();
  EVariableTypeMismatch(IValue &, std::string);
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

class EProcessInternal : public BuildError {
private:
  std::string cmdline;
  StreamReference reference;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EProcessInternal() = delete;
  EProcessInternal(std::string, StreamReference);
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

class EDependencyFailed : public BuildError {
private:
  std::unique_ptr<IValue> dependency;
  std::string dependency_value;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EDependencyFailed() = delete;
  EDependencyFailed(IValue &, std::string);
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

class EAdjacentWildcards : public BuildError {
private:
  IString istring;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EAdjacentWildcards() = delete;
  EAdjacentWildcards(IString);
};

class ERecursiveVariable : public BuildError {
private:
  Identifier identifier;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ERecursiveVariable() = delete;
  ERecursiveVariable(Identifier);
};

class ERecursiveTask : public BuildError {
private:
  Task task;
  std::string dependency_value;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  ERecursiveTask() = delete;
  ERecursiveTask(Task, std::string);
};

class EDuplicateIdentifier : public BuildError {
private:
  Identifier identifier_1;
  Identifier identifier_2;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EDuplicateIdentifier() = delete;
  EDuplicateIdentifier(Identifier, Identifier);
};

class EDuplicateTask : public BuildError {
  Task task_1;
  Task task_2;
  std::string key;

public:
  std::string render_error(std::vector<unsigned char> config) override;
  char const *get_exception_msg() override;
  EDuplicateTask() = delete;
  EDuplicateTask(Task, Task, std::string);
};

// a single frame in the context stack.
class Frame {
public:
  virtual std::string render_frame(std::vector<unsigned char> config) = 0;
  virtual std::string get_unique_identifier() = 0;
  virtual ~Frame() = default;
};

class EntryBuildFrame : public Frame {
private:
  std::string task;
  StreamReference reference;

public:
  std::string render_frame(std::vector<unsigned char> config) override;
  std::string get_unique_identifier() override;
  EntryBuildFrame() = delete;
  EntryBuildFrame(std::string task, StreamReference reference);
};

class DependencyBuildFrame : public Frame {
private:
  std::string task;
  StreamReference reference;

public:
  std::string render_frame(std::vector<unsigned char> config) override;
  std::string get_unique_identifier() override;
  DependencyBuildFrame() = delete;
  DependencyBuildFrame(std::string task, StreamReference reference);
};

class IdentifierEvaluateFrame : public Frame {
private:
  std::string identifier;
  StreamReference reference;

public:
  std::string render_frame(std::vector<unsigned char> config) override;
  std::string get_unique_identifier() override;
  IdentifierEvaluateFrame() = delete;
  IdentifierEvaluateFrame(std::string identifier, StreamReference reference);
};

#endif
