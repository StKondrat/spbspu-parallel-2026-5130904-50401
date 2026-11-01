#include "commands.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>
#include <poll.h>
#include <unistd.h>
#include "input.hpp"

kondrat::CommandContext::CommandContext():
  shapes(),
  sets(),
  processes(),
  jobs(processes)
{}

kondrat::CommandTable kondrat::makeCommandTable()
{
  CommandTable table;
  table.emplace("circle", createShape);
  table.emplace("ellipse", createShape);
  table.emplace("show", showShape);
  table.emplace("rotate", rotateShape);
  table.emplace("set", createSet);
  table.emplace("showset", showSet);
  table.emplace("frame", showFrame);
  table.emplace("frameset", showSetFrame);
  table.emplace("spawn", spawn);
  table.emplace("area-on", areaOn);
  table.emplace("status", status);
  table.emplace("wait", wait);
  table.emplace("timeout", timeout);
  table.emplace("kill", kill);

  return table;
}

void kondrat::createShape(const std::vector< std::string > &words, CommandContext &context, std::ostream &)
{
  std::map< std::string, Shape > &shapes = context.shapes;

  constexpr size_t circle_words = 5;
  constexpr size_t ellipse_words = 6;
  constexpr size_t parameters_begin = 2;

  if (words.empty() || (words[0] != "circle" && words[0] != "ellipse"))
  {
    throw std::invalid_argument("Invalid shape command");
  }

  const bool is_circle = words[0] == "circle";
  const size_t expected_words = is_circle ? circle_words : ellipse_words;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::string &name = words[1];

  if (!isValidName(name) || shapes.find(name) != shapes.end())
  {
    throw std::invalid_argument("Invalid or duplicate shape name");
  }

  size_t index = parameters_begin;
  const int horizontal_radius = parseInteger(words[index++]);
  const int vertical_radius = is_circle ? horizontal_radius : parseInteger(words[index++]);
  const int x = parseInteger(words[index++]);
  const int y = parseInteger(words[index]);

  if (horizontal_radius <= 0 || vertical_radius <= 0)
  {
    throw std::invalid_argument("Radii must be positive");
  }

  const Shape shape
      = {is_circle ? ShapeKind::Circle : ShapeKind::Ellipse, horizontal_radius, vertical_radius, x, y, 0.0};

  shapes.emplace(name, shape);
}

void kondrat::showShape(const std::vector< std::string > &words, CommandContext &context, std::ostream &output)
{
  const std::map< std::string, Shape > &shapes = context.shapes;

  constexpr size_t expected_words = 2;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::map< std::string, Shape >::const_iterator position = shapes.find(words[1]);

  if (position == shapes.end())
  {
    throw std::invalid_argument("Unknown shape");
  }

  printShape(position->second, output);
}

void kondrat::rotateShape(const std::vector< std::string > &words, CommandContext &context, std::ostream &)
{
  std::map< std::string, Shape > &shapes = context.shapes;

  constexpr size_t expected_words = 3;
  constexpr size_t angle_index = 2;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::map< std::string, Shape >::iterator position = shapes.find(words[1]);

  if (position == shapes.end())
  {
    throw std::invalid_argument("Unknown shape");
  }

  const double angle = parseReal(words[angle_index]);
  const double new_angle = position->second.angle + angle;

  if (!std::isfinite(new_angle))
  {
    throw std::invalid_argument("Angle is out of range");
  }

  position->second.angle = new_angle;
}

void kondrat::createSet(const std::vector< std::string > &words, CommandContext &context, std::ostream &)
{
  const std::map< std::string, Shape > &shapes = context.shapes;
  std::map< std::string, std::vector< std::string > > &sets = context.sets;

  constexpr size_t names_begin = 3;
  constexpr size_t size_index = 2;

  if (words.size() < names_begin)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::string &name = words[1];

  if (!isValidName(name) || sets.find(name) != sets.end())
  {
    throw std::invalid_argument("Invalid or duplicate set name");
  }

  const int set_size = parseInteger(words[size_index]);

  if (set_size <= 0 || static_cast< size_t >(set_size) != words.size() - names_begin)
  {
    throw std::invalid_argument("Invalid set size");
  }

  std::vector< std::string > names;

  for (size_t i = names_begin; i < words.size(); ++i)
  {
    if (shapes.find(words[i]) == shapes.end())
    {
      throw std::invalid_argument("Unknown shape");
    }

    names.push_back(words[i]);
  }

  sets.emplace(name, names);
}

