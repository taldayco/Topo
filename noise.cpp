// noise.cpp
#include "noise.h"
#include "FastNoiseLite.h"

void generate_heightmap(std::span<float> out, int width, int height,
                        const NoiseParams &params) {
  FastNoiseLite noise(params.seed);
  noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
  noise.SetFractalType(FastNoiseLite::FractalType_FBm);
  noise.SetFractalOctaves(params.octaves);
  noise.SetFractalLacunarity(params.lacunarity);
  noise.SetFractalGain(params.gain);
  noise.SetFrequency(params.frequency);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float value = noise.GetNoise((float)x, (float)y);
      out[y * width + x] = (value + 1.0f) * 0.5f; // normalize to [0,1]
    }
  }
}
