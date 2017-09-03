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
    } else {
      std::stringstream lineStream(lineStr);
      if (lineStr.substr(0, 4) == ":def") {
        lineStream.ignore(4);
        parse::skipWhitespace(lineStream);
        logic::SymId name = parse::parseSymId(lineStream);
        if (name.size() > 0) {
          logic::Shadow sh = logic::Shadow(&s);
          sh.shadow(name);
          logic::ValPtr expr = parse::parse(lineStream, sh);
          if (expr) {
            logic::ValSet evald = expr->eval(sh, w);
            s.add(name, evald);
            continue;
          }
        }
      } else if (lineStr.substr(0, 5) == ":decl") {
        lineStream.ignore(5);
        logic::ValPtr expr = parse::parse(lineStream, s);
        if (expr) {
          logic::ValSet evald = expr->eval(s, w);
          for (logic::ValPtr val : evald) {
            w.add(val);
          }
          continue;
        }
      } else {
        logic::ValPtr expr = parse::parse(lineStream, s);
        if (expr) {
          logic::ValSet evald = expr->eval(s, w);
          for (logic::ValPtr val : evald) {
            val->repr(std::cout);
            std::cout << std::endl;
          }
          continue;
        }
      }
    }
    std::cout << "Syntax error" << std::endl;
  }
}