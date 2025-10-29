#include "plateau.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <queue>

std::vector<Plateau> detect_plateaus(std::span<const float> heightmap,
                                     int width, int height) {

  std::vector<bool> visited(width * height, false);
  std::vector<Plateau> plateaus;

  const float HEIGHT_THRESHOLD = 0.02f;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int idx = y * width + x;
      if (visited[idx])
        continue;

      float plateau_height = heightmap[idx];
      Plateau plateau;
      plateau.height = plateau_height;

      std::queue<int> queue;
      queue.push(idx);
      visited[idx] = true;

      float sum_x = 0, sum_y = 0;
      int count = 0;

      while (!queue.empty()) {
        int current = queue.front();
        queue.pop();

        plateau.pixels.push_back(current);

        int cx = current % width;
        int cy = current / width;
        sum_x += cx;
        sum_y += cy;
        count++;

        int neighbors[4][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
        for (auto [dx, dy] : neighbors) {
          int nx = cx + dx;
          int ny = cy + dy;

          if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
            int nidx = ny * width + nx;
            if (!visited[nidx] &&
                std::abs(heightmap[nidx] - plateau_height) < HEIGHT_THRESHOLD) {
              visited[nidx] = true;
              queue.push(nidx);
            }
          }
        }
      }

      plateau.center_x = sum_x / count;
      plateau.center_y = sum_y / count;

      if (count > 50) {
        plateaus.push_back(plateau);
      }
    }
  }

  SDL_Log("Detected %zu plateaus", plateaus.size());
  return plateaus;
}
