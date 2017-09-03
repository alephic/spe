#include "parse.h"
#include <sstream>
#include <iostream>

namespace parse {
  
  bool isSymChar(char c) {
    switch (c) {
    case '(':
    case ')':
    case '[':
    case ']':
    case '<':
    case '>':
    case '{':
    case '}':
    case '*':
    case '?':
    case ' ':
    case '\t':
    case '\r':
    case '\n':
    case '\f':
    case EOF:
      return false;
    default:
      return true;
    }
  }

  logic::SymId parseSymId(std::istream& i) {
    std::stringstream ss;
    while (isSymChar(i.peek())) {
      ss << (char) i.get();
    }
    return ss.str();
  }

  void skipWhitespace(std::istream& i) {
    while (true) {
      switch(i.peek()) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        case '\f':
          i.get();
          continue;
        default:
          return;
      }
    }
  }
  
  logic::ValPtr parse_not_apply(std::istream& i, logic::Scope& refIds) {
    skipWhitespace(i);
    char c = i.peek();
    switch (c) {
    case '(': {
      i.get();
      logic::ValPtr p = parse(i, refIds);
      if (p && i.peek() == ')') {
        i.get();
        return p;
      } else {
        return logic::ValPtr();
      }
    }
    case '<': {
      i.get();
      skipWhitespace(i);
      logic::SymId argId = parseSymId(i);
      skipWhitespace(i);
      if (i.peek() == '>') {
        i.get();
        skipWhitespace(i);
        logic::Scope refIds2 = logic::Scope(&refIds);
        logic::ValSet empty;
        refIds2.add(argId, empty);
        logic::ValPtr body = parse(i, refIds2);
        if (body) {
          return logic::bundle(new logic::Lambda(argId, body));
        }
      } else {
        return logic::ValPtr();
      }
    }
    case '[': {
      i.get();
      skipWhitespace(i);
      logic::ValPtr constraint = parse(i, refIds);
      skipWhitespace(i);
      if (i.peek() == ']') {
        i.get();
        skipWhitespace(i);
        if (logic::ValPtr body = parse(i, refIds)) {
          return logic::bundle(new logic::Constrain(constraint, body));
        }
      } else {
        return logic::ValPtr();
      }
    }
    case '{': {
      i.get();
      skipWhitespace(i);
      logic::ValPtr with = parse(i, refIds);
      skipWhitespace(i);
      if (i.peek() == '}') {
        i.get();
        skipWhitespace(i);
        if (logic::ValPtr body = parse(i, refIds)) {
          return logic::bundle(new logic::Declare(with, body));
        }
      } else {
        return logic::ValPtr();
      }
    }
    case '*':
      i.get();
      return logic::Wildcard::INSTANCE;
    case '?':
      i.get();
      return logic::Arbitrary::INSTANCE;
    default:
      logic::SymId symId = parseSymId(i);
      if (symId.size() == 0) {
        return logic::ValPtr();
      }
      if (refIds.has(symId)) {
        return logic::bundle(new logic::Ref(symId));
      } else {
        return logic::bundle(new logic::Sym(symId));
      }
    }
  }

  logic::ValPtr parse(std::istream& i) {
    logic::Scope refIds;
    return parse(i, refIds);
  }

  logic::ValPtr parse(std::istream& i, logic::Scope& refIds) {
    logic::ValPtr curr = parse_not_apply(i, refIds);
    if (!curr) {
      return curr;
    }
    while (true) {
      skipWhitespace(i);
      logic::ValPtr next = parse_not_apply(i, refIds);
      if (next) {
        curr = logic::bundle(new logic::Apply(curr, next));
      } else {
        return curr;
      }
    }
  }
}
