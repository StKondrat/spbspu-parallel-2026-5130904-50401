#ifndef SHAPE_HPP
#define SHAPE_HPP

#include <iosfwd>
#include <vector>

namespace kondrat
{
  enum class ShapeKind
  {
    Circle,
    Ellipse
  };

  struct Shape
  {
    ShapeKind kind;
    int horizontal_radius;
    int vertical_radius;
    int x;
    int y;
    double angle;
  };

  struct Rectangle
  {
    double left;
    double right;
    double bottom;
    double top;
  };

  Rectangle findFrame(const Shape & shape);
  void printFrame(const Rectangle & rectangle, std::ostream & output);
  void printShape(const Shape & shape, std::ostream & output);
  double calculateArea(const std::vector< Shape > & shapes, long long threads, long long tries, long long seed);
}

#endif
