#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
#include <string>

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

int main(int argc, char *argv[]) {
  std::string dep_name(argv[1]);

  if (auto env_p = std::getenv("BUILD_WORKING_DIRECTORY")) {
    std::filesystem::current_path(env_p);
  }

  auto module_path = get_module_file();

  if (!module_path) {
    std::cerr << "Couldn't find MODULE.bazel file to write to" << std::endl;
    return 1;
  }

  auto absolute_module_path = absolute(*module_path);
  auto stream = std::fstream(absolute_module_path,
                             std::ios_base::in | std::ios_base::out);

  if (!stream) {
    std::cerr << "Stream attempted on invalid file" << std::endl;
  }

  stream.seekg(0, std::ios_base::beg);

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

    std::cout << "found dep" << std::endl;
    dep_full_name = line_str.substr( //
        first_quote_index + 1,       //
        second_quote_index - first_quote_index - 1);
    break;
  } while (!stream.eof());

  if (dep_full_name == std::nullopt) {
    std::cerr << "Failed to find dependency for local_path_override: "
              << dep_name << std::endl;
    return 1;
  } else {
    std::cout << "FULL NAME : " << *dep_full_name << std::endl;
  }

  auto current_path = std::filesystem::current_path();
  auto directory_iter = std::filesystem::directory_iterator(current_path);

  auto found_dir = find_dir(current_path, dep_full_name);

  if (!found_dir) {
    std::cerr << "Couldn't find given repo directory" << std::endl;
    return 1;
  }

  std::cout << "PATH: " << current_path << std::endl;

  if (stream) {
    stream.clear();
    stream.seekp(0, std::ios_base::end);

    // stream.open(absolute_module_path, std::ios_base::app);
    stream << "local_path_override(\n"
           << "\t module_name=\"" << *dep_full_name << "\",\n"
           << "\t path=\"" << absolute_module_path.generic_string() << "\",\n"
           << ")";
  } else {
    std::cerr << "Stream attempted in on invalid file or an operation failed"
              << std::endl;
    return 1;
  }

  stream.flush();
  stream.close();

  return 0;
}
