#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace compiler::ast {

// Tensor types are canonical strings in TypeInfo::name: "tensor<float,2,3>"
// (element type, then static dimensions). These helpers convert to and from a Shape.

struct Shape {
  std::string elem;
  std::vector<long long> dims;

  /** Total number of elements. */
  long long size() const {
    long long n = 1;
    for (long long d : dims) n *= d;
    return n;
  }
};

inline bool isTensor(const std::string& t) { return t.rfind("tensor<", 0) == 0 && t.back() == '>'; }

inline Shape parseTensor(const std::string& t) {
  Shape s;
  std::stringstream in(t.substr(7, t.size() - 8));
  std::string part;
  std::getline(in, s.elem, ',');
  while (std::getline(in, part, ',')) s.dims.push_back(std::stoll(part));
  return s;
}

inline std::string tensorName(const std::string& elem, const std::vector<long long>& dims) {
  std::string name = "tensor<" + elem;
  for (long long d : dims) name += "," + std::to_string(d);
  return name + ">";
}

/** Result element type of mixing two numeric element types. */
inline std::string promote(const std::string& a, const std::string& b) {
  return (a == "float" || b == "float") ? "float" : "int";
}

}  // namespace compiler::ast