void kondrat::showSet(const std::vector< std::string > &words, CommandContext &context, std::ostream &output)
{
  const std::map< std::string, Shape > &shapes = context.shapes;
  const std::map< std::string, std::vector< std::string > > &sets = context.sets;

  constexpr size_t expected_words = 2;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::map< std::string, std::vector< std::string > >::const_iterator position = sets.find(words[1]);

  if (position == sets.end())
  {
    throw std::invalid_argument("Unknown set");
  }

  const std::vector< std::string > &names = position->second;

  for (const std::string &name : names)
  {
    printShape(shapes.at(name), output);
  }
}

void kondrat::showFrame(const std::vector< std::string > &words, CommandContext &context, std::ostream &output)
{
  const std::map< std::string, Shape > &shapes = context.shapes;

  constexpr size_t expected_words = 2;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::map< std::string, Shape >::const_iterator position = shapes.find(words[1]);

  if (position == shapes.end())
  {
    throw std::invalid_argument("Unknown shape");
  }

  printFrame(findFrame(position->second), output);
}

void kondrat::showSetFrame(const std::vector< std::string > &words, CommandContext &context, std::ostream &output)
{
  const std::map< std::string, Shape > &shapes = context.shapes;
  const std::map< std::string, std::vector< std::string > > &sets = context.sets;

  constexpr size_t expected_words = 2;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::map< std::string, std::vector< std::string > >::const_iterator position = sets.find(words[1]);

  if (position == sets.end())
  {
    throw std::invalid_argument("Unknown set");
  }

  const std::vector< std::string > &names = position->second;
  Rectangle rectangle = findFrame(shapes.at(names.front()));

  for (const std::string &name : names)
  {
    const Rectangle frame = findFrame(shapes.at(name));

    rectangle.left = std::min(rectangle.left, frame.left);
    rectangle.right = std::max(rectangle.right, frame.right);
    rectangle.bottom = std::min(rectangle.bottom, frame.bottom);
    rectangle.top = std::max(rectangle.top, frame.top);
  }

  printFrame(rectangle, output);
}

kondrat::Jobs::Jobs(ProcessMap &processes):
  processes_(processes),
  jobs_(),
  queues_(),
  failed_processes_(),
  next_id_(1)
{}

void kondrat::Jobs::submit(const std::vector< std::string > &words, const std::map< std::string, Shape > &shapes,
    const std::map< std::string, std::vector< std::string > > &sets)
{
  constexpr size_t expected_words = 6;
  constexpr size_t name_index = 2;
  constexpr size_t set_index = 3;
  constexpr size_t threads_index = 4;
  constexpr size_t tries_index = 5;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::string &process_name = words[1];
  const std::string &name = words[name_index];
  const std::map< std::string, std::vector< std::string > >::const_iterator set = sets.find(words[set_index]);

  if (processes_.find(process_name) == processes_.end() || failed_processes_.count(process_name) != 0)
  {
    throw std::invalid_argument("Unknown or unavailable process");
  }

  if (!isValidName(name) || jobs_.find(name) != jobs_.end() || set == sets.end())
  {
    throw std::invalid_argument("Invalid calculation name or set");
  }

  const long long threads = parseLongLong(words[threads_index]);
  const long long tries = parseLongLong(words[tries_index]);

  if (threads < 0 || tries <= 0 || next_id_ == std::numeric_limits< unsigned long long >::max())
  {
    throw std::invalid_argument("Invalid calculation parameters");
  }

  std::vector< Shape > snapshot;

  for (const std::string &shape_name : set->second)
  {
    snapshot.push_back(shapes.at(shape_name));
  }

  Job job = {next_id_, process_name, threads, tries, std::move(snapshot), false, JobState::InProgress, 0.0, 0.0};
  jobs_.emplace(name, std::move(job));
  queues_[process_name].push_back(name);
  ++next_id_;
  startNext(process_name);
}

void kondrat::Jobs::startNext(const std::string &process_name)
{
  std::deque< std::string > &queue = queues_.at(process_name);

  if (queue.empty() || failed_processes_.count(process_name) != 0)
  {
    return;
  }

  Job &job = jobs_.at(queue.front());

  if (job.started)
  {
    return;
  }

  const Request request = {RequestKind::Area, job.id, job.threads, job.tries, job.shapes.size()};
  const int descriptor = processes_.at(process_name)->requestDescriptor();

  try
  {
    writeFull(descriptor, &request, sizeof(request));
    writeFull(descriptor, job.shapes.data(), job.shapes.size() * sizeof(Shape));
    job.started = true;
    std::vector< Shape >().swap(job.shapes);
  }
  catch (const std::exception &)
  {
    cancelProcess(process_name);
  }
}

