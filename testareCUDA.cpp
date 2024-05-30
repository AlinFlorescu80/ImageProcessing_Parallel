#include <iostream>
#include <vector>
#include <chrono>
#include <cuda_runtime.h>
#include <curand_kernel.h>

using namespace std;
using namespace std::chrono;

struct Color {
    unsigned char r, g, b;
};

struct Image {
    int width, height;
    vector<Color> pixels;
};

__global__ void initializeRandomColors(Color* pixels, int width, int height, unsigned long seed) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < width && y < height) {
        int idx = y * width + x;
        curandState state;
        curand_init(seed, idx, 0, &state);
        pixels[idx].r = curand(&state) % 256;
        pixels[idx].g = curand(&state) % 256;
        pixels[idx].b = curand(&state) % 256;
    }
}

__global__ void perspectiveTransformKernel(Color* d_in, Color* d_out, int width, int height, double* d_matrix) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x < width && y < height) {
        double newX = (d_matrix[0] * x + d_matrix[1] * y + d_matrix[2]) / (d_matrix[6] * x + d_matrix[7] * y + d_matrix[8]);
        double newY = (d_matrix[3] * x + d_matrix[4] * y + d_matrix[5]) / (d_matrix[6] * x + d_matrix[7] * y + d_matrix[8]);

        int intNewX = static_cast<int>(newX);
        int intNewY = static_cast<int>(newY);

        if (intNewX >= 0 && intNewX < width && intNewY >= 0 && intNewY < height) {
            d_out[y * width + x] = d_in[intNewY * width + intNewX];
        } else {
            d_out[y * width + x] = {0, 0, 0}; // Black color for out-of-bounds
        }
    }
}

Image generateRandomImage(int width, int height) {
    Color *d_pixels;
    size_t imageSize = width * height * sizeof(Color);
    cudaMalloc(&d_pixels, imageSize);

    dim3 threadsPerBlock(16, 16);
    dim3 numBlocks((width + threadsPerBlock.x - 1) / threadsPerBlock.x,
                   (height + threadsPerBlock.y - 1) / threadsPerBlock.y);

    initializeRandomColors<<<numBlocks, threadsPerBlock>>>(d_pixels, width, height, time(0));

    Image img;
    img.width = width;
    img.height = height;
    img.pixels.resize(width * height);
    cudaMemcpy(img.pixels.data(), d_pixels, imageSize, cudaMemcpyDeviceToHost);
    cudaFree(d_pixels);

    return img;
}

Image perspectiveTransformCUDA(const Image& img, double matrix[3][3]) {
    Color *d_in, *d_out;
    double *d_matrix;
    size_t imageSize = img.width * img.height * sizeof(Color);
    size_t matrixSize = 9 * sizeof(double);

    cudaMalloc(&d_in, imageSize);
    cudaMalloc(&d_out, imageSize);
    cudaMalloc(&d_matrix, matrixSize);

    cudaMemcpy(d_in, img.pixels.data(), imageSize, cudaMemcpyHostToDevice);
    cudaMemcpy(d_matrix, matrix, matrixSize, cudaMemcpyHostToDevice);

    dim3 threadsPerBlock(16, 16);
    dim3 numBlocks((img.width + threadsPerBlock.x - 1) / threadsPerBlock.x,
                   (img.height + threadsPerBlock.y - 1) / threadsPerBlock.y);

    perspectiveTransformKernel<<<numBlocks, threadsPerBlock>>>(d_in, d_out, img.width, img.height, d_matrix);

    Image transformedImg;
    transformedImg.width = img.width;
    transformedImg.height = img.height;
    transformedImg.pixels.resize(img.width * img.height);

    cudaMemcpy(transformedImg.pixels.data(), d_out, imageSize, cudaMemcpyDeviceToHost);

    cudaFree(d_in);
    cudaFree(d_out);
    cudaFree(d_matrix);

    return transformedImg;
}

int main() {
    auto start = high_resolution_clock::now();

    // Generate a random image of 3840x2160
    int width = 3840;
    int height = 2160;
    Image originalImage = generateRandomImage(width, height);

    // Define the perspective transformation matrix with moderate effects
    double matrix[3][3] = {
        {1, 0.2, 0},
        {0.2, 1, 0},
        {0.0002, 0.0002, 1}
    };

    Image transformedImage = perspectiveTransformCUDA(originalImage, matrix);

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);
    cout << "Time taken: " << duration.count() << " milliseconds" << endl;

    // Print a part of the resulting matrix to verify
    for (int y = 0; y < min(10, height); y++) {
        for (int x = 0; x < min(10, width); x++) {
            Color& c = transformedImage.pixels[y * width + x];
            printf("(%3d, %3d, %3d) ", c.r, c.g, c.b);
        }
        cout << endl;
    }

    return 0;
}
