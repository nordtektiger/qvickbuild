#ifndef DRIVER_H
#define DRIVER_H

#include <optional>
#include <string>
#include <vector>
#include <memory>

enum class InputMethod {
  ConfigFile,
  Stdin,
};

enum class LoggingLevel {
  Quiet,
  Standard,
  Verbose,
};

struct Setup {
  std::optional<std::string> task;
  InputMethod input_method;
  std::string input_file; // only used for InputMethod::ConfigFile
  LoggingLevel logging_level;
  bool dry_run;
};

// temporary compatability patch for cli formatting.
struct DriverState {
  Setup setup;
};

class Driver {
private:
  std::unique_ptr<DriverState> state;
  void unwind_errors(std::vector<unsigned char> config);
  std::vector<unsigned char> get_config();

public:
  Driver(Setup);
  static Setup default_setup();
  int run();
};

#endif
