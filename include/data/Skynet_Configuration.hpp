#ifndef SKYNET_CONFIGURATION_HPP__
#define SKYNET_CONFIGURATION_HPP__

#include <fstream>
#include <iostream>
#include <map>

namespace skynet
{
  /** \class Configuration
   * \brief Configuration data for the Skynet instance
   *
   */
  class Configuration
  {
  public:
    /** \brief Construct a new Configuration.
     *
     * \param filename Specifies configuration filename to be read in
     */
    Configuration(std::string filename, std::string delimiter)
    {
      std::ifstream infile(filename);
      if (!infile.is_open())
      {
        std::cout << "Configuration file " << filename << " could not be opened."
          << std::endl;
        exit(-1);
      }
      std::string line;
      while (getline(infile,line))
      {
        dictionary_[line.substr(0, line.find(delimiter))] =
          line.substr(line.find(delimiter)+1, line.length());
      }
      infile.close();
    }

    /** \brief Get value of key from configuration
     *
     * \param key Specifies name of the key
     * \return The value of the specified key
     */
    std::string get_value(std::string key)
    {
      if (dictionary_.count(key) > 0)
        return dictionary_[key];
      else
      {
        std::cout << "Configuration file does not contain key " << key << std::endl;
        exit(-1);
      }
    }

    /** \brief Return whether a particular key exists in the configuration
     *
     * \param key Specifies name of the key
     * \return Whether the key exists in the dictionary
     */
    bool has_key(std::string key)
    { return dictionary_.count(key) > 0; }

    /** \brief Verify that certain keys exist in the configuration and throw an
     * error if they dont
     *
     * \param keys A list of keys to check for
     */

    void verify_keys(std::vector<std::string>& keys)
    {
      for (unsigned i = 0; i < keys.size(); i++)
      {
        if (!has_key(keys[i]))
        {
          std::cout << "Configuration file is missing key " << keys[i] << std::endl;
          exit(-1);
        }
      }
    }

  private:

    std::map<std::string,std::string> dictionary_;

  }; // class Configuration
} // namespace skynet

#endif /* SKYNET_CONFIGURATION_HPP__ */
