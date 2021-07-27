/*
MIT License

Copyright (c) 2017-2020 Matthias C. M. Troffaes

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef SKYNET_UPPER_MACHINE_CONFIG_HPP
#define SKYNET_UPPER_MACHINE_CONFIG_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/internal/iterative_base.hpp"
#include "skynet_upper/pending_iterative.hpp"
//include "skynet_core/skynet.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <iostream>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>


namespace skynet {

namespace detail {

  // trim functions based on http://stackoverflow.com/a/217605

  inline void ltrim(std::string & s, const std::locale & loc) {
    s.erase(s.begin(),
                  std::find_if(s.begin(), s.end(),
                              [&loc](char ch) { return !std::isspace(ch, loc); }));
  }

  inline void rtrim(std::string & s, const std::locale & loc) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                              [&loc](char ch) { return !std::isspace(ch, loc); }).base(),
                  s.end());
  }

  template <class UnaryPredicate>
  inline void rtrim2(std::string& s, UnaryPredicate pred) {
    s.erase(std::find_if(s.begin(), s.end(), pred), s.end());
  }

  // string replacement function based on http://stackoverflow.com/a/3418285

  inline bool replace(std::string & str, const std::string & from, const std::string & to) {
    auto changed = false;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length();
      changed = true;
    }
    return changed;
  }

} // namespace detail

template <typename T>
inline bool extract(const std::string & value, T & dst) {
  char c;
  std::istringstream is{value};
  T result;
  if ((is >> std::boolalpha >> result) && !(is >> c)) {
    dst = result;
    return true;
  }
  else {
    return false;
  }
}

template <>
inline bool extract(const std::string & value, std::string & dst) {
  dst = value;
  return true;
}

template<typename T>
inline bool extract_vector(const std::string & value, std::vector<T> & dst) {
  std::istringstream is{value};
  T result;
  while (!is.eof() && is >> std::boolalpha >> result) {
    dst.push_back(result);
  }
  return !dst.empty();
}


//template <typename char, typename T>
//inline bool get_value(const std::map<std::string, std::string> & sec, const std::string & key, T & dst) {
//  const auto it = sec.find(key);
//  if (it == sec.end()) return false;
//  return extract(it->second, dst);
//}

//template <typename char, typename T>
//inline bool get_value(const std::map<std::string, std::string>& sec, const char* key, T& dst) {
//  return get_value(sec, std::string(key), dst);
//}

class Format
{
public:
  // used for generating
  const char char_section_start;
  const char char_section_end;
  const char char_assign;
  const char char_comment;

  // used for parsing
  virtual bool is_section_start(char ch) const { return ch == char_section_start; }
  virtual bool is_section_end(char ch) const { return ch == char_section_end; }
  virtual bool is_assign(char ch) const { return ch == char_assign; }
  virtual bool is_comment(char ch) const { return ch == char_comment; }

  // used for interpolation
  const char char_interpol;
  const char char_interpol_start;
  const char char_interpol_sep;
  const char char_interpol_end;

  Format(char section_start, char section_end, char assign, char comment, char interpol, char interpol_start, char interpol_sep, char interpol_end)
    : char_section_start(section_start)
    , char_section_end(section_end)
    , char_assign(assign)
    , char_comment(comment)
    , char_interpol(interpol)
    , char_interpol_start(interpol_start)
    , char_interpol_sep(interpol_sep)
    , char_interpol_end(interpol_end) {}

  Format() : Format('[', ']', '=', ';', '$', '{', ':', '}') {}

  const std::string local_symbol(const std::string& name) const {
    return char_interpol + (char_interpol_start + name + char_interpol_end);
  }

  const std::string global_symbol(const std::string& sec_name, const std::string& name) const {
    return local_symbol(sec_name + char_interpol_sep + name);
  }
};

template<typename TagType>
struct TagGroup {
  using GroupTag = skynet::ReduceGroupTag<TagType>;
  using ValueTag = skynet::ReduceValueTag<TagType>;
  TagGroup(const GroupTag& reduce_group_tag, const ValueTag& reduce_value_tag, const std::vector<ValueTag>& reduce_value_tags)
  : reduce_group_tag(std::move(reduce_group_tag)),
    reduce_value_tag(std::move(reduce_value_tag)),
    reduce_value_tags(std::move(reduce_value_tags))
  {}

  const skynet::ReduceGroupTag<TagType> reduce_group_tag;
  const skynet::ReduceValueTag<TagType> reduce_value_tag;
  const std::vector<skynet::ReduceValueTag<TagType>> reduce_value_tags;
};


class MachineConfig
{
public:
  using string = std::string;
  using Section = std::map<string, string>;
  using Sections = std::map<string, Section>;

  Sections sections;
  std::list<string> errors;
  std::shared_ptr<Format> format;

  static const int max_interpolation_depth = 10;

  MachineConfig() : format(std::make_shared<Format>()) {};
  MachineConfig(std::shared_ptr<Format> fmt) : format(fmt) {};

  template<typename RetType>
  RetType get_value(std::string sec_name, std::string key) {
    auto sec = sections.at(sec_name);
    const auto it = sec.find(key);
    if (it == sec.end()) throw std::out_of_range("Key not found.");
    try {
      RetType ret;
      auto success = extract(it->second, ret);
      if (success) {
        return ret;
      } 
      throw std::exception();
    } catch(...) {
      return {it->second};
    }
  };

  template<typename TagType>
  TagGroup<TagType> get_reduce_group(std::string sec_name) {
    auto sec = sections.at(sec_name);

    const auto it1 = sec.find("reduce_value_tag");
    if (it1 == sec.end()) throw std::out_of_range("Missing 'reduce_value_tag' key in reduce group section " + sec_name ".");
    const auto reduce_value_tag_name = it1->second;

    const auto it2 = sec.find("reduce_value_tags");
    if (it2 == sec.end()) throw std::out_of_range("Missing 'reduce_value_tags' key in reduce group section " + sec_name ".");
    const auto reduce_value_tags_names = it2->second;

    skynet::ReduceGroupTag<TagType> reduce_group_tag{sec_name};

    skynet::ReduceValueTag<TagType> reduce_value_tag{reduce_value_tag_name};

    std::vector<skynet::ReduceValueTag<TagType>> reduce_value_tags;
    std::istringstream list(reduce_value_tags_names);
    std::string valStr;
    while (list >> valStr) {
      reduce_value_tags.emplace_back(valStr);
    }

    return {reduce_group_tag, reduce_value_tag, reduce_value_tags};
  }

  void generate(std::ostream& os) const {
    for (auto const & sec : sections) {
      os << format->char_section_start << sec.first << format->char_section_end << std::endl;
      for (auto const & val : sec.second) {
        os << val.first << format->char_assign << val.second << std::endl;
      }
      os << std::endl;
    }
  }

  void parse(std::istream& is) {
    string line;
    string section;
    const std::locale loc{"C"};
    while (std::getline(is, line)) {
      detail::ltrim(line, loc);
      detail::rtrim(line, loc);
      const auto length = line.length();
      if (length > 0) {
        const auto pos = std::find_if(line.begin(), line.end(), [this](char ch) { return format->is_assign(ch); });
        const auto & front = line.front();
        if (format->is_comment(front)) {
        }
        else if (format->is_section_start(front)) {
          if (format->is_section_end(line.back()))
            section = line.substr(1, length - 2);
          else
            errors.push_back(line);
        }
        else if (pos != line.begin() && pos != line.end()) {
          string variable(line.begin(), pos);
          string value(pos + 1, line.end());
          detail::rtrim(variable, loc);
          detail::ltrim(value, loc);
          auto & sec = sections[section];
          if (sec.find(variable) == sec.end())
            sec.insert(std::make_pair(variable, value));
          else
            errors.push_back(line);
        }
        else {
          errors.push_back(line);
        }
      }
    }
  }

  void interpolate() {
    int global_iteration = 0;
    auto changed = false;
    // replace each "${variable}" by "${section:variable}"
    for (auto & sec : sections)
      replace_symbols(local_symbols(sec.first, sec.second), sec.second);
    // replace each "${section:variable}" by its value
    do {
      changed = false;
      const auto syms = global_symbols();
      for (auto & sec : sections)
        changed |= replace_symbols(syms, sec.second);
    } while (changed && (max_interpolation_depth > global_iteration++));
  }

  void default_section(const Section & sec) {
    for (auto & sec2 : sections)
      for (const auto & val : sec)
        sec2.second.insert(val);
  }

  void strip_trailing_comments() {
    const std::locale loc{ "C" };
    for (auto & sec : sections)
      for (auto & val : sec.second) {
        detail::rtrim2(val.second, [this](char ch) { return format->is_comment(ch); });
        detail::rtrim(val.second, loc);
      }
  }

  void clear() {
    sections.clear();
    errors.clear();
  }

private:
  using Symbols = std::list<std::pair<string, string>>;

  const Symbols local_symbols(const string & sec_name, const Section & sec) const {
    Symbols result;
    for (const auto & val : sec)
      result.push_back(std::make_pair(format->local_symbol(val.first), format->global_symbol(sec_name, val.first)));
    return result;
  }

  const Symbols global_symbols() const {
    Symbols result;
    for (const auto & sec : sections)
      for (const auto & val : sec.second)
        result.push_back(
          std::make_pair(format->global_symbol(sec.first, val.first), val.second));
    return result;
  }

  bool replace_symbols(const Symbols & syms, Section & sec) const {
    auto changed = false;
    for (auto & sym : syms)
      for (auto & val : sec)
        changed |= detail::replace(val.second, sym.first, sym.second);
    return changed;
  }
};

}; // namespace machine_config

#endif
