#pragma once
#include <fstream>
#include <vector>
#include <string.h>
#include <math.h>

#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t file_type{ 0x4D42 };          // File type always BM which is 0x4D42 (stored as hex uint16_t in little endian)
    uint32_t file_size{ 0 };               // Size of the file (in bytes)
    uint16_t reserved1{ 0 };               // Reserved, always 0
    uint16_t reserved2{ 0 };               // Reserved, always 0
    uint32_t offset_data{ 0 };             // Start position of pixel data (bytes from the beginning of the file)
};

struct BMPInfoHeader {
    uint32_t size{ 0 };                      // Size of this header (in bytes)
    int32_t width{ 0 };                      // width of bitmap in pixels
    int32_t height{ 0 };                     // width of bitmap in pixels
                                             //       (if positive, bottom-up, with origin in lower left corner)
                                             //       (if negative, top-down, with origin in upper left corner)
    uint16_t planes{ 1 };                    // No. of planes for the target device, this is always 1
    uint16_t bit_count{ 0 };                 // No. of bits per pixel
    uint32_t compression{ 0 };               // 0 or 3 - uncompressed. THIS PROGRAM CONSIDERS ONLY UNCOMPRESSED BMP images
    uint32_t size_image{ 0 };                // 0 - for uncompressed images
    int32_t x_pixels_per_meter{ 0 };
    int32_t y_pixels_per_meter{ 0 };
    uint32_t colors_used{ 0 };               // No. color indexes in the color table. Use 0 for the max number of colors allowed by bit_count
    uint32_t colors_important{ 0 };          // No. of colors used for displaying the bitmap. If 0 all colors are required
};

struct BMPColorHeader {
    uint32_t red_mask{ 0x00ff0000 };         // Bit mask for the red channel
    uint32_t green_mask{ 0x0000ff00 };       // Bit mask for the green channel
    uint32_t blue_mask{ 0x000000ff };        // Bit mask for the blue channel
    uint32_t alpha_mask{ 0xff000000 };       // Bit mask for the alpha channel
    uint32_t color_space_type{ 0x73524742 }; // Default "sRGB" (0x73524742)
    uint32_t unused[16]{ 0 };                // Unused data for sRGB color space
};
#pragma pack(pop)

// Point Struct
struct Point {
    uint32_t x;
    uint32_t y;

    Point(const uint32_t x, const uint32_t y) {
        this->x = x;
        this->y = y;
    }
    friend bool operator==(const Point& p1, const Point& p2);

    Point& operator=(const Point& other) { 
        if (this != &other) {
            x = other.x;
            y = other.y;
        }
        return *this;
    }
};

bool operator==(const Point& p1, const Point& p2) {
    return (p1.x == p2.x && p1.y == p2.y);
}

// Color Struct
struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t alpha;

    Color(const uint8_t r, const uint8_t g, const u_int8_t b, const uint8_t alpha = 255) {
        this->r = r;
        this->g = g;
        this->b = b;
        this->alpha = alpha;
    }

    Color& operator=(const Color& other) {
        if (this != &other) {
            r = other.r;
            g = other.g;
            b = other.b;
            alpha = other.alpha;
        }

        return *this;
    }
};

// Pixel Struct
struct pixel {
    
    uint32_t x;
    uint32_t y;
    Color color;
    //Point point;

    //pixel(const Color& c_, const Point& p_) : color(c_), point(p_) {}
    pixel(const uint32_t x_, const uint32_t y_, const Color& c_) : x(x_), y(y_), color(c_) {}
    
    pixel& operator=(const pixel& other) {
        if (this != &other) {
            color = other.color;
            x = other.x;
            y = other.y;
        }

        return *this;
    }
};

// BMP class
class BMP {
    public:
        BMP(const char *fname) {
            read(fname);
        }

