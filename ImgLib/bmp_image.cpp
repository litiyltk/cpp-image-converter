#include "bmp_image.h"
#include "pack_defines.h"

#include <array>
#include <fstream>
#include <string_view>

using namespace std;

namespace img_lib {

constexpr uint16_t BMP_SIG = 0x4D42; // Подпись "BM"
constexpr uint16_t BMP_BITS_PER_PIXEL = 24; // По 8 бит на компоненту цвета
constexpr uint32_t BMP_RESERVED = 0; // Зарезервированное пространство заполненно нулями
constexpr uint32_t BMP_HEADER_SIZE = 40; // Информационный заголовок, 40 байт
constexpr uint16_t BMP_PLANES = 1; // Одна плоскость
constexpr uint32_t BMP_COMPRESSION = 0; // Без сжатия, 0
constexpr int32_t  BMP_PIXELS_PER_METER = 11811; // Разрешение примерно соответствует 300DPI
constexpr uint32_t BMP_COLORS_USED = 0; // Количество использованных цветов, значение не определено 0
constexpr uint32_t BMP_COLORS_IMPORTANT = 0x1000000; // Количество значимых цветов, 0x1000000

constexpr uint32_t BMP_BYTES_PER_PIXEL = 3;  // По 3 байта на на компоненту цвета
constexpr uint32_t BMP_ALIGNMENT = 4;    // Выравнивание по 4 байтам

PACKED_STRUCT_BEGIN BitmapFileHeader {
    uint16_t sign = BMP_SIG; // Подпись BMP файла
    uint32_t file_size; // Суммарный размер заголовка и данных
    uint32_t reserved = BMP_RESERVED; // Зарезервированное пространство
    uint32_t data_offset; // Отступ данных от начала файла
} PACKED_STRUCT_END;

PACKED_STRUCT_BEGIN BitmapInfoHeader {
    uint32_t header_size = BMP_HEADER_SIZE; // Размер заголовка
    int32_t width; // Ширина изображения
    int32_t height; // Высота изображения
    uint16_t planes = BMP_PLANES; // Количество плоскостей
    uint16_t bits_per_pixel = BMP_BITS_PER_PIXEL; // Количество бит на пиксель
    uint32_t compression = BMP_COMPRESSION; // Тип сжатия
    uint32_t data_size; // Размер данных изображения
    int32_t x_pixels_per_meter = BMP_PIXELS_PER_METER; // Горизонтальное разрешение
    int32_t y_pixels_per_meter = BMP_PIXELS_PER_METER; // Вертикальное разрешение
    uint32_t colors_used = BMP_COLORS_USED; // Количество использованных цветов
    uint32_t colors_important = BMP_COLORS_IMPORTANT; // Количество значимых цветов
} PACKED_STRUCT_END;

/*
функция вычисления отступа по ширине
деление, а затем умножение на 4 округляет до числа, кратного четырём
прибавление тройки гарантирует, что округление будет вверх
*/
static int GetBMPStride(int w) {
    return BMP_ALIGNMENT * (BMP_BYTES_PER_PIXEL * (w + 1) / BMP_ALIGNMENT);
}

bool SaveBMP(const Path& file, const Image& image) {
    std::ofstream ofs(file, std::ios::binary);
    BitmapFileHeader file_header;
    BitmapInfoHeader info_header;

    const int w = image.GetWidth();
    const int h = image.GetHeight();
    const int stride = GetBMPStride(w);

    file_header.data_offset = sizeof(file_header) + sizeof(info_header); // отступ равен размеру двух частей заголовка
    info_header.width = w;
    info_header.height = h;
    info_header.data_size = stride * h;
    file_header.file_size = file_header.data_offset + info_header.data_size;

    ofs.write(reinterpret_cast<char*>(&file_header), sizeof(file_header));
    ofs.write(reinterpret_cast<char*>(&info_header), sizeof(info_header));

    std::vector<char> buff(stride);

    for (int y = h - 1; y >= 0; --y) {
        const Color* line = image.GetLine(y);
        int index = 0;
        for (int x = 0; x < w; ++x) { // Порядок B,G,R
            buff[index] = static_cast<char>(line[x].b); 
            buff[index+1] = static_cast<char>(line[x].g); 
            buff[index+2] = static_cast<char>(line[x].r); 
            index += 3;
        }
        ofs.write(buff.data(), stride);
    }

    return ofs.good();
}

Image LoadBMP(const Path& file) {
    std::ifstream ifs(file, std::ios::binary);
    BitmapFileHeader file_header;
    BitmapInfoHeader info_header;

    ifs.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));
    if (!ifs) {
        return {}; // Возврат пустого изображения при ошибке чтения заголовка BMP
    }

    ifs.read(reinterpret_cast<char*>(&info_header), sizeof(info_header));
    if (!ifs) {
        return {}; // Возврат пустого изображения при ошибке чтения информационного заголовка BMP
    }

    if (file_header.sign != BMP_SIG ||
        file_header.reserved != BMP_RESERVED ||
        info_header.header_size != sizeof(info_header) ||
        info_header.bits_per_pixel != BMP_BITS_PER_PIXEL ||
        info_header.planes != BMP_PLANES ||
        info_header.compression != BMP_COMPRESSION ||
        info_header.x_pixels_per_meter != BMP_PIXELS_PER_METER ||
        info_header.y_pixels_per_meter != BMP_PIXELS_PER_METER ||
        info_header.colors_used != BMP_COLORS_USED ||
        info_header.colors_important != BMP_COLORS_IMPORTANT)
        {
        return {}; // Возврат пустого изображения при некорректных заголовках BMP
    }

    const int w = info_header.width;
    const int h = info_header.height;
    const int stride = GetBMPStride(w);
    std::vector<char> buff(stride); // Размер буфера равен отступу
    Image result(w, h, Color::Black());

    for (int y = h - 1; y >= 0; --y) {

        ifs.read(buff.data(), stride); // Читаем строку изображения с учетом отступа
        if (!ifs) { // Возврат пустого изображения при ошибке чтения текущей строки BMP
            return {};
        }

        int index = 0;
        Color* line = result.GetLine(y);
        for (int x = 0; x < w; ++x) { // Порядок B,G,R
            line[x].b = static_cast<byte>(buff[index]);
            line[x].g = static_cast<byte>(buff[index+1]);
            line[x].r = static_cast<byte>(buff[index+2]);
            index += 3;
        }
    }

    return result;
}

}  // namespace img_lib