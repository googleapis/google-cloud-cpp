#include <algorithm>  // For std::find_if
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Helper function to trim leading whitespace from a string
std::string TrimLeadingWhitespace(std::string const& s) {
  size_t first_char_pos = s.find_first_not_of(" 	\n\r\f\v");
  if (std::string::npos == first_char_pos) {
    return "";  // Entire string is whitespace
  }
  return s.substr(first_char_pos);
}

std::string ReadFile(std::string const& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("Cannot open file: " + path);
  }
  return std::string(std::istreambuf_iterator<char>(stream), {});
}

std::string ExtractSnippet(std::string const& full_path,
                           std::string const& tag) {
  std::string content;
  try {
    content = ReadFile(full_path);
  } catch (std::exception const& e) {
    return "SNIPPET NOT FOUND: " + full_path;
  }

  std::string snippet_raw;
  std::string start_tag = "//! [" + tag + "]";
  std::stringstream ss(content);
  std::string line;
  bool in_snippet = false;
  while (std::getline(ss, line)) {
    if (line.find(start_tag) != std::string::npos) {
      if (in_snippet) {
        break;  // End of snippet
      }
      in_snippet = true;  // Start of snippet
      continue;
    }
    if (in_snippet) {
      snippet_raw += line + "\n";
    }
  }

  // Now process snippet_raw to remove common leading indentation
  std::stringstream snippet_ss(snippet_raw);
  std::string processed_snippet;
  std::string current_line;

  // Find the minimum indentation level of non-empty lines
  size_t min_indent = std::string::npos;
  std::vector<std::string> lines_to_process;

  while (std::getline(snippet_ss, current_line)) {
    lines_to_process.push_back(current_line);
    size_t indent = current_line.find_first_not_of(" 	");
    if (indent != std::string::npos &&
        (min_indent == std::string::npos || indent < min_indent)) {
      min_indent = indent;
    }
  }

  // Apply the minimum indentation removal to all lines
  for (auto const& l : lines_to_process) {
    if (min_indent != std::string::npos && l.length() >= min_indent) {
      processed_snippet += l.substr(min_indent) + "\n";
    } else {
      processed_snippet += l + "\n";
    }
  }

  return processed_snippet;
}

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <url_prefix> <markdown_file>\n";
    return 1;
  }

  std::string url_prefix = argv[1];
  std::string markdown_file = argv[2];
  std::string markdown_dir =
      markdown_file.substr(0, markdown_file.find_last_of("/\\"));

  std::string content;
  try {
    content = ReadFile(markdown_file);
  } catch (std::exception const& e) {
    std::cerr << "Error reading markdown file: " << e.what() << "\n";
    return 1;
  }

  // Transform code snippets
  std::regex snippet_re(R"(\[!code-cpp\[\]\((.*?)\)\])");
  std::string transformed_content;
  auto it = std::sregex_iterator(content.begin(), content.end(), snippet_re);
  auto end = std::sregex_iterator();
  auto last_match = content.cbegin();

  for (; it != end; ++it) {
    auto const& match = *it;
    transformed_content.append(last_match, match.prefix().second);
    last_match = match.suffix().first;

    std::string path_and_tag = match[1].str();
    auto pos = path_and_tag.find('#');
    if (pos == std::string::npos) continue;

    std::string path = path_and_tag.substr(0, pos);
    std::string tag = path_and_tag.substr(pos + 1);

    std::string full_snippet_path = markdown_dir;
    full_snippet_path += "/";
    full_snippet_path += path;

    std::string snippet = ExtractSnippet(full_snippet_path, tag);
    transformed_content += "```cpp\n" + snippet + "```";
  }
  transformed_content.append(last_match, content.cend());
  content = transformed_content;  // Update content with snippet replacements

  // Transform relative links to absolute URLs
  std::regex link_re(R"(\[(.*?)\]\((/?)([^:h#][^)]*)\))");
  content = std::regex_replace(content, link_re, "[$1](" + url_prefix + "/$3)");

  std::cout << content;

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception thrown: " << ex.what() << "\n";
  return 1;
}
