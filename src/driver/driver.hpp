#ifndef DRIVER_H
#define DRIVER_H

#include "../cli/cli.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

/*!
 * specifies where the driver should look for the configuration file.
 */
enum class InputMethod {
  ConfigFile,
  Stdin,
};

/*!
 * contains options required by the driver to run.
 */
struct Setup {
  std::optional<std::string> task;
  InputMethod input_method;
  std::string input_file; // only used for InputMethod::ConfigFile.
  LogLevel logging_level;
  bool dry_run;
};

/*!
 * interface for running qvickbuild.
 */
class Driver {
private:
  Setup setup;

  void unwind_errors(std::vector<unsigned char> config);
  std::vector<unsigned char> get_config();

public:
  /*!
   * constructs driver from setup options.
   */
  Driver(Setup);

  /*!
   * \return default options for running the driver.
   */
  static Setup default_setup();

  /*!
   * runs the driver.
   * \return EXIT_FAILURE on failure, EXIT_SUCCESS on success.
   */
  int run();
};

#endif
