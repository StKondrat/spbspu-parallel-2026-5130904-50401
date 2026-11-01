#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <deque>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "process.hpp"
#include "shape.hpp"

namespace kondrat
{
  enum class JobState
  {
    InProgress,
    Done,
    Canceled
  };

  struct Job
  {
    unsigned long long id;
    std::string process_name;
    long long threads;
    long long tries;
    std::vector< Shape > shapes;
    bool started;
    JobState state;
    double time_ms;
    double area;
  };

  class Jobs
  {
  public:
    explicit Jobs(ProcessMap &processes);

    void submit(const std::vector< std::string > &words, const std::map< std::string, Shape > &shapes,
        const std::map< std::string, std::vector< std::string > > &sets);
    void show(const std::vector< std::string > &words, bool wait, std::ostream &output);
    void timeout(const std::vector< std::string > &words, std::ostream &output);
    void stopProcess(const std::vector< std::string > &words);
    void collectReady();
    void finishAll();
    std::vector< int > resultDescriptors() const;

  private:
    ProcessMap &processes_;
    std::map< std::string, Job > jobs_;
    std::map< std::string, std::deque< std::string > > queues_;
    std::set< std::string > failed_processes_;
    unsigned long long next_id_;

    static void printResult(const Job &job, std::ostream &output);
    void startNext(const std::string &process_name);
    void receive(const std::string &process_name);
    void cancelProcess(const std::string &process_name);
  };

  class CommandReader
  {
  public:
    CommandReader();

    bool readLine(std::string &line, Jobs &jobs);

  private:
    std::string pending_;
    bool finished_;
  };

  struct CommandContext
  {
    std::map< std::string, Shape > shapes;
    std::map< std::string, std::vector< std::string > > sets;
    ProcessMap processes;
    Jobs jobs;

    CommandContext();
    CommandContext(const CommandContext &) = delete;
    CommandContext &operator=(const CommandContext &) = delete;
  };

  using CommandHandler = void (*)(const std::vector< std::string > &, CommandContext &, std::ostream &);
  using CommandTable = std::map< std::string, CommandHandler >;

  CommandTable makeCommandTable();

  void createShape(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void showShape(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void rotateShape(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void createSet(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void showSet(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void showFrame(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void showSetFrame(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void spawn(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void areaOn(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void status(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void wait(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void timeout(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
  void kill(const std::vector< std::string > &words, CommandContext &context, std::ostream &output);
}

#endif
