#include <iostream>
#include <algorithm>
#include <variant>
#include <bit>
#include <ostream>


struct A {
  int x;
};

void f(const A&& a) {

}

int main() {
  std::cout << "here" << std::endl;
  std::variant<int> v{1};
  std::popcount(123ull); 

  f(A{.x=1});

}