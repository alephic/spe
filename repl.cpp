#include "logic.h"
#include "parse.h"
#include <sstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  std::string lineStr;
  logic::Scope s;
  logic::World w;
  while (true) {
    std::cout << '>' << ' ';
    std::getline(std::cin, lineStr);
    if (lineStr == ":q") {
      return 0;
    }
    std::stringstream lineStream(lineStr);
    logic::ValPtr expr = parse::parse(lineStream);
    if (expr) {
      logic::ValSet evald = expr->eval(s, w);
      for (logic::ValPtr val : evald) {
        val->repr(std::cout);
        std::cout << std::endl;
      }
    } else {
      std::cout << "Syntax error" << std::endl;
    }
  }
}