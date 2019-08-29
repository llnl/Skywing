#ifndef SKYNET_KEYVALUEREADER_HPP__
#define SKYNET_KEYVALUEREADER_HPP__

#include <fstream>
#include <iostream>
#include <unordered_map>

namespace skynet
{
  /** \class Configuration
   * \brief Configuration data for the Skynet instance
   *
   */
  class KeyValueReader
  {
  public:
    /** \brief Construct a new KeyValueReader.
     *
     * The KeyValueReader reads in data when instantiated to disallow further
     * reading of data (which is not currently handled).  Note that key
     * collisions are not allowed.
     *
     * \param filename Specifies configuration filename to be read in
     * \param delimiter Specifies what is to use to delimit keys from values
     */
    KeyValueReader(const std::string& filename, const std::string& delimiter)
    {
      std::ifstream infile(filename);
      if (!infile.is_open())
      {
        std::cout << "Key-value file " << filename << " could not be opened."
          << std::endl;
        exit(-1);
      }
      std::string line;
      while (getline(infile,line))
      {
        const std::string key = line.substr(0, line.find(delimiter));
        if (!key_exists(key))
          dictionary_[key] = line.substr(line.find(delimiter)+1, line.length());
        else
        {
          std::cout << "Key collision on key " << key << " in " << filename << std::endl;
          exit(-1);
        }
      }
      infile.close();
    }

    /** \brief Get value of key from dictionary
     *
     * \param key Specifies name of the key
     * \return The value of the specified key
     */
    const std::string& get_value(const std::string& key)
    {
      if (dictionary_.count(key) > 0)
        return dictionary_[key];
      else
      {
        std::cout << "Key-value dictionary does not contain key " << key << std::endl;
        exit(-1);
      }
    }

    /** \brief Return whether a particular key exists in the dictionary
     *
     * \param key Specifies name of the key
     * \return Whether the key exists in the dictionary
     */
    bool key_exists(const std::string& key)
    { return dictionary_.count(key) > 0; }

    /** \brief Verify that certain keys exist in the dictionary and throw an
     * error if they dont
     *
     * \param keys A list of keys to check for
     */
    void verify_keys(const std::vector<std::string>& keys)
    {
      for (const auto& key : keys)
      {
        if (!key_exists(key))
        {
          std::cout << "Key-value file is missing key " << key << std::endl;
          exit(-1);
        }
      }
    }

  private:

    std::unordered_map<std::string,std::string> dictionary_;

  }; // class KeyValueReader
} // namespace skynet

#endif /* SKYNET_KEYVALUEREADER_HPP__ */
