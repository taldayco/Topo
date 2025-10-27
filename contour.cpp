#include "contour.h"

void extract_contours(std::span<const float> heightmap, int width, int height,
                      float interval, std::vector<Line> &out_lines) {
  out_lines.clear();

  // Extract each elevation level
  for (float level = interval * 0.5f; level < 1.0f; level += interval) {
    // Marching squares with linear interpolation
    for (int y = 0; y < height - 1; ++y) {
      for (int x = 0; x < width - 1; ++x) {
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
        float points[4][2]; // Up to 4 edge crossings
        int point_count = 0;

        // Top edge (h00 to h10)
        if ((h00 < level && h10 >= level) || (h00 >= level && h10 < level)) {
          float t = (level - h00) / (h10 - h00);
          points[point_count][0] = fx + t;
          points[point_count][1] = fy;
          point_count++;
        }

        // Right edge (h10 to h11)
        if ((h10 < level && h11 >= level) || (h10 >= level && h11 < level)) {
          float t = (level - h10) / (h11 - h10);
          points[point_count][0] = fx + 1;
          points[point_count][1] = fy + t;
          point_count++;
        }

        // Bottom edge (h11 to h01)
        if ((h11 < level && h01 >= level) || (h11 >= level && h01 < level)) {
          float t = (level - h11) / (h01 - h11);
          points[point_count][0] = fx + 1 - t;
          points[point_count][1] = fy + 1;
          point_count++;
        }

        // Left edge (h01 to h00)
        if ((h01 < level && h00 >= level) || (h01 >= level && h00 < level)) {
          float t = (level - h01) / (h00 - h01);
          points[point_count][0] = fx;
          points[point_count][1] = fy + 1 - t;
          point_count++;
        }

        // Connect points (handle saddle case with config 5 or 10)
        if (point_count == 2) {
          out_lines.push_back(
              {points[0][0], points[0][1], points[1][0], points[1][1]});
        } else if (point_count == 4) {
          // Saddle case - connect based on center value
          float center = (h00 + h10 + h11 + h01) * 0.25f;
          if (center >= level) {
            out_lines.push_back(
                {points[0][0], points[0][1], points[1][0], points[1][1]});
            out_lines.push_back(
                {points[2][0], points[2][1], points[3][0], points[3][1]});
          } else {
            out_lines.push_back(
                {points[0][0], points[0][1], points[3][0], points[3][1]});
            out_lines.push_back(
                {points[1][0], points[1][1], points[2][0], points[2][1]});
          }
        }
      }
    }
  }
}
