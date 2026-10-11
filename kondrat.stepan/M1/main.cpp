#include <iostream>
#include <vector>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <future>
#include <iomanip>
#include <exception>
#include <limits>
#include <functional>
#include <random>

namespace kondrat
{
  struct Ellipse
  {
    int horizontal_radius;
    int vertical_radius;
    int x;
    int y;
  };

  struct Rectangle
  {
    double left;
    double right;
    double top;
    double bottom;
  };

  struct HitCounts
  {
    long long union_hits;
    long long intersection_hits;
  };

  long long parseArgument(const char * argument);
  Rectangle findRectangle(const std::vector< Ellipse > & ellipses);
  bool contains(const Ellipse & ellipse, double x, double y);
  HitCounts countHits(
      long long tries, long long seed, const std::vector< Ellipse > & ellipses, const Rectangle & rectangle);
  HitCounts runTask(long long task_count, long long tries, long long seed, const std::vector< Ellipse > & ellipses,
      const Rectangle & rectangle);
}

int main(int argc, char ** argv)
{
  constexpr int min_args = 3;
  constexpr int max_args = 4;
  constexpr int threads_index = 1;
  constexpr int tries_index = 2;
  constexpr int seed_index = 3;
  constexpr int internal_error = 2;

  if (argc != min_args && argc != max_args)
  {
    std::cerr << "Usage: ./lab threads tries [seed]\n";
    return 1;
  }

  long long threads = 0;
  long long tries = 0;
  long long seed = 0;

  try
  {
    threads = kondrat::parseArgument(argv[threads_index]);
    tries = kondrat::parseArgument(argv[tries_index]);

    if (argc == max_args)
    {
      seed = kondrat::parseArgument(argv[seed_index]);
    }
  }
  catch (const std::invalid_argument & error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
  catch (const std::out_of_range & error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }

  if (threads < 0 || tries <= 0 || seed < 0)
  {
    std::cerr << "Threads and seed must be nonnegative, tries must be positive\n";
    return 1;
  }

  try
  {
    std::vector< kondrat::Ellipse > ellipses;
    int a = 0;
    int b = 0;
    int cx = 0;
    int cy = 0;

    while (std::cin >> a >> b >> cx >> cy)
    {
      const int vertical_radius = b == 0 ? a : b;
      ellipses.push_back({a, vertical_radius, cx, cy});
    }

    if (std::cin.bad() || !std::cin.eof())
    {
      std::cerr << "Failed to parse figure\n";
      return 1;
    }

    if (ellipses.empty())
    {
      std::cout << "0 0\n";
      return 0;
    }

    const kondrat::Rectangle rectangle = kondrat::findRectangle(ellipses);

    const long long task_count = threads == 0 ? 1 : threads;

    const kondrat::HitCounts counts = kondrat::runTask(task_count, tries, seed, ellipses, rectangle);

    const double width = rectangle.right - rectangle.left;
    const double height = rectangle.top - rectangle.bottom;
    const double rectangle_area = width * height;

    const double union_area = rectangle_area * (static_cast< double >(counts.union_hits) / tries);
    const double intersection_area = rectangle_area * (static_cast< double >(counts.intersection_hits) / tries);

    std::cout << std::setprecision(std::numeric_limits< double >::max_digits10);
    std::cout << union_area << ' ' << intersection_area << '\n';
  }
  catch (const std::exception & error)
  {
    std::cerr << error.what() << '\n';
    return internal_error;
  }

  return 0;
}

long long kondrat::parseArgument(const char * argument)
{
  size_t position = 0;
  const long long value = std::stoll(argument, &position);

  if (argument[position] != '\0')
  {
    throw std::invalid_argument("Invalid argument");
  }

  return value;
}

kondrat::Rectangle kondrat::findRectangle(const std::vector< Ellipse > & ellipses)
{
  const Ellipse first = ellipses.front();
  const double first_x = first.x;
  const double first_y = first.y;

  Rectangle rectangle = {first_x - first.horizontal_radius, first_x + first.horizontal_radius,
      first_y + first.vertical_radius, first_y - first.vertical_radius};

  for (const Ellipse & ellipse : ellipses)
  {
    const double x = ellipse.x;
    const double y = ellipse.y;

    rectangle.left = std::min(rectangle.left, x - ellipse.horizontal_radius);
    rectangle.right = std::max(rectangle.right, x + ellipse.horizontal_radius);
    rectangle.top = std::max(rectangle.top, y + ellipse.vertical_radius);
    rectangle.bottom = std::min(rectangle.bottom, y - ellipse.vertical_radius);
  }

  return rectangle;
}

bool kondrat::contains(const Ellipse & ellipse, double x, double y)
{
  const double dx = (x - ellipse.x) / ellipse.horizontal_radius;
  const double dy = (y - ellipse.y) / ellipse.vertical_radius;

  return dx * dx + dy * dy <= 1.0;
}

kondrat::HitCounts kondrat::countHits(
    long long tries, long long seed, const std::vector< Ellipse > & ellipses, const Rectangle & rectangle)
{
  std::default_random_engine generator(seed);
  std::uniform_real_distribution< double > x_distribution(rectangle.left, rectangle.right);
  std::uniform_real_distribution< double > y_distribution(rectangle.bottom, rectangle.top);

  HitCounts counts = {0, 0};

  for (long long i = 0; i < tries; ++i)
  {
    const double x = x_distribution(generator);
    const double y = y_distribution(generator);
    bool inside_any = false;
    bool inside_all = true;

    for (const Ellipse & ellipse : ellipses)
    {
      const bool inside = contains(ellipse, x, y);
      inside_any = inside_any || inside;
      inside_all = inside_all && inside;
    }

    if (inside_any)
    {
      ++counts.union_hits;
    }

    if (inside_all)
    {
      ++counts.intersection_hits;
    }
  }

  return counts;
}

kondrat::HitCounts kondrat::runTask(long long task_count, long long tries, long long seed,
    const std::vector< Ellipse > & ellipses, const Rectangle & rectangle)
{
  const long long base_tries = tries / task_count;
  const long long remainder = tries % task_count;
  std::vector< std::future< HitCounts > > futures;

  for (long long i = 0; i < task_count; ++i)
  {
    const long long task_tries = base_tries + (i < remainder ? 1 : 0);
    const long long task_seed = seed + i;

    futures.push_back(
        std::async(std::launch::async, countHits, task_tries, task_seed, std::cref(ellipses), std::cref(rectangle)));
  }

  HitCounts total = {0, 0};

  for (std::future< HitCounts > & future : futures)
  {
    const HitCounts counts = future.get();
    total.union_hits += counts.union_hits;
    total.intersection_hits += counts.intersection_hits;
  }

  return total;
}
