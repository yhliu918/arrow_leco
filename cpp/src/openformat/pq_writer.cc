#include <parquet/arrow/writer.h>

#include <iostream>

#include "arrow/array.h"
#include "arrow/csv/api.h"
#include "arrow/io/file.h"
#include "stats.h"

arrow::Status RunMain(int argc, char** argv) {
  std::string file_name = "Generico_2";
  arrow::io::IOContext io_context = arrow::io::default_io_context();
  ARROW_ASSIGN_OR_RAISE(
      auto input, arrow::io::ReadableFile::Open(
                      "/root/arrow-private/cpp/src/openformat/data/" + file_name + ".csv",
                      arrow::default_memory_pool()));

  auto read_options = arrow::csv::ReadOptions::Defaults();
  // read_options.column_names = {"Anunciante"};
  read_options.column_names = {"Anunciante",   "Aviso",
                               "A�o",          "Cadena",
                               "Categoria",    "Circulacion",
                               "Codigo",       "Cols",
                               "Concatenar 1", "Concatenar 2",
                               "Corte",        "De_Npags",
                               "Dia_Semana",   "Disco",
                               "Duracion",     "Est",
                               "FECHA",        "Franja",
                               "Genero",       "Holding",
                               "Hora_Pagina",  "InversionQ",
                               "InversionUS",  "Marca",
                               "Medio",        "Mes",
                               "NumAnuncios",  "Number of Records",
                               "Plgs",         "Posicion_Edicion",
                               "PrimeraLinea", "Producto",
                               "SEMANA",       "Sector",
                               "Soporte",      "Subsector",
                               "Unidad",       "VER ANUNCIO",
                               "Vehiculo",     "extencion",
                               "medio2",       "www1",
                               "www2"};
  auto parse_options = arrow::csv::ParseOptions::Defaults();
  parse_options.delimiter = '|';
  auto convert_options = arrow::csv::ConvertOptions::Defaults();

  // Instantiate TableReader from input stream and options
  ARROW_ASSIGN_OR_RAISE(auto reader,
                        arrow::csv::TableReader::Make(io_context, input, read_options,
                                                      parse_options, convert_options))

  // Read table from CSV file
  auto begin = stats::Time::now();
  ARROW_ASSIGN_OR_RAISE(auto table, reader->Read());
  stats::cout_sec(begin, "read csv");

  // std::cout << table->schema()->ToString() << std::endl;
  // auto array = table->GetColumnByName("Aviso")->chunks().at(0);
  // auto string_array = std::static_pointer_cast<arrow::StringArray>(array);
  // std::cout << string_array->Value(1) << std::endl;

  std::shared_ptr<arrow::io::FileOutputStream> outfile;
  PARQUET_ASSIGN_OR_THROW(outfile, arrow::io::FileOutputStream::Open(
                                       "/root/arrow-private/cpp/src/openformat/data/" +
                                       file_name + "_both_no.parquet"));
  // FIXME: hard code for now
  uint32_t row_group_size = 1 * 1024 * 1024;         // 64M / 10
  uint32_t dictionary_pages_size = 1 * 1024 * 1024;  // 64M * 0.03
  arrow::Compression::type codec = arrow::Compression::UNCOMPRESSED;
  std::shared_ptr<parquet::WriterProperties> properties =
      parquet::WriterProperties::Builder()
          .dictionary_pagesize_limit(dictionary_pages_size)
          ->compression(codec)
          ->disable_dictionary()
          ->build();

  begin = stats::Time::now();
  PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(),
                                                  outfile, row_group_size, properties));
  stats::cout_sec(begin, "Write pq");
  return arrow::Status::OK();
}

int main(int argc, char** argv) {
  arrow::Status status = RunMain(argc, argv);
  if (!status.ok()) {
    std::cerr << status << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}