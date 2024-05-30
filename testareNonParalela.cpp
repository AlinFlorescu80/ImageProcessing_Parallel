#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <cmath>

using namespace std;
using namespace std::chrono;

#pragma pack(push, 1)
struct BMPHeader {
    char header[2]{'B', 'M'};
    int32_t fileSize;
    int32_t reserved{0};
    int32_t dataOffset;
    int32_t headerSize{40};
    int32_t width;
    int32_t height;
    int16_t planes{1};
    int16_t bpp{24};
    int32_t compression{0};
    int32_t dataSize{0};
    int32_t hRes{0};
    int32_t vRes{0};
    int32_t colors{0};
    int32_t importantColors{0};
};
#pragma pack(pop)

struct Color {
    unsigned char r, g, b;
};

struct Image {
    int width, height;
    vector<Color> pixels;
};

Image readBMP(const char* filename) {
    ifstream file(filename, ios::binary);
    Image img;
    BMPHeader header;
    
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    img.width = header.width;
    img.height = header.height;
    
    int padding = (4 - (img.width * 3) % 4) % 4;
    img.pixels.resize(img.width * img.height);

    for (int y = img.height - 1; y >= 0; y--) {
        for (int x = 0; x < img.width; x++) {
            file.read(reinterpret_cast<char*>(&img.pixels[y * img.width + x].b), 1);
            file.read(reinterpret_cast<char*>(&img.pixels[y * img.width + x].g), 1);
            file.read(reinterpret_cast<char*>(&img.pixels[y * img.width + x].r), 1);
        }
        file.seekg(padding, ios::cur);
    }

    return img;
}

void writeBMP(const char* filename, const Image& img) {
    ofstream file(filename, ios::binary);
    BMPHeader header;
    int padding = (4 - (img.width * 3) % 4) % 4;
    int dataSize = (img.width * 3 + padding) * img.height;
    
    header.fileSize = sizeof(header) + dataSize;
    header.dataOffset = sizeof(header);
    header.width = img.width;
    header.height = img.height;
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    for (int y = img.height - 1; y >= 0; y--) {
        for (int x = 0; x < img.width; x++) {
            file.put(img.pixels[y * img.width + x].b);
            file.put(img.pixels[y * img.width + x].g);
            file.put(img.pixels[y * img.width + x].r);
        }
        for (int i = 0; i < padding; i++) {
            file.put(0);
        }
    }
}

Image resizeImage(const Image& img, int newWidth, int newHeight) {
    Image resizedImg;
    resizedImg.width = newWidth;
    resizedImg.height = newHeight;
    resizedImg.pixels.resize(newWidth * newHeight);
    
    for (int y = 0; y < newHeight; y++) {
        for (int x = 0; x < newWidth; x++) {
            int originalX = static_cast<int>((static_cast<double>(x) / newWidth) * img.width);
            int originalY = static_cast<int>((static_cast<double>(y) / newHeight) * img.height);

            // Apply a simple 3x3 convolution filter
            int sumR = 0, sumG = 0, sumB = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = originalX + dx;
                    int ny = originalY + dy;
                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        Color originalPixel = img.pixels[ny * img.width + nx];
                        sumR += originalPixel.r;
                        sumG += originalPixel.g;
                        sumB += originalPixel.b;
                    }
                }
            }

            // Average the color values
            int numPixels = 9;
            Color modifiedPixel;
            modifiedPixel.r = sumR / numPixels;
            modifiedPixel.g = sumG / numPixels;
            modifiedPixel.b = sumB / numPixels;

            // Assign the modified pixel to the resized image
            resizedImg.pixels[y * newWidth + x] = modifiedPixel;
        }
    }

    return resizedImg;
}

Image perspectiveTransform(const Image& img, double matrix[3][3]) {
    Image transformedImg;
    transformedImg.width = img.width;
    transformedImg.height = img.height;
    transformedImg.pixels.resize(img.width * img.height);

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            double newX = (matrix[0][0] * x + matrix[0][1] * y + matrix[0][2]) / (matrix[2][0] * x + matrix[2][1] * y + matrix[2][2]);
            double newY = (matrix[1][0] * x + matrix[1][1] * y + matrix[1][2]) / (matrix[2][0] * x + matrix[2][1] * y + matrix[2][2]);

            int intNewX = static_cast<int>(newX);
            int intNewY = static_cast<int>(newY);

            if (intNewX >= 0 && intNewX < img.width && intNewY >= 0 && intNewY < img.height) {
                transformedImg.pixels[y * img.width + x] = img.pixels[intNewY * img.width + intNewX];
            } else {
                transformedImg.pixels[y * img.width + x] = {0, 0, 0}; // Black color for out-of-bounds
            }
        }
    }

    return transformedImg;
}

int main() {
    auto start = high_resolution_clock::now();

    Image originalImage = readBMP("C:\\Facultate\\PP\\Proiect\\ProiectPPFinalTry\\exempluMare.bmp");

    Image resizedImage = resizeImage(originalImage, 2000, 2000);

    writeBMP("C:\\Facultate\\PP\\Proiect\\ProiectPPFinalTry\\imagineMareResized.bmp", resizedImage);

    // Define the perspective transformation matrix
   double matrix[3][3] = {
        {1, 0.2, 0},
        {0.2, 1, 0},
        {0.0002, 0.0002, 1}
    };
    Image transformedImage = perspectiveTransform(resizedImage, matrix);

    writeBMP("C:\\Facultate\\PP\\Proiect\\ProiectPPFinalTry\\imagineMareTransformed.bmp", transformedImage);

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);
    cout << "Time taken: " << duration.count() << " milliseconds" << endl;

    return 0;
}
