#include "bmp_image.h"
#include "pack_defines.h"

#include <array>
#include <fstream>
#include <string_view>

using namespace std;

namespace img_lib {

constexpr uint16_t BMP_SIG = 0x4D42; // Подпись "BM"

PACKED_STRUCT_BEGIN BitmapFileHeader {
    uint16_t sign = BMP_SIG; // Подпись BMP файла
    uint32_t file_size; // Суммарный размер заголовка и данных
    uint32_t reserved = 0; // Зарезервированное пространство, заполненное нулями
    uint32_t data_offset; // Отступ данных от начала файла
} PACKED_STRUCT_END;

PACKED_STRUCT_BEGIN BitmapInfoHeader {
    uint32_t header_size = 40; // Размер заголовка
    int32_t width; // Ширина изображения
    int32_t height; // Высота изображения
    uint16_t planes = 1; // Количество плоскостей, 1
    uint16_t bits_per_pixel = 24; // Количество бит на пиксель, 24 (по 8 бит на компоненту цвета)
    uint32_t compression = 0; // Тип сжатия, считаем равным 0
    uint32_t data_size; // Размер данных изображения
    int32_t x_pixels_per_meter = 11811; // Горизонтальное разрешение, 300DPI
    int32_t y_pixels_per_meter = 11811; // Вертикальное разрешение, 300DPI
    uint32_t colors_used = 0; // Количество использованных цветов, значение не определено 0
    uint32_t colors_important = 0x1000000; // Количество значимых цветов, 0x1000000
} PACKED_STRUCT_END;

/*
функция вычисления отступа по ширине
деление, а затем умножение на 4 округляет до числа, кратного четырём
прибавление тройки гарантирует, что округление будет вверх
*/
static int GetBMPStride(int w) {
    return 4 * ((w * 3 + 3) / 4);
}

bool SaveBMP(const Path& file, const Image& image) {
    std::ofstream ofs(file, std::ios::binary);
    BitmapFileHeader file_header;
    BitmapInfoHeader info_header;

    file_header.data_offset = sizeof(file_header) + sizeof(info_header); // отступ равен размеру двух частей заголовка
    info_header.width = image.GetWidth();
    info_header.height = image.GetHeight();
    info_header.data_size = GetBMPStride(info_header.width) * info_header.height;
    file_header.file_size = file_header.data_offset + info_header.data_size;

    ofs.write(reinterpret_cast<char*>(&file_header), sizeof(file_header));
    ofs.write(reinterpret_cast<char*>(&info_header), sizeof(info_header));

    const int w = info_header.width;
    const int h = info_header.height;
    std::vector<char> buff(GetBMPStride(w));

    for (int y = h - 1; y >= 0; --y) {
        const Color* line = image.GetLine(y);
        int index = 0;
        for (int x = 0; x < w; ++x) { // Порядок B,G,R
            buff[index] = static_cast<char>(line[x].b); 
            buff[index+1] = static_cast<char>(line[x].g); 
            buff[index+2] = static_cast<char>(line[x].r); 
            index += 3;
        }
        ofs.write(buff.data(), buff.size());
    }

    return ofs.good();
}

Image LoadBMP(const Path& file) {
    std::ifstream ifs(file, std::ios::binary);
    BitmapFileHeader file_header;
    BitmapInfoHeader info_header;
    int w, h;

    ifs.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));
    ifs.read(reinterpret_cast<char*>(&info_header), sizeof(info_header));

    if (file_header.sign != BMP_SIG ||
        info_header.header_size != sizeof(info_header) ||
        info_header.bits_per_pixel != 24 ||
        info_header.compression != 0) {
        return {};
    }

    int stride = GetBMPStride(info_header.width);

    Image result(info_header.width, info_header.height, Color::Black());
    std::vector<char> buff(stride); // Размер буфера равен отступу

    for (int y = info_header.height - 1; y >= 0; --y) {
        ifs.read(buff.data(), stride); // Читаем строку изображения с учетом отступа

        int index = 0;
        Color* line = result.GetLine(y);
        for (int x = 0; x < info_header.width; ++x) { // Порядок B,G,R
            line[x].b = static_cast<byte>(buff[index]);
            line[x].g = static_cast<byte>(buff[index+1]);
            line[x].r = static_cast<byte>(buff[index+2]);
            index += 3;
        }
    }

    return result;
}

}  // namespace img_lib