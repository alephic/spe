#include "logic.h"
#include "parse.h"
#include <sstream>
#include <iostream>
#include <string>
#include <readline/readline.h>
#include <readline/history.h>

int main(int argc, char** argv) {
  std::string lineStr;
  logic::Scope s;
  logic::World w;
  while (true) {
    char *lineCstr = readline("> ");
    if (lineCstr == nullptr) {
      continue;
    }
    std::string lineStr(lineCstr);
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
            add_history(lineCstr);
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
          add_history(lineCstr);
          continue;
        }
      } else if (lineStr.substr(0, 6) == ":check") {
        lineStream.ignore(6);
        logic::ValPtr expr = parse::parse(lineStream, s);
        if (expr) {
          bool holds(false);
          logic::ValSet evald = expr->eval(s, w);
          for (logic::ValPtr val : evald) {
            if (w.get_matches(val).size() > 0) {
              holds = true;
              break;
            }
          }
          std::cout << (holds ? "# Holds" : "# Does not hold") << std::endl;
          add_history(lineCstr);
          continue;
        }
      } else {
        logic::ValPtr expr = parse::parse(lineStream, s);
        if (expr) {
          logic::ValSet evald = expr->eval(s, w);
          if (evald.size() == 0) {
            std::cout << "# No result" << std::endl;
          } else for (logic::ValPtr val : evald) {
            val->repr(std::cout);
            std::cout << std::endl;
          }
          add_history(lineCstr);
          continue;
        }
      }
    }
    free(lineCstr);
    std::cout << "Syntax error" << std::endl;
  }
}