        BMP(int32_t width, int32_t height, bool has_alpha = true) {
            if (width <= 0 || height <= 0) {
                std::cerr << "The image width and height must be positive numbers.\n";
            }

            bmp_info_header.width = width;
            bmp_info_header.height = height;
            if (has_alpha) {
                bmp_info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
                file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);

                bmp_info_header.bit_count = 32;
                bmp_info_header.compression = 3;
                row_stride = width * 4;
                data.resize(row_stride * height);
                file_header.file_size = file_header.offset_data + data.size();
            }
            else {
                bmp_info_header.size = sizeof(BMPInfoHeader);
                file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

                bmp_info_header.bit_count = 24;
                bmp_info_header.compression = 0;
                row_stride = width * 3;
                data.resize(row_stride * height);

                uint32_t new_stride = make_stride_aligned(4);
                file_header.file_size = file_header.offset_data + data.size() + bmp_info_header.height * (new_stride - row_stride);
            }

            channels = bmp_info_header.bit_count / 8;
        }
        
        void read(const char *fname) {
            std::ifstream inp{ fname, std::ios_base::binary };
            if (inp) {
                inp.read((char*)&file_header, sizeof(file_header));
                if(file_header.file_type != 0x4D42) {
                    std::cerr << "Error! Unrecognized file format.\n";
                }
                inp.read((char*)&bmp_info_header, sizeof(bmp_info_header));

                // The BMPColorHeader is used only for transparent images
                if(bmp_info_header.bit_count == 32) {
                    // Check if the file has bit mask color information
                    if(bmp_info_header.size >= (sizeof(BMPInfoHeader) + sizeof(BMPColorHeader))) {
                        inp.read((char*)&bmp_color_header, sizeof(bmp_color_header));
                        // Check if the pixel data is stored as BGRA and if the color space type is sRGB
                        check_color_header(bmp_color_header);
                    } else {
                        std::cerr << "Error! The file \"" << fname << "\" does not seem to contain bit mask information\n";
                        std::cerr << "Error! Unrecognized file format.\n";
                    }
                }

                // Jump to the pixel data location
                inp.seekg(file_header.offset_data, inp.beg);

                // Adjust the header fields for output.
                // Some editors will put extra info in the image file, we only save the headers and the data.
                if(bmp_info_header.bit_count == 32) {
                    bmp_info_header.size = sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
                    file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + sizeof(BMPColorHeader);
                } else {
                    bmp_info_header.size = sizeof(BMPInfoHeader);
                    file_header.offset_data = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
                }
                file_header.file_size = file_header.offset_data;

                if (bmp_info_header.height < 0) {
                    std::cerr << "The program can treat only BMP images with the origin in the bottom left corner!\n";
                }

                data.resize(bmp_info_header.width * bmp_info_header.height * bmp_info_header.bit_count / 8);

                // Here we check if we need to take into account row padding
                if (bmp_info_header.width % 4 == 0) {
                    inp.read((char*)data.data(), data.size());
                    file_header.file_size += data.size();
                }
                else {
                    row_stride = bmp_info_header.width * bmp_info_header.bit_count / 8;
                    uint32_t new_stride = make_stride_aligned(4);
                    std::vector<uint8_t> padding_row(new_stride - row_stride);

                    for (int y = 0; y < bmp_info_header.height; ++y) {
                        inp.read((char*)(data.data() + row_stride * y), row_stride);
                        inp.read((char*)padding_row.data(), padding_row.size());
                    }
                    file_header.file_size += data.size() + bmp_info_header.height * padding_row.size();
                }

                channels = bmp_info_header.bit_count / 8;
            }
            else {
                std::cerr << "Unable to open the input image file.\n";
            }
        }

        void write(const char *fname) {
            std::ofstream of{ fname, std::ios_base::binary };
            if (of) {
                if (bmp_info_header.bit_count == 32) {
                    write_headers_and_data(of);
                }
                else if (bmp_info_header.bit_count == 24) {
                    if (bmp_info_header.width % 4 == 0) {
                        write_headers_and_data(of);
                    }
                    else {
                        uint32_t new_stride = make_stride_aligned(4);
                        std::vector<uint8_t> padding_row(new_stride - row_stride);

                        write_headers(of);

                        for (int y = 0; y < bmp_info_header.height; ++y) {
                            of.write((const char*)(data.data() + row_stride * y), row_stride);
                            of.write((const char*)padding_row.data(), padding_row.size());
                        }
                    }
                }
                else {
                    std::cerr << "The program can treat only 24 or 32 bits per pixel BMP files\n";
                }
            }
            else {
                std::cerr << "Unable to open the output image file.\n";
            }
        }

        int32_t width() const {
            return bmp_info_header.width;
        }

        int32_t height() const {
            return bmp_info_header.height;
        }
        
        uint32_t Channels() const {
            return channels;
        }
        
        Color getPixel(const uint32_t x, const uint32_t y) const {
            if (x < 0 || x > (uint32_t)bmp_info_header.width || y < 0 || y > (uint32_t)bmp_info_header.height) {
                std::cerr << "setPixel error: Pixel is outside!\n";
            }

            uint32_t pos = channels * (y * bmp_info_header.width + x);
            Color c(data[pos + 2], data[pos + 1], data[pos + 0]);
            if (channels == 4) {
                c.alpha = data[pos + 3];
            }

            return c;
        }

        void setPixel(const uint32_t x, const uint32_t y, const Color& c) {
            if (x < 0 || x > (uint32_t)bmp_info_header.width || y < 0 || y > (uint32_t)bmp_info_header.height) {
                std::cerr << "setPixel error: Pixel is outside!\n";
            }

            uint32_t pos = channels * (y * bmp_info_header.width + x);
            data[pos + 0] = c.b;
            data[pos + 1] = c.g;
            data[pos + 2] = c.r;
            if (channels == 4) {
                data[pos + 3] = c.alpha;
            }
        }

        void clear(const uint8_t c) {
            std::fill(data.begin(),data.end(), c);
        }

        void CopyFrom(BMP& image) {
            if (image.channels == channels && image.height() == bmp_info_header.height && image.width() == bmp_info_header.width) { // Can copy
                data = image.data;
            } else {
                std::cerr << "CopyFrom error!\n";
            }
        }

        /// Effects

        void BlackWhite(const float r = 0.33, const float g = 0.33, const float b = 0.33) {
            if (r + g + b > 1) {
                std::cerr << "BlackWhite error: Invalid grey scale\n";
            }

            for (uint32_t y = 0; y < bmp_info_header.height; ++y) {
                for (uint32_t x = 0; x < bmp_info_header.width; ++x) {
                    uint32_t pos = channels * (y * bmp_info_header.width + x);
                    uint8_t grey = data[pos + 0] * b + data[pos + 1] * g + data[pos + 2] * r;
                    data[pos + 0] = grey; // blue
                    data[pos + 1] = grey; // green
                    data[pos + 2] = grey; // red
                }
            }
        }

        void FlipX() {
            for (uint32_t y = 0; y < (uint32_t)bmp_info_header.height; y++) {
                for(uint32_t x = 0; x < (uint32_t)bmp_info_header.width/2; x++) {
                    uint32_t pos1 = channels * (y * bmp_info_header.width + x);
                    uint32_t pos2 = channels * (y * bmp_info_header.width + bmp_info_header.width - 1 - x);
                    
                    // Save color of one of both pixel
                    Color aux(data[pos1 + 2], data[pos1 + 1], data[pos1 + 0]);

                    // Exchange
                    data[pos1 + 0] = data[pos2 + 0];
                    data[pos1 + 1] = data[pos2 + 1];
                    data[pos1 + 2] = data[pos2 + 2];

                    data[pos2 + 0] = aux.b;
                    data[pos2 + 1] = aux.g;
                    data[pos2 + 2] = aux.r;


                    if (channels == 4) {
                        aux.alpha = data[pos1 + 3];
                        data[pos1 + 3] = data[pos2 + 3];
                        data[pos2 + 3] = aux.alpha;
                    }
                }
            }
        }

        void FlipY() {
            for (uint32_t x = 0; x < (uint32_t)bmp_info_header.width; x++) {
                for (uint32_t y = 0; y < (uint32_t)bmp_info_header.height/2; y++) {
                    uint32_t pos1 = channels * (y * bmp_info_header.width + x);
                    uint32_t pos2 = channels * ((bmp_info_header.height - 1 - y) * bmp_info_header.width + x);
                    
                    // Save color of one of both pixel
                    Color aux(data[pos1 + 2], data[pos1 + 1], data[pos1 + 0]);

                    // Exchange
                    data[pos1 + 0] = data[pos2 + 0];
                    data[pos1 + 1] = data[pos2 + 1];
                    data[pos1 + 2] = data[pos2 + 2];

                    data[pos2 + 0] = aux.b;
                    data[pos2 + 1] = aux.g;
                    data[pos2 + 2] = aux.r;


                    if (channels == 4) {
                        aux.alpha = data[pos1 + 3];
                        data[pos1 + 3] = data[pos2 + 3];
                        data[pos2 + 3] = aux.alpha;
                    }
                }
            }
        }

    private:

        BMPFileHeader file_header;
        BMPInfoHeader bmp_info_header;
        BMPColorHeader bmp_color_header;
        std::vector<uint8_t> data;

        uint32_t channels;

        const int MaxList = 10;
        
        uint32_t row_stride{ 0 };

        void write_headers(std::ofstream &of) {
            of.write((const char*)&file_header, sizeof(file_header));
            of.write((const char*)&bmp_info_header, sizeof(bmp_info_header));
            if(bmp_info_header.bit_count == 32) {
                of.write((const char*)&bmp_color_header, sizeof(bmp_color_header));
            }
        }

        void write_headers_and_data(std::ofstream &of) {
            write_headers(of);
            of.write((const char*)data.data(), data.size());
        }

        // Add 1 to the row_stride until it is divisible with align_stride
        uint32_t make_stride_aligned(uint32_t align_stride) {
            uint32_t new_stride = row_stride;
            while (new_stride % align_stride != 0) {
                new_stride++;
            }
            return new_stride;
        }

        // Check if the pixel data is stored as BGRA and if the color space type is sRGB
        void check_color_header(BMPColorHeader &bmp_color_header) {
            BMPColorHeader expected_color_header;
            if(expected_color_header.red_mask != bmp_color_header.red_mask ||
                expected_color_header.blue_mask != bmp_color_header.blue_mask ||
                expected_color_header.green_mask != bmp_color_header.green_mask ||
                expected_color_header.alpha_mask != bmp_color_header.alpha_mask) {
                    std::cerr << "Unexpected color mask format! The program expects the pixel data to be in the BGRA format\n";
            }
            if(expected_color_header.color_space_type != bmp_color_header.color_space_type) {
                std::cerr << "Unexpected color space type! The program expects sRGB values\n";
            }
        }
};

