#include <img_lib.h>
#include <jpeg_image.h>
#include <ppm_image.h>
#include <bmp_image.h>

#include <filesystem>
#include <string_view>
#include <iostream>

using namespace std;

// абстрактный класс для поддержки различных форматов файлов
class ImageFormatInterface {
public:
    virtual bool SaveImage(const img_lib::Path& file, const img_lib::Image& image) const = 0;
    virtual img_lib::Image LoadImage(const img_lib::Path& file) const = 0;
};

namespace format_interfaces {

    // производный класс для поддержки формата PPM
    class ImagePPM : public ImageFormatInterface {
    public:
        bool SaveImage(const img_lib::Path& file, const img_lib::Image& image) const override {
            return img_lib::SavePPM(file, image);
        }

        img_lib::Image LoadImage(const img_lib::Path& file) const override {
            return img_lib::LoadPPM(file);
        }
    };

    // производный класс для поддержки формата JPEG
    class ImageJPEG : public ImageFormatInterface {
    public:
        bool SaveImage(const img_lib::Path& file, const img_lib::Image& image) const override {
            return img_lib::SaveJPEG(file, image);
        }

        img_lib::Image LoadImage(const img_lib::Path& file) const override {
            return img_lib::LoadJPEG(file);
        }
    };

    // производный класс для поддержки формата BMP
    class ImageBMP : public ImageFormatInterface {
    public:
        bool SaveImage(const img_lib::Path& file, const img_lib::Image& image) const override {
            return img_lib::SaveBMP(file, image);
        }

        img_lib::Image LoadImage(const img_lib::Path& file) const override {
            return img_lib::LoadBMP(file);
        }
    };

} // namespace format_interfaces

// поддерживаемые форматы файлов
enum class Format {
    JPEG,
    PPM,
    BMP,
    UNKNOWN
};

// возвращает формат файла или Format::UNKNOWN, если формат не удалось определить
Format GetFormatByExtension(const img_lib::Path& input_file) {
    const string ext = input_file.extension().string();
    if (ext == ".jpg"sv || ext == ".jpeg"sv) {
        return Format::JPEG;
    }

    if (ext == ".ppm"sv) {
        return Format::PPM;
    }

    if (ext == ".bmp"sv) {
        return Format::BMP;
    }

    return Format::UNKNOWN;
}

// возвращает указатель на интерфейс нужного формата или nullptr, если формат не удалось определить
const ImageFormatInterface* GetFormatInterface(const img_lib::Path& path) {
    Format format = GetFormatByExtension(path);

    static const format_interfaces::ImageJPEG jpegInterface;
    static const format_interfaces::ImagePPM ppmInterface;
    static const format_interfaces::ImageBMP bmpInterface;

    switch (format) {
        case Format::JPEG:
            return &jpegInterface;
        case Format::PPM:
            return &ppmInterface;
        case Format::BMP:
            return &bmpInterface;
        default:
            return nullptr;
    }
}

int main(int argc, const char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: "sv << argv[0] << " <in_file> <out_file>"sv << std::endl;
        return 1;
    }

    // Получаем пути к файлам
    img_lib::Path in_path = argv[1];
    img_lib::Path out_path = argv[2];

    // Получаем форматы файлов
    Format in_format = GetFormatByExtension(in_path);
    Format out_format = GetFormatByExtension(out_path);

    // Создаем объекты интерфейсов для форматов файлов
    const ImageFormatInterface* in_interface = GetFormatInterface(in_path);
    const ImageFormatInterface* out_interface = GetFormatInterface(out_path);

    // Неизвестный формат входного файла — Unknown format of the input file. Код возврата 2
    if (!in_interface) {
        std::cerr << "Unknown format of the input file"sv << std::endl;
        return 2;
    }

    // Неизвестный формат выходного файла — Unknown format of the output file. Код возврата 3
    if (!out_interface) {
        std::cerr << "Unknown format of the output file"sv << std::endl;
        return 3;
    }

    // Загружаем изображение
    img_lib::Image image = in_interface->LoadImage(in_path);
    if (!image) {
        std::cerr << "Loading failed"sv << std::endl;
        return 4;
    }

    // Сохраняем изображение
    if (!out_interface->SaveImage(out_path, image)) {
        std::cerr << "Saving failed"sv << std::endl;
        return 5;
    }

    std::cout << "Successfully converted"sv << std::endl;
}