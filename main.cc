#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>

using namespace std::filesystem;

auto find_dir(const std::filesystem::path &path,
              const auto dep_name) -> std::optional<std::filesystem::path> {
  auto directory_iter = std::filesystem::directory_iterator(path);

  for (const auto &dir : directory_iter) {
    if (dir.is_directory() && dir.path().has_filename()) {
      if (dir.path().filename() == dep_name) {
        return dir.path();
      }

      auto all_dirs = directory_iterator(dir);

      for (const auto &nested_dir : all_dirs) {

        if (nested_dir.path().filename() == dep_name &&
            nested_dir.is_directory()) {
          return nested_dir.path();
        }
      }
    }
  }
  if (path.has_parent_path()) {
    return find_dir(path.parent_path(), dep_name);
  } else {
    return std::nullopt;
  }
}

auto get_module_file() -> std::optional<path> {
  auto current_path = std::filesystem::current_path();
  auto directory_iter = std::filesystem::directory_iterator(current_path);

  for (const auto &dir : directory_iter) {

    if (dir.path().has_extension()) {
      if (dir.path().filename() == "MODULE.bazel" &&
          dir.path().extension() == ".bazel") {
        return dir.path();
      }
    }
  }
  return std::nullopt;
}

auto log_read_error() -> int {
  std::cerr << "Found bazel_dep but search failed. bzloverride looks for "
               "name=\"dep_name\" syntax"
            << std::endl;
  return 1;
}

enum LineEnding {
  LF = 0,
  CRLF = 1,
};

auto determine_line_ending(std::fstream &stream) -> LineEnding {
  std::string first_line;
  std::getline(stream, first_line);
  if (first_line.ends_with("\r\n")) {
    stream.seekg(0, std::ios_base::beg);
    return LineEnding::CRLF;
  }
  stream.seekg(0, std::ios_base::beg);
  return LineEnding::LF;
}

auto print_local_override(std::string dep_name) -> int {

  if (auto env_p = std::getenv("BUILD_WORKING_DIRECTORY")) {
    std::filesystem::current_path(env_p);
  }

  auto module_path = get_module_file();

  if (!module_path) {
    std::cerr << "Couldn't find MODULE.bazel file to write to" << std::endl;
    return 1;
  }

  auto absolute_module_path = absolute(*module_path);
  auto stream = std::fstream( //
      absolute_module_path,   //
      std::ios_base::in | std::ios_base::binary | std::ios_base::out);

  if (!stream) {
    std::cerr << "Stream attempted on an invalid file" << std::endl;
  }

  stream.seekg(0, std::ios_base::beg);

  auto line_ending = determine_line_ending(stream);

  std::optional<std::string> dep_full_name = std::nullopt;

  do {
    std::string line_str;
    std::getline(stream, line_str);

    if (!line_str.starts_with("bazel_dep"))
      continue;

    auto dir_index = line_str.find(dep_name);

    if (dir_index == std::string::npos)
      continue;

    auto name_index = line_str.find("name =");
    if (name_index == std::string::npos) {
      return log_read_error();
      break;
    }

    auto first_quote_index = line_str.find("\"", name_index);
    if (first_quote_index == std::string::npos) {
      return log_read_error();
      break;
    }

    auto second_quote_index = line_str.find("\"", first_quote_index + 1);
    if (second_quote_index == std::string::npos) {
      return log_read_error();
      break;
    }

    dep_full_name = line_str.substr( //
        first_quote_index + 1,       //
        second_quote_index - first_quote_index - 1);
    break;
  } while (!stream.eof());

  if (dep_full_name == std::nullopt) {
    std::cerr << "Failed to find dependency for local_path_override: "
              << dep_name << std::endl;
    return 1;
  }

  auto current_path = std::filesystem::current_path();
  auto directory_iter = std::filesystem::directory_iterator(current_path);

  auto found_dir = find_dir(current_path, dep_full_name);

  if (!found_dir) {
    std::cerr << "Couldn't find given bazel_dep directory" << std::endl;
    return 1;
  }

  if (stream) {
    stream.clear();
    stream.seekp(0, std::ios_base::end);

    std::error_code ec;

    auto rel_path = std::filesystem::relative(
        *found_dir, absolute_module_path.parent_path(), ec);

    if (ec) {
      std::cerr << "ERR: " << rel_path << std::endl;
    }

    if (line_ending == LineEnding::CRLF) {
      stream << "\r\nlocal_path_override(\r\n"
             << "    " << "module_name = \"" << *dep_full_name << "\",\r\n"
             << "    " << "path = \"" << rel_path.generic_string() << "\",\r\n"
             << ")\r\n";
    } else {
      stream << "\nlocal_path_override(\n"
             << "    " << "module_name = \"" << *dep_full_name << "\",\n"
             << "    " << "path = \"" << rel_path.generic_string() << "\",\n"
             << ")\n";
    }
  } else {
    std::cerr << "Stream attempted on an invalid file or previous read "
                 "operation failed"
              << std::endl;
    return 1;
  }

  stream.flush();
  stream.close();

  return 0;
}

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    auto exit_code = print_local_override(std::string(argv[i]));
    if (exit_code != 0) {
      return exit_code;
    }
  }
}
