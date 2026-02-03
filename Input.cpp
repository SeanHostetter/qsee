#include "Input.hpp"
#include <iostream>
#include <set>
#include <stack>

// --- Helper Functions ---

enum class InputLineType { SECTION_HEADER, DATA_ENTRY, CONTINUATION, EMPTY };

bool containsUnenclosedEqualSign(const std::string &s) {
  std::stack<char> st;

  for (char c : s) {
    if (c == '(' || c == '[' || c == '{') {
      st.push(c);
    } else if (c == ')' || c == ']' || c == '}') {
      if (st.empty()) {
        std::cerr << "Unmatched closing bracket in input file line:\n"
                  << s << std::endl;
        return false; // Soft fail
      }
      char top = st.top();
      st.pop();
      if ((c == ')' && top != '(') || (c == ']' && top != '[') ||
          (c == '}' && top != '{'))
        std::cerr << "Unmatched bracket in input file line:\n"
                  << s << std::endl;
    } else if (c == '=' || c == ':') {
      if (st.empty()) {
        // unenclosed '=' or ':' sign
        return true;
      }
    }
  }

  return false; // If we don't find an unenclosed '=' by the end
}

InputLineType get_input_line_type_and_trim(std::string &line) {

  // Determine position of first and last non-space character
  size_t firstNonSpace = line.find_first_not_of(" \t\r\n");
  size_t lastNonSpace = line.find_last_not_of(" \t\r\n");

  if (firstNonSpace == std::string::npos)
    return InputLineType::EMPTY;

  size_t comPos = line.find("#");

  // Skip lines in which the first non-space character is #
  // (Comment line)
  if (comPos == firstNonSpace)
    return InputLineType::EMPTY;

  // Remove comment portion of the line if it exists
  if (comPos != std::string::npos)
    line = line.substr(0, comPos);

  // Strip trailing spaces
  Input::trim_right(line);
  Input::trim_left(line);

  if (line.empty())
    return InputLineType::EMPTY;

  size_t lBrckPos = line.find('[');
  size_t rBrckPos = line.find(']');

  // Re-check non-space after trimming
  firstNonSpace = 0;
  lastNonSpace = line.length() - 1;

  // Check if we have a section header
  if (lBrckPos == firstNonSpace && rBrckPos == lastNonSpace)
    return InputLineType::SECTION_HEADER;

  // Check if we have a data entry
  if (containsUnenclosedEqualSign(line)) {
    return InputLineType::DATA_ENTRY;
  }

  // If we get here, we have a continuation line
  return InputLineType::CONTINUATION;
};

// --- Input Class Implementation ---

void Input::addData(const std::string &key, const std::string &value) {
  if (containsData(key)) {
    std::cerr << "Warning: Key " << key
              << " already exists in the parsed input. Overwriting."
              << std::endl;
  }
  dict_[key] = value;
}

void Input::mergeSection(const InputMap &subsection,
                         const std::string &prefix) {
  if (prefix == "")
    for (auto &kv : subsection) {
      addData(kv.first, kv.second);
    }
  else
    for (auto &kv : subsection) {
      addData(prefix + "." + kv.first, kv.second);
    }
}

std::pair<std::string, std::string>
Input::splitQuery(const std::string &query) {
  std::vector<std::string> tokens;
  split(tokens, query, ".");
  for (auto &X : tokens) {
    trim(X);
    std::transform(X.begin(), X.end(), X.begin(),
                   [](unsigned char c) { return std::toupper(c); });
  }

  if (tokens.size() < 2)
    return {query, ""};
  return {tokens[0], tokens[1]};
}

void Input::parse() {
  if (!inFile_->good()) {
    throw std::runtime_error("Input File Couldn't Be Found!");
  }

  // Read in all lines of the file
  std::vector<std::string> lines;
  while (!inFile_->eof()) {
    std::string line;
    std::getline(*inFile_, line);
    lines.push_back(line);
  }

  // Parse the file
  parse(lines.cbegin(), lines.cend(), "");
}