// bmpDrawer class
class bmpDrawer {
    public:
        bmpDrawer(BMP* image_) : image(image_) {}

        /// Draw on image

        void drawPixel(const uint32_t x, const uint32_t y, const Color& c) {
            image->setPixel(x,y,c);
        }

        void drawPixel(const pixel& px) {
            drawPixel(px.x, px.y, px.color);
        }

        void drawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const Color& c) {
            
            if (x1 == x2 && y1 == y2) { // One pixel
                drawPixel(x1,y1,c);
            } else if (y1 == y2) { // Horizontal line
                if (x1 > x2) {
                    std::swap(x1,x2);
                }
                for (int32_t i = 0; i < (x2 - x1); i++) {
                    drawPixel(x1 + i, y1, c);
                }
            } else if (x1 == x2) { // Vertical line
                if (y1 > y2) {
                    std::swap(y1,y2);
                }
                for (int32_t i = 0; i < (y2 - y1); i++) {
                    drawPixel(x1, y1 + i, c);
                }
            } else {
                // DDA algorithm
                int32_t steep = 0;
                int32_t sx = ((x2 - x1) > 0) ? 1 : -1;
                int32_t sy    = ((y2 - y1) > 0) ? 1 : -1;
                int32_t dx    = abs(x2 - x1);
                int32_t dy    = abs(y2 - y1);

                if (dy > dx)
                {
                    std::swap(x1,y1);
                    std::swap(dx,dy);
                    std::swap(sx,sy);

                    steep = 1;
                }

                int32_t e = 2 * dy - dx;

                for (int32_t i = 0; i < dx; ++i)
                {
                    if (steep) {
                        drawPixel(y1,x1,c);
                    } else {
                        drawPixel(x1,y1,c);
                    }
                    while (e >= 0)
                    {
                        y1 += sy;
                        e -= (dx << 1);
                    }

                    x1 += sx;
                    e  += (dy << 1);
                }

                drawPixel(x2,y2,c);
            }
        }

        void drawTriangle(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t x3, uint32_t y3, const Color& c) {
            drawLine(x1, y1, x2, y2, c);
            drawLine(x2, y2, x3, y3, c);
            drawLine(x3, y3, x1, y1, c);
        }

        void drawCircle(const uint32_t x_center, const uint32_t y_center, int32_t radius, const Color& c) {
            int32_t x = 0;
            int32_t d = (1 - radius) << 1;
            
            while(radius >= 0) {
                drawPixel(x_center + x, y_center + radius, c);
                drawPixel(x_center + x, y_center - radius, c);
                drawPixel(x_center - x, y_center + radius, c);
                drawPixel(x_center - x, y_center - radius, c);

                if ((d + radius) > 0) {
                    d -= ((--radius) << 1) - 1;
                }
                if (x > d) {
                    d += ((++x) << 1) + 1;
                }
            }
            
        }

        void drawRegion(const uint32_t x, const uint32_t y, const uint32_t w, const uint32_t h, const Color& c) {
            for (uint32_t yy = y; yy < y + h; ++yy) {
                for (uint32_t xx = x; xx < x + w; ++xx) {
                    drawPixel(xx,yy,c);
                }
            }
        }

            /// 32 bits/pixel only

        void erasePixel(const uint32_t x, const uint32_t y) {
            if (image->Channels() != 4) {
                std::cerr << "erasePixel error: only for 32 bits/pixel\n";
            }
            drawPixel(x,y,Color(0,0,0,0));
        }

        void erasePixel(const Point& p) {
            erasePixel(p.x, p.y);
        }

        void eraseRegion(const uint32_t x, const uint32_t y, const uint32_t w, const uint32_t h) {
            if (image->Channels() != 4) {
                std::cerr << "erasePixel error: only for 32 bits/pixel\n";
            }

            for (uint32_t yy = y; yy < y + h; ++yy) {
                for (uint32_t xx = x; xx < x + w; ++xx) {
                    drawPixel(xx,yy,Color(0,0,0,0));
                }
            }
        }

        void eraseRegion(const Point& p, const uint32_t w, const uint32_t h) {
            eraseRegion(p.x, p.y, w, h);
        }

        /*
        void undo() {
            if (undoList.size() > 0) {
                CopyFrom(undoList.back());
                undoList.pop_back();
            } else {
                std::cout << "Nop" << undoList.size() << std::endl;
            }
        }
        */
    private:
        BMP* image;
        /*
        std::vector<BMP> undoList;
        
        void addItemChng(const bool isUndo) {
            if (isUndo) {
                if (undoList.size() < MaxList) {
                    BMP aux = CreateCopy();
                    undoList.push_back(aux);
                }
            } else {
                if (redoList.size() < MaxList) {
                    BMP aux = CreateCopy();
                    redoList.push_back(aux);
                }
            }   
        }*/
};