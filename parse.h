#ifndef __SPE_PARSE_H
#define __SPE_PARSE_H

#include "logic.h"

namespace parse {

  void skipWhitespace(std::istream& i);

  logic::SymId parseSymId(std::istream& i);
  
  logic::ValPtr parse(std::istream& i, logic::Scope& refIds);
  logic::ValPtr parse(std::istream& i);

}

#endif