// noise.cpp
#include "noise.h"
#include "FastNoiseLite.h"
#include <algorithm>
#include <cmath>
#include <vector>

void generate_heightmap(std::span<float> out, int width, int height,
                        const NoiseParams &params) {
  // Initialize output and gradient accumulators
  std::vector<float> gradient_x(width * height, 0.0f);
  std::vector<float> gradient_y(width * height, 0.0f);
  std::fill(out.begin(), out.end(), 0.0f);

  FastNoiseLite noise(params.seed);
  noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  noise.SetFractalType(
      FastNoiseLite::FractalType_None); // We'll do fractal manually

  float amplitude = 1.0f;
  float frequency = params.frequency;
  float max_value = 0.0f;

  constexpr float GRADIENT_SCALE = 2.0f; // Controls erosion strength

  // Build up octaves, applying gradient trick per layer
  for (int octave = 0; octave < params.octaves; ++octave) {
    noise.SetFrequency(frequency);

    // Sample current octave
    std::vector<float> octave_values(width * height);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        octave_values[y * width + x] = noise.GetNoise((float)x, (float)y);
      }
    }

    // Calculate gradient magnitude from accumulated gradients
    for (int y = 1; y < height - 1; ++y) {
      for (int x = 1; x < width - 1; ++x) {
        int idx = y * width + x;
        float grad_magnitude = std::sqrt(gradient_x[idx] * gradient_x[idx] +
                                         gradient_y[idx] * gradient_y[idx]);

        // Reduce amplitude on steep slopes (the gradient trick!)
        float erosion_factor = 1.0f / (1.0f + grad_magnitude * GRADIENT_SCALE);
        float scaled_amplitude = amplitude * erosion_factor;

        // Add this octave's contribution
        out[idx] += octave_values[idx] * scaled_amplitude;

        // Calculate and accumulate this octave's gradient
        float dx = (octave_values[y * width + (x + 1)] -
                    octave_values[y * width + (x - 1)]) *
                   0.5f;
        float dy = (octave_values[(y + 1) * width + x] -
                    octave_values[(y - 1) * width + x]) *
                   0.5f;

        gradient_x[idx] += dx * scaled_amplitude * frequency;
        gradient_y[idx] += dy * scaled_amplitude * frequency;
      }
    }

    max_value += amplitude;
    amplitude *= params.gain;
    frequency *= params.lacunarity;
  }
  for (int x = 0; x < width; ++x) {
    out[x] = out[width + x];                                       // Top row
    out[(height - 1) * width + x] = out[(height - 2) * width + x]; // Bottom row
  }
  for (int y = 0; y < height; ++y) {
    out[y * width] = out[y * width + 1];                         // Left column
    out[y * width + (width - 1)] = out[y * width + (width - 2)]; // Right column
  }
  // Normalize to [0, 1]
  for (int i = 0; i < width * height; ++i) {
    out[i] = (out[i] / max_value + 1.0f) * 0.5f;
  }
}
