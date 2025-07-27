#include "Perlin3D.hpp"

int Hash(int x, int y, int z)
{
	// A hash function that scrambles the input values to return a unique value for each (x, y, z)
	return (x * 73856093 ^ y * 19349663 ^ z * 83492791) & 255;
}

Vec3 Gradient(int x, int y, int z)
{
	static Vec3 gradients[] = {
		Vec3(1,1,0), Vec3(-1,1,0), Vec3(1,-1,0), Vec3(-1,-1,0),
		Vec3(1,0,1), Vec3(-1,0,1), Vec3(1,0,-1), Vec3(-1,0,-1),
		Vec3(0,1,1), Vec3(0,-1,1), Vec3(0,1,-1), Vec3(0,-1,-1)
	};
	int hashValue = Hash(x, y, z);
	return gradients[hashValue % 12];  // Use hash to pick a gradient
}

float Fade(float t)
{
	// 6t^5 - 15t^4 + 10t^3, a standard smoothstep function for interpolation
	return t * t * t * (t * (t * 6 - 15) + 10);
}

float Lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

float DotGridGradient(int ix, int iy, int iz, float x, float y, float z)
{
	// Compute the distance vector
	Vec3 distance = Vec3(x - ix, y - iy, z - iz);

	// Compute the gradient vector at the grid point
	Vec3 gradient = Gradient(ix, iy, iz);

	// Compute the dot product between the distance and gradient vectors
	return gradient.x * distance.x + gradient.y * distance.y + gradient.z * distance.z;
}

float PerlinNoise3D(float x, float y, float z)
{
	// Determine grid cell coordinates (lower corner)
	int x0 = (int)std::floor(x);
	int x1 = x0 + 1;
	int y0 = (int)std::floor(y);
	int y1 = y0 + 1;
	int z0 = (int)std::floor(z);
	int z1 = z0 + 1;

	// Determine relative positions within cell
	float tx = x - x0;
	float ty = y - y0;
	float tz = z - z0;

	// Apply fade curves to relative positions
	float u = Fade(tx);
	float v = Fade(ty);
	float w = Fade(tz);

	// Dot products of each corner of the cube
	float n000 = DotGridGradient(x0, y0, z0, x, y, z);
	float n100 = DotGridGradient(x1, y0, z0, x, y, z);
	float n010 = DotGridGradient(x0, y1, z0, x, y, z);
	float n110 = DotGridGradient(x1, y1, z0, x, y, z);

	float n001 = DotGridGradient(x0, y0, z1, x, y, z);
	float n101 = DotGridGradient(x1, y0, z1, x, y, z);
	float n011 = DotGridGradient(x0, y1, z1, x, y, z);
	float n111 = DotGridGradient(x1, y1, z1, x, y, z);

	// Interpolate along x
	float nx00 = Lerp(u, n000, n100);
	float nx10 = Lerp(u, n010, n110);
	float nx01 = Lerp(u, n001, n101);
	float nx11 = Lerp(u, n011, n111);

	// Interpolate along y
	float nxy0 = Lerp(v, nx00, nx10);
	float nxy1 = Lerp(v, nx01, nx11);

	// Interpolate along z
	float nxyz = Lerp(w, nxy0, nxy1);

	// Return the final noise value
	return nxyz;
}

std::vector<float> GeneratePerlin3D(int width, int height, int depth, float scale, float frequency, float amplitude, float persistence, int numOctaves, int seed)
{
	UNUSED(seed);
	std::vector<float> noiseArray (width * height * depth);

	//float* noise = new float[width * height * depth];
	//float maxNoise = 0.f;
	//float minNoise = 0.f;
	//
	//for (int z = 0; z < depth; z++)
	//{
	//	for (int y = 0; y < height; y++)
	//	{
	//		for (int x = 0; x < width; x++)
	//		{
	//			float noiseValue = 0.f;
	//			float amplitudeAccumulator = 0.f;
	//			float frequencyAccumulator = 0.f;
	//
	//			for (int octave = 0; octave < numOctaves; octave++)
	//			{
	//				float sampleX = (float)x * frequency + frequencyAccumulator;
	//				float sampleY = (float)y * frequency + frequencyAccumulator;
	//				float sampleZ = (float)z * frequency + frequencyAccumulator;
	//
	//				float perlinValue = PerlinNoise3D(sampleX, sampleY, sampleZ);
	//				noiseValue += perlinValue * amplitude;
	//				amplitudeAccumulator += amplitude;
	//				frequencyAccumulator += frequency;
	//				amplitude *= persistence;
	//			}
	//
	//			noise[x + y * width + z * width * height] = noiseValue;
	//			maxNoise = std::max(maxNoise, noiseValue);
	//			minNoise = std::min(minNoise, noiseValue);
	//		}
	//	}
	//}
	//
	//for (int i = 0; i < width * height * depth; i++)
	//{
	//	noise[i] = (noise[i] - minNoise) / (maxNoise - minNoise);
	//}
	//
	//return noise;



	for (int z = 0; z < depth; ++z) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				float noiseValue = 0.0f;
				float currentAmplitude = amplitude;
				float currentFrequency = frequency * scale;

				// Accumulate noise value over the octaves
				for (int octave = 0; octave < numOctaves; ++octave) {
					float sampleX = x * currentFrequency;
					float sampleY = y * currentFrequency;
					float sampleZ = z * currentFrequency;

					noiseValue += PerlinNoise3D(sampleX, sampleY, sampleZ) * currentAmplitude;

					// Adjust amplitude and frequency for the next octave
					currentAmplitude *= persistence;
					currentFrequency *= 2.0f;
				}

				// Store the final noise value in the array
				noiseArray[z * width * height + y * width + x] = noiseValue;
			}
		}
	}
	return noiseArray;
}