void Input::parse(std::vector<std::string>::const_iterator lines_begin,
                  std::vector<std::string>::const_iterator lines_end,
                  const std::string &prefix) {

  auto strToUpper = [](std::string &s) {
    std::for_each(s.begin(), s.end(), [](char &c) { c = std::toupper(c); });
  };

  std::string sectionHeader;
  std::string dataHeader;

  // Keywords that are case sensitive (do *not* transform data to UPPER)
  std::set<std::string> caseSens, caseSensReverse;

  // Add case sensitive data keywords here
  caseSens.insert("BASIS.BASIS");

  // Reverse entries in caseSens
  for (auto &sec : caseSens)
    caseSensReverse.insert(reverse_by_dot(sec));

  // Loop over all lines of the file
  for (auto line_iter = lines_begin; line_iter != lines_end; ++line_iter) {

    std::string line = *line_iter;

    InputLineType lineType = get_input_line_type_and_trim(line);

    // Skip empty lines
    if (lineType == InputLineType::EMPTY)
      continue;

    // Section line
    if (lineType == InputLineType::SECTION_HEADER) {

      // Obtain the section header name
      sectionHeader = line.substr(1, line.length() - 2);

      // Convert to UPPER
      strToUpper(sectionHeader);

      continue;
    }

    // Data line
    if (lineType == InputLineType::DATA_ENTRY) {

      // Find first = or : and get substring before and after it
      size_t equalIndex = line.find_first_of("=:");
      dataHeader = line.substr(0, equalIndex);
      trim(dataHeader);
      std::string value = line.substr(equalIndex + 1);
      trim(value);

      strToUpper(dataHeader);
      if (sectionHeader != "")
        dataHeader = sectionHeader + "." + dataHeader;

      // Check if the data entry has continuation lines below
      while (line_iter + 1 != lines_end) {
        std::string next_line = *(line_iter + 1);
        std::string lookahead = next_line;
        InputLineType next_line_type = get_input_line_type_and_trim(lookahead);

        // End while loop if next line is not a continuation line or empty
        if (next_line_type != InputLineType::CONTINUATION &&
            next_line_type != InputLineType::EMPTY)
          break;

        if (next_line_type == InputLineType::CONTINUATION)
          value += "\n" + lookahead; // Use the trimmed version
        ++line_iter;
      }

      // Capitalize data if not case sensitive
      auto it = caseSensReverse.lower_bound(reverse_by_dot(dataHeader));
      bool isCaseSensitive = (it != caseSensReverse.end() &&
                              it->find(reverse_by_dot(dataHeader)) == 0);

      if (!isCaseSensitive && !value.empty()) {
        // Don't uppercase everything blindly as some values might be paths or
        // specific strings But original code did strToUpper(value). Let's stick
        // to original logic for consistency unless it breaks things.
        strToUpper(value);
      }

      // Create a dictionary entry
      if (!value.empty())
        addData(dataHeader, value);
      else
        std::cerr << "Warning: No data entry for " << dataHeader
                  << " in input file." << std::endl;
    }
  }
}

bool Input::containsSection(const std::string &str) const {
  auto it = dict_.lower_bound(str);
  if (it == dict_.end())
    return false;

  if (it->first == str)
    it++;
  if (it == dict_.end())
    return false;
  return it->first.find(str) == 0 && it->first.size() > str.size() &&
         it->first[str.size()] == '.';
}

bool Input::containsList(const std::string &str) const {
  auto it = dict_.lower_bound(str);
  if (it == dict_.end())
    return false;
  while (it->first.find(str) == 0) {
    if (it->first.size() > str.size() && it->first[str.size()] == '[')
      return true;
    it++;
  }
  return false;
}

size_t Input::getListSize(const std::string &str) const {
  if (!containsList(str))
    return 0;
  size_t max_index = 0;
  auto it = dict_.lower_bound(str);
  while (it->first.find(str) == 0) {
    if (it->first.size() > str.size() && it->first[str.size()] == '[')
      max_index = std::max(
          max_index, InputKeyCompare::extractNumber(it->first, str.size() + 1));
    it++;
  }
  return max_index + 1;
}

bool Input::containsData(std::string str) const {
  return dict_.find(str) != dict_.end();
}

std::vector<std::string> Input::getDataInSection(std::string section) const {
  std::set<std::string> datasets;
  std::string::size_type lenSection = section.size();
  auto it = dict_.lower_bound(section);
  while (it != dict_.end()) {
    const std::string &key = it->first;
    if (key.find(section) != 0)
      break;

    if (key.size() > lenSection) {
      std::string::size_type nextDotPos = key.find('.', lenSection + 1);

      if (nextDotPos == std::string::npos) {
        datasets.emplace(key.substr(lenSection + 1));
      } else {
        datasets.emplace(
            key.substr(lenSection + 1, nextDotPos - lenSection - 1));
      }
    }
    ++it;
  }
  return std::vector<std::string>(datasets.begin(), datasets.end());
}

InputMap Input::getSection(const std::string &section) const {
  if (!containsSection(section) &&
      !containsData(section)) // Slight logic tweak: check if it exists at all
    return {};

  InputMap sectionData;
  auto it = dict_.lower_bound(section);
  if (it->first == section)
    it++; // Skip exact match if it's a data entry
  while (it != dict_.end()) {
    const std::string &key = it->first;
    if (key.find(section) != 0 || key[section.size()] != '.')
      break;

    if (key.size() > section.size())
      sectionData.emplace(key.substr(section.size() + 1), it->second);

    ++it;
  }
  return sectionData;
}

// Specializations
template <> std::string Input::getData(std::string query) {
  auto kv = dict_.find(query);
  if (kv != dict_.end())
    return kv->second;
  else
    throw std::runtime_error("Data " + query + " Not Found");
}

template <> int Input::getData(std::string query) {
  return std::stoi(getData<std::string>(query));
}

template <> bool Input::getData(std::string query) {
  query = getData<std::string>(query);
  if (query == "TRUE" || query == "ON")
    return true;
  if (query == "FALSE" || query == "OFF")
    return false;
  throw std::runtime_error("Invalid Boolean Input: " + query);
}

template <> size_t Input::getData(std::string query) {
  return std::stoul(getData<std::string>(query));
}

template <> double Input::getData(std::string query) {
  return std::stod(getData<std::string>(query));
}
