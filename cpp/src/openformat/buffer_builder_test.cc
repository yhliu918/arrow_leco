#include <arrow/buffer_builder.h>

int main(int argc, char** argv) {
  arrow::BufferBuilder builder;
  builder.Append("Hello, World!", 10);
  return 0;
}