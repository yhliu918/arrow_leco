#ifndef __stats_h
#define __stats_h
#include <chrono>
#include <iostream>
#include <string>

namespace stats {
typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::milliseconds ms;
typedef std::chrono::duration<double> fsec;

void print_sec(std::chrono::high_resolution_clock::time_point begin, std::string name,
               std::ostream& output) {
  std::cerr << name << "(s): " << ((fsec)(Time::now() - begin)).count() << std::endl;
  output << name << "(s): " << ((fsec)(Time::now() - begin)).count() << "\n";
}

void cout_sec(std::chrono::high_resolution_clock::time_point begin, std::string name) {
  std::cout << name << "(s): " << ((fsec)(Time::now() - begin)).count() << std::endl;
}

void print_ms(std::chrono::high_resolution_clock::time_point begin, std::string name,
              std::ostream& output) {
  output << name
         << "(ms): " << std::chrono::duration_cast<ms>(Time::now() - begin).count()
         << "\n";
}
}  // namespace stats
#endif