void kondrat::Jobs::cancelProcess(const std::string &process_name)
{
  std::deque< std::string > &queue = queues_.at(process_name);

  for (const std::string &name : queue)
  {
    Job &job = jobs_.at(name);
    job.state = JobState::Canceled;
    std::vector< Shape >().swap(job.shapes);
  }

  queue.clear();
  failed_processes_.insert(process_name);
}

void kondrat::Jobs::receive(const std::string &process_name)
{
  std::deque< std::string > &queue = queues_.at(process_name);

  try
  {
    Response response = {};
    const int descriptor = processes_.at(process_name)->resultDescriptor();

    if (!readFull(descriptor, &response, sizeof(response)))
    {
      cancelProcess(process_name);
      return;
    }

    Job &job = jobs_.at(queue.front());

    if (response.calculation_id != job.id)
    {
      throw std::runtime_error("Unexpected calculation response");
    }

    job.time_ms = response.time_ms;
    job.area = response.area;
    job.state = JobState::Done;
    queue.pop_front();
  }
  catch (const std::exception &)
  {
    cancelProcess(process_name);
    return;
  }

  startNext(process_name);
}

void kondrat::Jobs::collectReady()
{
  for (const std::map< std::string, std::deque< std::string > >::value_type &entry : queues_)
  {
    if (entry.second.empty())
    {
      continue;
    }

    struct pollfd descriptor = {};
    descriptor.fd = processes_.at(entry.first)->resultDescriptor();
    descriptor.events = POLLIN;
    int ready = poll(&descriptor, 1, 0);

    while (ready == -1 && errno == EINTR)
    {
      ready = poll(&descriptor, 1, 0);
    }

    if (ready == -1 || (descriptor.revents & (POLLERR | POLLNVAL)) != 0)
    {
      cancelProcess(entry.first);
    }
    else if (ready > 0 && (descriptor.revents & (POLLIN | POLLHUP)) != 0)
    {
      receive(entry.first);
    }
  }
}

void kondrat::Jobs::show(const std::vector< std::string > &words, bool wait, std::ostream &output)
{
  constexpr size_t expected_words = 2;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::map< std::string, Job >::iterator position = jobs_.find(words[1]);

  if (position == jobs_.end())
  {
    throw std::invalid_argument("Unknown calculation");
  }

  collectReady();
  const Job &job = position->second;

  while (wait && job.state == JobState::InProgress)
  {
    receive(job.process_name);
  }

  printResult(job, output);
}

void kondrat::Jobs::printResult(const Job &job, std::ostream &output)
{
  constexpr int result_precision = 3;

  output << job.process_name << ' ';

  if (job.state == JobState::Done)
  {
    output << std::fixed << std::setprecision(result_precision) << job.time_ms << ' ' << job.area << '\n';
  }
  else if (job.state == JobState::Canceled)
  {
    output << "<CANCELED>\n";
  }
  else
  {
    output << "<IN PROGRESS>\n";
  }
}

void kondrat::Jobs::finishAll()
{
  for (const std::map< std::string, std::deque< std::string > >::value_type &entry : queues_)
  {
    while (!entry.second.empty())
    {
      receive(entry.first);
    }
  }
}

std::vector< int > kondrat::Jobs::resultDescriptors() const
{
  std::vector< int > descriptors;

  for (const std::map< std::string, std::deque< std::string > >::value_type &entry : queues_)
  {
    if (!entry.second.empty())
    {
      descriptors.push_back(processes_.at(entry.first)->resultDescriptor());
    }
  }

  return descriptors;
}

void kondrat::Jobs::timeout(const std::vector< std::string > &words, std::ostream &output)
{
  constexpr size_t expected_words = 3;
  constexpr size_t milliseconds_index = 2;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const long long milliseconds = parseLongLong(words[milliseconds_index]);
  const std::map< std::string, Job >::iterator position = jobs_.find(words[1]);

  if (milliseconds < 0 || position == jobs_.end())
  {
    throw std::invalid_argument("Invalid timeout or calculation name");
  }

  const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  collectReady();
  const Job &job = position->second;

  while (job.state == JobState::InProgress)
  {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const long long elapsed = std::chrono::duration_cast< std::chrono::milliseconds >(now - start).count();

    if (elapsed >= milliseconds)
    {
      break;
    }

    const long long remaining = milliseconds - elapsed;
    const long long interval = std::min< long long >(remaining, std::numeric_limits< int >::max());
    struct pollfd descriptor = {};
    descriptor.fd = processes_.at(job.process_name)->resultDescriptor();
    descriptor.events = POLLIN;
    const int ready = poll(&descriptor, 1, static_cast< int >(interval));

    if (ready == -1 && errno == EINTR)
    {
      continue;
    }

    if (ready == -1 || (descriptor.revents & (POLLERR | POLLNVAL)) != 0)
    {
      cancelProcess(job.process_name);
      break;
    }

    if (ready > 0 && (descriptor.revents & (POLLIN | POLLHUP)) != 0)
    {
      receive(job.process_name);
    }
  }

  printResult(job, output);
}

