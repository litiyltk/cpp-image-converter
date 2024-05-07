#include "ppm_image.h"

#include <array>
#include <fstream>
#include <stdio.h>
#include <setjmp.h>

#include <jpeglib.h>

using namespace std;

namespace img_lib {

// структура из примера LibJPEG
struct my_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr* my_error_ptr;

// функция из примера LibJPEG
METHODDEF(void)
my_error_exit (j_common_ptr cinfo) {
    my_error_ptr myerr = (my_error_ptr) cinfo->err;
    (*cinfo->err->output_message) (cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

// В эту функцию вставлен код примера из библиотеки libjpeg.
// Измените его, чтобы адаптировать к переменным file и image.
// Задание качества уберите - будет использовано качество по умолчанию
bool SaveJPEG(const Path& file, const Image& image) {
    struct jpeg_compress_struct cinfo;
    struct my_error_mgr jerr;
    FILE * outfile = nullptr;
    JSAMPROW row_pointer[1] = { nullptr };
    int row_stride = 0;

    // Открытие файла с использованием _wfopen под Visual Studio
#ifdef _MSC_VER
    if ((outfile = _wfopen(file.wstring().c_str(), L"wb")) == NULL) {
#else
    if ((outfile = fopen(file.string().c_str(), "wb")) == NULL) {
#endif
        return false; // В случае ошибки открытия файла возвращаем false
    }

    // Настройка обработчика ошибок
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;

    // Установка точки возврата для обработки ошибок
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_compress(&cinfo);
        fclose(outfile);
        return false; // В случае ошибки возвращаем false
    }

    // Инициализация структуры с настройками сжатия
    jpeg_create_compress(&cinfo);

    // Настройка цели вывода
    jpeg_stdio_dest(&cinfo, outfile);

    // Установка параметров изображения
    cinfo.image_width = image.GetWidth();
    cinfo.image_height = image.GetHeight();
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    // Установка параметров сжатия
    jpeg_set_defaults(&cinfo);

    // Начало сжатия
    jpeg_start_compress(&cinfo, TRUE);

    // Получение размера строки изображения
    row_stride = image.GetWidth() * 3;

    // Выделение памяти для временного буфера
    std::vector<JSAMPLE> buffer(row_stride);

    // Сжатие и запись изображения
    while (cinfo.next_scanline < cinfo.image_height) {
        // Конвертация строки изображения в JSAMPLE
        for (int x = 0; x < image.GetWidth(); ++x) {
            const Color& pixel = image.GetPixel(x, cinfo.next_scanline);
            buffer[x * 3] = static_cast<JSAMPLE>(pixel.r);
            buffer[x * 3 + 1] = static_cast<JSAMPLE>(pixel.g);
            buffer[x * 3 + 2] = static_cast<JSAMPLE>(pixel.b);
        }
        // Установка указателя на строку для библиотеки libjpeg
        row_pointer[0] = buffer.data();
        // Запись строки
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    // Завершение сжатия
    jpeg_finish_compress(&cinfo);
    // Освобождение ресурсов
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);

    return true; // В случае успеха возвращаем true
}

// тип JSAMPLE фактически псевдоним для unsigned char
void SaveSсanlineToImage(const JSAMPLE* row, int y, Image& out_image) {
    Color* line = out_image.GetLine(y);
    for (int x = 0; x < out_image.GetWidth(); ++x) {
        const JSAMPLE* pixel = row + x * 3;
        line[x] = Color{byte{pixel[0]}, byte{pixel[1]}, byte{pixel[2]}, byte{255}};
    }
}

Image LoadJPEG(const Path& file) {
    jpeg_decompress_struct cinfo;
    my_error_mgr jerr;
    
    FILE* infile;
    JSAMPARRAY buffer;
    int row_stride;

    // Тут не избежать функции открытия файла из языка C,
    // поэтому приходится использовать конвертацию пути к string.
    // Под Visual Studio это может быть опасно, и нужно применить
    // нестандартную функцию _wfopen
#ifdef _MSC_VER
    if ((infile = _wfopen(file.wstring().c_str(), "rb")) == NULL) {
#else
    if ((infile = fopen(file.string().c_str(), "rb")) == NULL) {
#endif
        return {};
    }

    /* Шаг 1: выделяем память и инициализируем объект декодирования JPEG */

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return {};
    }

    jpeg_create_decompress(&cinfo);

    /* Шаг 2: устанавливаем источник данных */

    jpeg_stdio_src(&cinfo, infile);

    /* Шаг 3: читаем параметры изображения через jpeg_read_header() */

    (void) jpeg_read_header(&cinfo, TRUE);

    /* Шаг 4: устанавливаем параметры декодирования */

    // установим желаемый формат изображения
    cinfo.out_color_space = JCS_RGB;
    cinfo.output_components = 3;

    /* Шаг 5: начинаем декодирование */

    (void) jpeg_start_decompress(&cinfo);
    
    row_stride = cinfo.output_width * cinfo.output_components;
    
    buffer = (*cinfo.mem->alloc_sarray)
                ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    /* Шаг 5a: выделим изображение ImgLib */
    Image result(cinfo.output_width, cinfo.output_height, Color::Black());

    /* Шаг 6: while (остаются строки изображения) */
    /*                     jpeg_read_scanlines(...); */

    while (cinfo.output_scanline < cinfo.output_height) {
        int y = cinfo.output_scanline;
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);

        SaveSсanlineToImage(buffer[0], y, result);
    }

    /* Шаг 7: Останавливаем декодирование */

    (void) jpeg_finish_decompress(&cinfo);

    /* Шаг 8: Освобождаем объект декодирования */

    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return result;
}

} // of namespace img_lib