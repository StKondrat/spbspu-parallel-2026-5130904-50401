#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <sys/types.h>

namespace kondrat
{
  enum class RequestKind
  {
    Area,
    Stop
  };

  struct Request
  {
    RequestKind kind;
    unsigned long long calculation_id;
    long long threads;
    long long tries;
    size_t shape_count;
  };

  struct Response
  {
    unsigned long long calculation_id;
    double time_ms;
    double area;
  };

  bool readFull(int descriptor, void *buffer, size_t size);
  void writeFull(int descriptor, const void *buffer, size_t size);

  int workerLoop(int request_descriptor, int result_descriptor, long long seed);

  class Process
  {
  public:
    Process(long long seed, const std::vector< int > &inherited_descriptors);
    Process(const Process &) = delete;
    Process &operator=(const Process &) = delete;
    ~Process();

    void stop();
    int requestDescriptor() const;
    int resultDescriptor() const;

  private:
    pid_t pid_;
    int request_descriptor_;
    int result_descriptor_;
  };

  using ProcessMap = std::map< std::string, std::unique_ptr< Process > >;

  void spawnProcess(const std::vector< std::string > &words, ProcessMap &processes);
}

#endif
