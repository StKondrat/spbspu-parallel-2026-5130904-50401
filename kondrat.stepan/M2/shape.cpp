#include "shape.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <random>
#include <ostream>
#include <stdexcept>

void kondrat::printShape(const Shape & shape, std::ostream & output)
{
  constexpr int angle_precision = 3;

  output << shape.horizontal_radius;

  if (shape.kind == ShapeKind::Ellipse)
  {
    output << ' ' << shape.vertical_radius;
  }

  output << " (" << shape.x << ' ' << shape.y << ')';

  if (shape.kind == ShapeKind::Ellipse)
  {
    output << ' ' << std::fixed << std::setprecision(angle_precision) << shape.angle;
  }

  output << '\n';
}

kondrat::Rectangle kondrat::findFrame(const Shape & shape)
{
  const double a = shape.horizontal_radius;
  const double b = shape.vertical_radius;
  double half_width = a;
  double half_height = a;

  if (shape.kind == ShapeKind::Ellipse)
  {
    const double cosine = std::cos(shape.angle);
    const double sine = std::sin(shape.angle);

    half_width = std::hypot(a * cosine, b * sine);
    half_height = std::hypot(a * sine, b * cosine);
  }

  return {shape.x - half_width, shape.x + half_width, shape.y - half_height, shape.y + half_height};
}

void kondrat::printFrame(const Rectangle & rectangle, std::ostream & output)
{
  output << std::defaultfloat << std::setprecision(std::numeric_limits< double >::max_digits10);
  output << '(' << rectangle.left << ' ' << rectangle.bottom << ") (" << rectangle.right << ' ' << rectangle.top
         << ")\n";
}

namespace
{
  struct PreparedShape
  {
    double x;
    double y;
    double xx;
    double xy;
    double yy;
  };

  static PreparedShape prepareShape(const kondrat::Shape & shape)
  {
    const double a = shape.horizontal_radius;
    const double b = shape.vertical_radius;
    const double inverse_a = 1.0 / (a * a);
    const double inverse_b = 1.0 / (b * b);

    if (shape.kind == kondrat::ShapeKind::Circle)
    {
      return {static_cast< double >(shape.x), static_cast< double >(shape.y), inverse_a, 0.0, inverse_a};
    }

    const double cosine = std::cos(shape.angle);
    const double sine = std::sin(shape.angle);
    constexpr double cross_factor = 2.0;

    return {static_cast< double >(shape.x), static_cast< double >(shape.y),
        cosine * cosine * inverse_a + sine * sine * inverse_b, cross_factor * cosine * sine * (inverse_a - inverse_b),
        sine * sine * inverse_a + cosine * cosine * inverse_b};
  }

  static bool contains(const PreparedShape & shape, double x, double y)
  {
    const double dx = x - shape.x;
    const double dy = y - shape.y;

    return shape.xx * dx * dx + shape.xy * dx * dy + shape.yy * dy * dy <= 1.0;
  }

  static long long countHits(const std::vector< PreparedShape > & shapes, const kondrat::Rectangle & rectangle,
      long long tries, std::default_random_engine::result_type seed)
  {
    std::default_random_engine generator(seed);
    const std::default_random_engine::result_type minimum = generator.min();
    const double range = static_cast< double >(generator.max()) - minimum + 1.0;
    const double x_scale = (rectangle.right - rectangle.left) / range;
    const double y_scale = (rectangle.top - rectangle.bottom) / range;
    long long hits = 0;

    if (shapes.size() == 1)
    {
      const PreparedShape & shape = shapes.front();

      for (long long i = 0; i < tries; ++i)
      {
        const double x = rectangle.left + (generator() - minimum) * x_scale;
        const double y = rectangle.bottom + (generator() - minimum) * y_scale;

        if (contains(shape, x, y))
        {
          ++hits;
        }
      }

      return hits;
    }

    for (long long i = 0; i < tries; ++i)
    {
      const double x = rectangle.left + (generator() - minimum) * x_scale;
      const double y = rectangle.bottom + (generator() - minimum) * y_scale;

      for (const PreparedShape & shape : shapes)
      {
        if (contains(shape, x, y))
        {
          ++hits;
          break;
        }
      }
    }

    return hits;
  }
}

double kondrat::calculateArea(const std::vector< Shape > & shapes, long long threads, long long tries, long long seed)
{
  if (shapes.empty() || threads < 0 || tries <= 0)
  {
    throw std::invalid_argument("Invalid calculation parameters");
  }

  std::vector< PreparedShape > prepared;
  prepared.reserve(shapes.size());
  Rectangle rectangle = findFrame(shapes.front());

  for (const Shape & shape : shapes)
  {
    const Rectangle frame = findFrame(shape);
    prepared.push_back(prepareShape(shape));

    rectangle.left = std::min(rectangle.left, frame.left);
    rectangle.right = std::max(rectangle.right, frame.right);
    rectangle.bottom = std::min(rectangle.bottom, frame.bottom);
    rectangle.top = std::max(rectangle.top, frame.top);
  }

  const long long task_count = threads == 0 ? 1 : threads;
  const long long base_tries = tries / task_count;
  const long long remainder = tries % task_count;
  std::vector< std::future< long long > > futures;

  for (long long i = 0; i < task_count; ++i)
  {
    const long long task_tries = base_tries + (i < remainder ? 1 : 0);
    const std::default_random_engine::result_type task_seed
        = static_cast< std::default_random_engine::result_type >(seed)
        + static_cast< std::default_random_engine::result_type >(i);

    futures.push_back(
        std::async(std::launch::async, countHits, std::cref(prepared), std::cref(rectangle), task_tries, task_seed));
  }

  long long hits = 0;

  for (std::future< long long > & future : futures)
  {
    hits += future.get();
  }

  const double rectangle_area = (rectangle.right - rectangle.left) * (rectangle.top - rectangle.bottom);

  return rectangle_area * (static_cast< double >(hits) / tries);
}
