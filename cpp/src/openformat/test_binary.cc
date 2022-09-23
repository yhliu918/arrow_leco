#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

template <typename T>
static std::vector<T> load_data_binary(const std::string& filename, bool print = true) {
  std::vector<T> data;

  std::ifstream in(filename, std::ios::binary);
  if (!in.is_open()) {
    std::cerr << "unable to open " << filename << std::endl;
    exit(EXIT_FAILURE);
  }
  // Read size.
  uint64_t size;
  in.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
  data.resize(size);
  // Read values.
  in.read(reinterpret_cast<char*>(data.data()), size * sizeof(T));
  in.close();

  return data;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <filename>" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::string filename = argv[1];
  std::vector<uint64_t> data = load_data_binary<uint64_t>(filename);

  std::cout << "size: " << data.size() << std::endl;
  std::cout << "first 10 values: ";
  std::copy(data.begin(), data.begin() + 10,
            std::ostream_iterator<uint64_t>(std::cout, " "));
  std::cout << std::endl;

  uint32_t cnt32 = 0;
  uint32_t cnt64 = 0;
  for (size_t i = 0; i < data.size(); ++i) {
    if (data[i] <= std::numeric_limits<int32_t>::max()) {
      cnt32++;
    } else {
      cnt64++;
    }
  }
  std::cout << "32-bit values: " << cnt32 << std::endl;
  std::cout << "64-bit values: " << cnt64 << std::endl;

  return 0;
}