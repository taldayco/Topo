// contour.cpp
#include "contour.h"

// Extract contours for a SINGLE elevation level
static void extract_level(std::span<const float> heightmap, int width,
                          int height, float level,
                          std::vector<Line> &out_lines) {

  for (int idx = 0; idx < (width - 1) * (height - 1); ++idx) {
    int x = idx % (width - 1);
    int y = idx / (width - 1);

    float h00 = heightmap[y * width + x];
    float h10 = heightmap[y * width + x + 1];
    float h01 = heightmap[(y + 1) * width + x];
    float h11 = heightmap[(y + 1) * width + x + 1];

    // Marching squares configuration
    int config = ((h00 >= level) << 0) | ((h10 >= level) << 1) |
                 ((h11 >= level) << 2) | ((h01 >= level) << 3);

    if (config == 0 || config == 15)
      continue; // No crossing

    float fx = (float)x, fy = (float)y;
    float points[4][2]; // Max 4 edge intersections
    int point_count = 0;

    // Top edge
    if ((config & 1) != (config & 2)) {
      float t = (level - h00) / (h10 - h00);
      points[point_count][0] = fx + t;
      points[point_count][1] = fy;
      point_count++;
    }
    // Right edge
    if ((config & 2) != (config & 4)) {
      float t = (level - h10) / (h11 - h10);
      points[point_count][0] = fx + 1;
      points[point_count][1] = fy + t;
      point_count++;
    }
    // Bottom edge
    if ((config & 4) != (config & 8)) {
      float t = (level - h11) / (h01 - h11);
      points[point_count][0] = fx + 1 - t;
      points[point_count][1] = fy + 1;
      point_count++;
    }
    // Left edge
    if ((config & 8) != (config & 1)) {
      float t = (level - h01) / (h00 - h01);
      points[point_count][0] = fx;
      points[point_count][1] = fy + 1 - t;
      point_count++;
    }

    // Connect points (saddle case: config 5 and 10 have 4 points)
    if (point_count == 2) {
      out_lines.push_back(
          {points[0][0], points[0][1], points[1][0], points[1][1]});
    } else if (point_count == 4) {
      out_lines.push_back(
          {points[0][0], points[0][1], points[1][0], points[1][1]});
      out_lines.push_back(
          {points[2][0], points[2][1], points[3][0], points[3][1]});
    }
  }
}

void extract_contours(std::span<const float> heightmap, int width, int height,
                      float interval, std::vector<Line> &out_lines) {
  out_lines.clear();

  // Extract each level separately - call single-pass function multiple times
  for (float level = interval; level < 1.0f; level += interval) {
    extract_level(heightmap, width, height, level, out_lines);
  }
}
