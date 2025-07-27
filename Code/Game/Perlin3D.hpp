#pragma once

#include "Engine/Core/EngineCommon.hpp"

int Hash(int x, int y, int z);

Vec3 Gradient(int x, int y, int z);

float Fade(float t);

float Lerp(float a, float b, float t);

float DotGridGradient(int ix, int iy, int iz, float x, float y, float z);

float PerlinNoise3D(float x, float y, float z);

std::vector<float> GeneratePerlin3D(int width, int height, int depth, float scale, float frequency, float amplitude, float persistence, int numOctaves, int seed);