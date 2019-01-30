#ifndef SKYNET_CONFIGURATION_HPP__
#define SKYNET_CONFIGURATION_HPP__

#include <fstream>
#include <iostream>
#include <map>

namespace skynet
{
  /** \class Configuration
   * \brief Configuration data for the Skynet Instance
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

    std::string get_value(std::string key)
    {
      return dictionary_[key];
    }


  private:

    std::map<std::string,std::string> dictionary_;

  }; // class Configuration
} // namespace skynet

#endif /* SKYNET_CONFIGURATION_HPP__ */
