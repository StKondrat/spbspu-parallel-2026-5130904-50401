#include "process.hpp"
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <exception>
#include <ratio>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "input.hpp"
#include "shape.hpp"

kondrat::Process::Process(long long seed, const std::vector< int > &inherited_descriptors):
  pid_(-1),
  request_descriptor_(-1),
  result_descriptor_(-1)
{
  int requests[2] = {-1, -1};
  int results[2] = {-1, -1};

  if (pipe(requests) == -1)
  {
    throw std::system_error(errno, std::generic_category(), "Cannot create request pipe");
  }

  if (pipe(results) == -1)
  {
    const int error = errno;

    close(requests[0]);
    close(requests[1]);
    throw std::system_error(error, std::generic_category(), "Cannot create result pipe");
  }

  pid_ = fork();

  if (pid_ == -1)
  {
    const int error = errno;

    close(requests[0]);
    close(requests[1]);
    close(results[0]);
    close(results[1]);
    throw std::system_error(error, std::generic_category(), "Cannot create process");
  }

  if (pid_ == 0)
  {
    close(requests[1]);
    close(results[0]);

    for (const int descriptor : inherited_descriptors)
    {
      close(descriptor);
    }

    const int exit_code = workerLoop(requests[0], results[1], seed);
    _exit(exit_code);
  }

  close(requests[0]);
  close(results[1]);
  request_descriptor_ = requests[1];
  result_descriptor_ = results[0];
}

kondrat::Process::~Process()
{
  stop();
}

void kondrat::Process::stop()
{
  if (request_descriptor_ != -1)
  {
    close(request_descriptor_);
    request_descriptor_ = -1;
  }

  if (result_descriptor_ != -1)
  {
    close(result_descriptor_);
    result_descriptor_ = -1;
  }

  if (pid_ != -1)
  {
    while (waitpid(pid_, nullptr, 0) == -1 && errno == EINTR)
    {
    }

    pid_ = -1;
  }
}

int kondrat::Process::requestDescriptor() const
{
  return request_descriptor_;
}

int kondrat::Process::resultDescriptor() const
{
  return result_descriptor_;
}

void kondrat::spawnProcess(const std::vector< std::string > &words, ProcessMap &processes)
{
  constexpr size_t expected_words = 3;
  constexpr size_t seed_index = 2;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::string &name = words[1];

  if (!isValidName(name) || processes.find(name) != processes.end())
  {
    throw std::invalid_argument("Invalid or duplicate process name");
  }

  const long long seed = parseLongLong(words[seed_index]);

  if (seed < 0)
  {
    throw std::invalid_argument("Seed cannot be negative");
  }

  std::vector< int > inherited_descriptors;

  for (const ProcessMap::value_type &entry : processes)
  {
    const int request_descriptor = entry.second->requestDescriptor();
    const int result_descriptor = entry.second->resultDescriptor();

    if (request_descriptor != -1)
    {
      inherited_descriptors.push_back(request_descriptor);
    }

    if (result_descriptor != -1)
    {
      inherited_descriptors.push_back(result_descriptor);
    }
  }

  std::unique_ptr< Process > process(new Process(seed, inherited_descriptors));
  processes.emplace(name, std::move(process));
}

bool kondrat::readFull(int descriptor, void *buffer, size_t size)
{
  char *const data = static_cast< char * >(buffer);
  size_t received = 0;

  while (received < size)
  {
    const ssize_t count = read(descriptor, data + received, size - received);

    if (count == -1)
    {
      if (errno == EINTR)
      {
        continue;
      }

      throw std::system_error(errno, std::generic_category(), "Cannot read pipe");
    }

    if (count == 0)
    {
      if (received != 0)
      {
        throw std::runtime_error("Incomplete pipe message");
      }

      return false;
    }

    received += static_cast< size_t >(count);
  }

  return true;
}

void kondrat::writeFull(int descriptor, const void *buffer, size_t size)
{
  const char *const data = static_cast< const char * >(buffer);
  size_t sent = 0;

  while (sent < size)
  {
    const ssize_t count = write(descriptor, data + sent, size - sent);

    if (count == -1)
    {
      if (errno == EINTR)
      {
        continue;
      }

      throw std::system_error(errno, std::generic_category(), "Cannot write pipe");
    }

    if (count == 0)
    {
      throw std::runtime_error("Cannot finish pipe write");
    }

    sent += static_cast< size_t >(count);
  }
}

int kondrat::workerLoop(int request_descriptor, int result_descriptor, long long seed)
{
  int exit_code = 0;

  try
  {
    struct sigaction action = {};
    action.sa_handler = SIG_IGN;

    if (sigemptyset(&action.sa_mask) == -1 || sigaction(SIGPIPE, &action, nullptr) == -1)
    {
      throw std::system_error(errno, std::generic_category(), "Cannot configure SIGPIPE");
    }

    Request request = {};

    while (readFull(request_descriptor, &request, sizeof(request)))
    {
      if (request.kind == RequestKind::Stop)
      {
        break;
      }

      if (request.kind != RequestKind::Area || request.shape_count == 0)
      {
        throw std::invalid_argument("Invalid calculation request");
      }

      std::vector< Shape > shapes(request.shape_count);

      if (!readFull(request_descriptor, shapes.data(), shapes.size() * sizeof(Shape)))
      {
        throw std::runtime_error("Missing request shapes");
      }

      const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
      const double area = calculateArea(shapes, request.threads, request.tries, seed);
      const std::chrono::steady_clock::time_point finish = std::chrono::steady_clock::now();
      const double time_ms = std::chrono::duration< double, std::milli >(finish - start).count();
      const Response response = {request.calculation_id, time_ms, area};

      writeFull(result_descriptor, &response, sizeof(response));
    }
  }
  catch (const std::exception &)
  {
    exit_code = 1;
  }

  close(request_descriptor);
  close(result_descriptor);

  return exit_code;
}
