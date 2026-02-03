#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * \brief A class to compare two strings in lexicographic order with exceptions:
 *        1. Dots are before any other characters
 *        2. Numbers in brackets are compared before any other characters
 *        3. Numbers in brackets are compared numerically
 */
struct InputKeyCompare {
  static size_t extractNumber(const std::string &str, size_t index) {
    size_t num = 0;
    while (index < str.size() && str[index] != ']') {
      if (std::isdigit(str[index])) {
        num = num * 10 + (str[index] - '0');
      } else {
        return std::numeric_limits<size_t>::max();
      }
      ++index;
    }
    return num;
  }

  bool operator()(const std::string &a, const std::string &b) const {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
      if (a[i] == '[' && b[j] == '[') {
        size_t numA = extractNumber(a, i + 1);
        size_t numB = extractNumber(b, j + 1);
        if (numA != numB)
          return numA < numB;
      } else if (a[i] == '[') {
        if (b[j] == '.')
          return false;
        return true;
      } else if (b[j] == '[') {
        if (a[i] == '.')
          return true;
        return false;
      } else {
        if (a[i] != b[j]) {
          if (a[i] == '.')
            return true;
          if (b[j] == '.')
            return false;
          return a[i] < b[j];
        }
      }
      ++i;
      ++j;
    }
    return a.size() < b.size();
  }
};

typedef std::map<std::string, std::string, InputKeyCompare> InputMap;

/**
 *  \brief A class to handle the parsing a data fetching from an input file.
 */
class Input {

  std::shared_ptr<std::ifstream> inFile_ = nullptr; ///< Input file
  InputMap dict_; ///< Input data fields partitioned by section headings

  /**
   * \brief Add an key-value pair to the input storage
   * \param [in] key   Key of the data field
   * \param [in] value Value of the data field
   */
  void addData(const std::string &key, const std::string &value);

  /**
   * \brief Merge a subsection into the input storage
   * \param [in] subsection Subsection to be merged
   * \param [in] prefix Prefix of the data field
   */
  void mergeSection(const InputMap &subsection, const std::string &prefix = "");

  // Splits query string on "."
  static std::pair<std::string, std::string> splitQuery(const std::string &);

public:
  Input() = delete;

  // Filename constructor.
  Input(std::string inFileName)
      : inFile_(std::make_shared<std::ifstream>(inFileName)) {
    if (!inFile_->is_open()) {
      throw std::runtime_error("Could not open file: " + inFileName);
    }
  }

  // Parses the input file
  void parse();

  // Parses a section of the input file
  void parse(std::vector<std::string>::const_iterator lines_begin,
             std::vector<std::string>::const_iterator lines_end,
             const std::string &prefix);

  /**
   *  \brief Template function which returns the value of a data field
   *  from the input file in a specified datatype given a formatted
   *  query string.
   */
  template <typename T> T getData(std::string key);

  /**
   *  Checks whether or not the parsed input file contains
   *  a query section.
   */
  bool containsSection(const std::string &str) const;

  /**
   *  Checks whether or not the parsed input file contains
   *  a query list.
   */
  bool containsList(const std::string &str) const;

  /**
   *  Checks the size of a query list.
   */
  size_t getListSize(const std::string &str) const;

  /**
   *  Checks whether or not the parsed input file contains
   *  a query data field.
   */
  bool containsData(std::string str) const;

  std::vector<std::string> getDataInSection(std::string section) const;

  InputMap getSection(const std::string &section) const;

  // Access the underlying dictionary (for debug/display)
  const InputMap &getDict() const { return dict_; }

  // Misc string functions
  static inline std::string &trim_left(std::string &s) {
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), [](auto &x) {
              return std::isspace(x);
            }));
    return s;
  }

  static inline std::string &trim_right(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](auto &x) { return !std::isspace(x); })
                .base(),
            s.end());
    return s;
  }

  static inline std::string &trim(std::string &s) {
    return trim_left(trim_right(s));
  }

  static inline void split(std::vector<std::string> &tokens,
                           const std::string &str,
                           const std::string &delimiters = " ") {
    tokens.clear();
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    std::string::size_type pos = str.find_first_of(delimiters, lastPos);
    while (std::string::npos != pos || std::string::npos != lastPos) {
      tokens.push_back(str.substr(lastPos, pos - lastPos));
      lastPos = str.find_first_not_of(delimiters, pos);
      pos = str.find_first_of(delimiters, lastPos);
    }
  }

  static inline std::string reverse_by_dot(const std::string &str) {
    std::vector<std::string> tokens;
    split(tokens, str, ".");
    std::string reversed = "";
    for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
      reversed += *it;
      if (it != tokens.rend() - 1)
        reversed += ".";
    }
    return reversed;
  }
};
