#ifndef INTERPRETER_TYPES_HPP
#define INTERPRETER_TYPES_HPP

#include "../lexer/tracking.hpp"
#include "../lexer/types.hpp"
#include <string>
#include <vector>
#include <memory>

struct IString;
struct IBool;
template <typename T> struct IList;

enum class IType {
  IString,
  IBool,
  IList_IString,
  IList_IBool,
};

class IValue {
private:
  virtual IString cast_to_istring() = 0;
  virtual IBool cast_to_ibool() = 0;
  virtual IList<IString> cast_to_ilist_istring() = 0;
  virtual IList<IBool> cast_to_ilist_ibool() = 0;

public:
  bool immutable;
  StreamReference reference;
  virtual IType get_type() = 0;
  virtual std::unique_ptr<IValue> clone() = 0;

  template <typename T> T autocast();

  IValue() = delete;
  IValue(bool immutable, StreamReference reference)
      : immutable(immutable), reference(reference) {};
  virtual ~IValue() = default;
};

class IString : public IValue {
private:
  IString cast_to_istring() override;
  IBool cast_to_ibool() override;
  IList<IString> cast_to_ilist_istring() override;
  IList<IBool> cast_to_ilist_ibool() override;

public:
  IType get_type() override;
  std::unique_ptr<IValue> clone() override;

  std::string content;
  std::string to_string() const;

  IString() = delete;
  IString(Token, bool);
  IString(std::string, StreamReference, bool);
  bool operator==(IString const other) const;
};

class IBool : public IValue {
private:
  IString cast_to_istring() override;
  IBool cast_to_ibool() override;
  IList<IString> cast_to_ilist_istring() override;
  IList<IBool> cast_to_ilist_ibool() override;

public:
  IType get_type() override;
  std::unique_ptr<IValue> clone() override;

  bool content;

  IBool() = delete;
  IBool(Token, bool);
  IBool(bool, StreamReference, bool);
  operator bool() const;
  bool operator==(IBool const other) const;
};

template <typename T> class IList : public IValue {
private:
  IString cast_to_istring() override;
  IBool cast_to_ibool() override;
  IList<IString> cast_to_ilist_istring() override;
  IList<IBool> cast_to_ilist_ibool() override;

public:
  IType get_type() override;
  std::unique_ptr<IValue> clone() override;

  std::vector<T> contents;

  IList() = delete;
  IList(std::vector<T>, StreamReference reference, bool);
  bool operator==(IList const other) const;
};

#endif
