// contour.h
#pragma once
#include <span>
#include <vector>

struct Line {
  float x1, y1, x2, y2;
  float elevation;
};

void extract_contours(std::span<const float> heightmap, int width, int height,
                      float interval, std::vector<Line> &out_lines);
