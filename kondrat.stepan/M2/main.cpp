#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <signal.h>
#include "commands.hpp"
#include "input.hpp"

int main()
{
  struct sigaction action = {};
  action.sa_handler = SIG_IGN;

  if (sigemptyset(&action.sa_mask) == -1 || sigaction(SIGPIPE, &action, nullptr) == -1)
  {
    std::cerr << "Cannot configure SIGPIPE\n";
    return 0;
  }

  kondrat::CommandContext context;
  kondrat::CommandReader reader;
  const kondrat::CommandTable commands = kondrat::makeCommandTable();
  std::string line;

  try
  {
    while (reader.readLine(line, context.jobs))
    {
      const std::vector< std::string > words = kondrat::splitCommand(line);

      if (words.empty())
      {
        continue;
      }

      const kondrat::CommandTable::const_iterator command = commands.find(words[0]);

      if (command == commands.end())
      {
        std::cout << "<UNKNOWN COMMAND>\n";
      }
      else
      {
        try
        {
          command->second(words, context, std::cout);
        }
        catch (const std::invalid_argument &)
        {
          std::cout << "<INVALID COMMAND>\n";
        }
        catch (const std::out_of_range &)
        {
          std::cout << "<INVALID COMMAND>\n";
        }
        catch (const std::system_error &)
        {
          std::cout << "<INVALID COMMAND>\n";
        }
      }

      std::cout.flush();
    }

    context.jobs.finishAll();
  }
  catch (const std::exception &error)
  {
    std::cerr << error.what() << '\n';
  }

  return 0;
}
