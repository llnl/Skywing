#ifndef SKYWING_EXAMPLE_ARG_PARSER_HPP
#define SKYWING_EXAMPLE_ARG_PARSER_HPP

#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>

class ArgParser
{
public:
  ArgParser(std::vector<std::string> params,
	    int argc, char* argv[])
    : params_(params)
  {
    parse_args(argc, argv);
  }

  template<typename T>
  T get_arg(std::string key)
  {
    if (auto search = parsed_value_strs_.find(key); search != parsed_value_strs_.end())
    {
      std::istringstream value_stream(search->second);
      T ret_val;
      value_stream >> ret_val;
      return ret_val;
    }
    else
      throw std::runtime_error("Missing required parameter: " + key);
  }

  template<typename T>
  T get_arg(std::string key, T default_value)
  {
    if (auto search = parsed_value_strs_.find(key); search != parsed_value_strs_.end())
    {
      std::istringstream value_stream(search->second);
      T ret_val;
      value_stream >> ret_val;
      return ret_val;
    }
    else
      return default_value;
  }

private:
  void parse_args(int argc, char* argv[])
  {
    for (int i = 1; i < argc; i++)
    {
      std::string arg = argv[i];
      auto pos = arg.find('=');
      if (pos != std::string::npos)
      {
	std::string name = arg.substr(0, pos);
	std::string value_str = arg.substr(pos + 1);
	if (std::find(params_.begin(), params_.end(), name) != params_.end())
	  parsed_value_strs_[name] = value_str;
	else
	  throw std::runtime_error("Unknown parameter: " + name);
      }
      else
	throw std::runtime_error("Incorrect format for " + arg + "\nMust be of the form [key]=[value]");
    }
  }

  std::unordered_map<std::string, std::string> parsed_value_strs_;
  std::vector<std::string> params_;
}; // class ArgParser

#endif // SKYWING_TEST_ARG_PARSER_HPP
