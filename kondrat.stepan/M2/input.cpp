#include "input.hpp"
#include <cstddef>
#include <cmath>
#include <stdexcept>

std::vector< std::string > kondrat::splitCommand(const std::string & line)
{
  std::vector< std::string > words;
  size_t i = 0;

  while (i < line.size())
  {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
    {
      ++i;
    }

    const size_t start = i;

    while (i < line.size() && line[i] != ' ' && line[i] != '\t')
    {
      ++i;
    }

    if (i > start)
    {
      words.emplace_back(line, start, i - start);
    }
  }

  return words;
}

int kondrat::parseInteger(const std::string & word)
{
  size_t position = 0;
  const int value = std::stoi(word, &position);

  if (position != word.size())
  {
    throw std::invalid_argument("Invalid integer");
  }

  return value;
}

bool kondrat::isValidName(const std::string & name)
{
  if (name.empty())
  {
    return false;
  }

  const char first = name.front();

  return (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z');
}

double kondrat::parseReal(const std::string & word)
{
  size_t position = 0;
  const double value = std::stod(word, &position);

  if (position != word.size() || !std::isfinite(value))
  {
    throw std::invalid_argument("Invalid real number");
  }

  return value;
}

long long kondrat::parseLongLong(const std::string & word)
{
  size_t position = 0;
  const long long value = std::stoll(word, &position);

  if (position != word.size())
  {
    throw std::invalid_argument("Invalid integer");
  }

  return value;
}
