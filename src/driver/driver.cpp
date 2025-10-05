#include "driver.hpp"
#include "../cli/cli.hpp"
#include "../cli/colour.hpp"
#include "../cli/environment.hpp"
#include "../errors/errors.hpp"
#include "../interpreter/interpreter.hpp"
#include "../kal/platform.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../system/pipeline.hpp"

#include <cassert>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>

Driver::Driver(Setup setup) {
  this->state = std::make_unique<DriverState>(DriverState{setup});
}
Setup Driver::default_setup() {
  return Setup{std::nullopt, InputMethod::ConfigFile, "./qvickbuild",
               LogLevel::Standard, false};
}

std::vector<unsigned char> Driver::get_config() {
  switch (this->state->setup.input_method) {
  case InputMethod::ConfigFile: {
    if (!std::filesystem::is_regular_file(this->state->setup.input_file))
      ErrorHandler::halt(EInvalidInputFile{this->state->setup.input_file});
    std::ifstream config_file(this->state->setup.input_file, std::ios::binary);
    if (!config_file.is_open())
      ErrorHandler::halt(EInvalidInputFile{this->state->setup.input_file});
    return std::vector<unsigned char>(std::istreambuf_iterator(config_file),
                                      {});
  }
  case InputMethod::Stdin: {
    std::string line;
    std::string all;
    while (getline(std::cin, line))
      all += line;
    return std::vector<unsigned char>(all.begin(), all.end());
  }
  }
  // code execution will never get here (let's hope)
  // keeps the compiler quiet
  assert(false && "driver encountered an unrecognized input method");
  __builtin_unreachable();
}

void Driver::unwind_errors(std::vector<unsigned char> config) {
  bool verbose_threads = ErrorHandler::get_errors().size() > 1;
  std::unordered_map<size_t, std::vector<std::shared_ptr<Frame>>> frames =
      ContextStack::dump_stack();
  for (auto [thread_hash, build_error] : ErrorHandler::get_errors()) {
    // display error.
    std::string rendered_error = ErrorRenderer::prefix_rendered_view(
        build_error->render_error(config),
        std::format("{}│{} ", CLIColour::red(), CLIColour::reset()));
    std::string thread_prefix =
        verbose_threads
            ? std::format("{}{}«thread {:x}»{} ", CLIColour::red(),
                          CLIColour::bold(), thread_hash, CLIColour::reset())
            : "";
    CLI::write_to_suffix(std::format("{}{}", thread_prefix, rendered_error));
    if (!frames[thread_hash].empty())
      CLI::write_to_suffix(CLIColour::red() + "│" + CLIColour::reset() + "");
    // display context stack.
    for (std::shared_ptr<Frame> const &frame : frames[thread_hash]) {
      CLI::write_to_suffix(std::format(
          "{}│{}  {}note:{} while {}", CLIColour::red(), CLIColour::reset(),
          CLIColour::grey(), CLIColour::reset(), frame->render_frame(config)));
    }
    CLI::write_to_suffix(
        std::format("{}╰ end.{}", CLIColour::red(), CLIColour::reset()));
  }
}

std::string get_version_string() {
  KALPlatformType platform = KALPlatform::current();
  switch (platform) {
  case KALPlatformType::Linux:
    return "v0.9.0/kal-linux";
  case KALPlatformType::Windows:
    return "v0.9.0/kal-windows";
  case KALPlatformType::Apple:
    return "v0.9.0/kal-apple";
  default:
    assert(false && "driver encountered an unrecognized kal platform");
  }
}

int Driver::run() {
  // LOG_STANDARD(CLIColour::bold() << "warning: you are running qvickbuild beta
  // "
  //                   << get_version_string() << CLIColour::reset());

  // initialize required subsystems.
  CLICapabilities capabilities = CLIEnvironment::detect_cli_capabilities();
  LogLevel log_level = this->state->setup.logging_level;
  CLIOptions cli_options{log_level, capabilities};
  CLI::initialize(cli_options);
  Pipeline::initialize(std::thread::hardware_concurrency());

  // config needs to be initialized out of scope so that
  // it can be read when unwinding the error stack.
  std::vector<unsigned char> config;

  try {
    // we still need to read this within the try-catch because
    // the file may not exist, but we still need to render the error
    config = get_config();

    // build script.
    Lexer lexer(config);
    std::vector<Token> token_stream;
    token_stream = lexer.get_token_stream();

    Parser parser = Parser(token_stream);
    AST ast(parser.parse_tokens());

    // build task.
    Interpreter interpreter(ast, this->state->setup);
    interpreter.build();

  } catch (BuildException &_) {
    unwind_errors(config);

    Pipeline::stop_sync();
    CLI::stop_sync();
    return EXIT_FAILURE;
  }

  // shut down required subsystems.
  CLI::stop_sync();
  Pipeline::stop_sync();

  return EXIT_SUCCESS;
}
