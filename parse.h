#ifndef __SPE_PARSE_H
#define __SPE_PARSE_H

#include "logic.h"

namespace parse {
  
  logic::ValPtr parse(std::istream& i, logic::Scope& refIds);
  logic::ValPtr parse(std::istream& i);

}

#endif