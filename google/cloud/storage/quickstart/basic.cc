#include <pybind11/pybind11.h>

int add(int i, int j) {
  return i + j;
}

PYBIND11_MODULE(basic, module) {
  module.doc() = "A basic pybind11 extension";
  module.def("add", &add, "A function that adds two numbers");
}