void kondrat::Jobs::stopProcess(const std::vector< std::string > &words)
{
  constexpr size_t expected_words = 2;

  if (words.size() != expected_words)
  {
    throw std::invalid_argument("Invalid parameter count");
  }

  const std::string &process_name = words[1];
  const ProcessMap::iterator position = processes_.find(process_name);

  if (position == processes_.end() || failed_processes_.count(process_name) != 0)
  {
    throw std::invalid_argument("Unknown or unavailable process");
  }

  failed_processes_.insert(process_name);
  std::deque< std::string > &queue = queues_[process_name];

  while (queue.size() > 1)
  {
    Job &job = jobs_.at(queue.back());
    job.state = JobState::Canceled;
    std::vector< Shape >().swap(job.shapes);
    queue.pop_back();
  }

  if (!queue.empty())
  {
    receive(process_name);
  }

  position->second->stop();
}

kondrat::CommandReader::CommandReader():
  pending_(),
  finished_(false)
{}

bool kondrat::CommandReader::readLine(std::string &line, Jobs &jobs)
{
  constexpr size_t buffer_size = 4096;

  while (!finished_ || !pending_.empty())
  {
    jobs.collectReady();
    const size_t newline = pending_.find('\n');

    if (newline != std::string::npos)
    {
      line = pending_.substr(0, newline);
      pending_.erase(0, newline + 1);

      if (!line.empty() && line.back() == '\r')
      {
        line.pop_back();
      }

      return true;
    }

    if (finished_)
    {
      line.swap(pending_);
      pending_.clear();
      return !line.empty();
    }

    std::vector< struct pollfd > descriptors;
    const struct pollfd input = {STDIN_FILENO, POLLIN, 0};
    descriptors.push_back(input);

    for (const int result : jobs.resultDescriptors())
    {
      const struct pollfd descriptor = {result, POLLIN, 0};
      descriptors.push_back(descriptor);
    }

    const int ready = poll(descriptors.data(), static_cast< nfds_t >(descriptors.size()), -1);

    if (ready == -1)
    {
      if (errno == EINTR)
      {
        continue;
      }

      throw std::system_error(errno, std::generic_category(), "Cannot wait for commands");
    }

    if ((descriptors[0].revents & POLLNVAL) != 0)
    {
      throw std::runtime_error("Invalid input descriptor");
    }

    if ((descriptors[0].revents & (POLLIN | POLLHUP | POLLERR)) != 0)
    {
      char buffer[buffer_size] = {};
      const ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));

      if (count == -1)
      {
        if (errno == EINTR)
        {
          continue;
        }

        throw std::system_error(errno, std::generic_category(), "Cannot read commands");
      }

      if (count == 0)
      {
        finished_ = true;
      }
      else
      {
        pending_.append(buffer, static_cast< size_t >(count));
      }
    }
  }

  return false;
}

void kondrat::spawn(const std::vector< std::string > &words, CommandContext &context, std::ostream &)
{
  spawnProcess(words, context.processes);
}

void kondrat::areaOn(const std::vector< std::string > &words, CommandContext &context, std::ostream &)
{
  context.jobs.submit(words, context.shapes, context.sets);
}

void kondrat::status(const std::vector< std::string > &words, CommandContext &context, std::ostream &output)
{
  context.jobs.show(words, false, output);
}

void kondrat::wait(const std::vector< std::string > &words, CommandContext &context, std::ostream &output)
{
  context.jobs.show(words, true, output);
}

void kondrat::timeout(const std::vector< std::string > &words, CommandContext &context, std::ostream &output)
{
  context.jobs.timeout(words, output);
}

void kondrat::kill(const std::vector< std::string > &words, CommandContext &context, std::ostream &)
{
  context.jobs.stopProcess(words);
}
