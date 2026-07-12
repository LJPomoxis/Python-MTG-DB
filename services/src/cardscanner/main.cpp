#include "common/utils.hpp"
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>
#include <vector>
#include <fstream>

cv::Mat processForOCR(cv::Mat roi, bool debug = false);
void runOCR(cv::Mat processed, const char* whitelist);
bool saveImg(cv::Mat img, std::string name);

int main(int argc, char** argv) {

    std::ofstream file("/var/www/mtgwebapp/test/run.log");
    if (file.is_open()) {
        file.close();
        std::remove("/var/www/mtgwedapp/test/run.log");
    }

    std::string path = "/var/www/mtgwebapp/test/";
    int numTests = argc >= 2 ? std::stoi(argv[1]) : 10;
    std::string baseName = argc >= 3 ? argv[2] : path + "test";
    int x = argc >= 4 ? std::stoi(argv[3]) : 40;
    int y = argc >= 5 ? std::stoi(argv[4]) : 635;
    int w = argc >= 6 ? std::stoi(argv[5]) : 55;
    int h = argc >= 7 ? std::stoi(argv[6]) : 14;

    if (numTests < 1) {
        numTests = 1;
    }
    if (numTests > 10) {
        numTests = 10;
    }

    cv::Rect numRoi(x, y, w, h);           // Offset of card number
    cv::Rect codeRoi(x-10, y+12, w-22, h); // Offset of set code

    for (int i=0; i < numTests; i++) {
        std::string fileName = baseName + std::to_string(i) + ".jpg";

        cv::Mat src = cv::imread( cv::samples::findFile(fileName), cv::IMREAD_COLOR);
    
        if (src.empty()) {
            std::string err = "Failed to load image #" + std::to_string(i);
            utils::log_error(err);
            continue;
        }

        cv::Mat numImg = processForOCR(src(numRoi));
        runOCR(numImg, "0123456789");

        cv::Mat codeImg = processForOCR(src(codeRoi));
        runOCR(codeImg, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

        std::string numName = "num" + std::to_string(i) + ".jpg";
        std::string codeName = "code" + std::to_string(i) + ".jpg";

        if (!saveImg(numImg, "num.jpg")) utils::log_error("Failed to save card num");
        if (!saveImg(codeImg, "code.jpg")) utils::log_error("Failed to save card code");
    }

    return 0;
}

cv::Mat processForOCR(cv::Mat roi, bool debug) {

    cv::Mat gray;
    cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);

    cv::Mat inverted;
    cv::bitwise_not(gray, inverted);

    // Based on px character height in image
    // Will need adjusting for final inputs
    int sourceCharHeight = 9;
    double targetCharHeight = 35.0;
    double scale = targetCharHeight / sourceCharHeight;

    cv::Mat upscaled;
    cv::resize(inverted, upscaled, cv::Size(), scale, scale, cv::INTER_CUBIC);

    cv::Mat denoised;
    cv::GaussianBlur(upscaled, denoised, cv::Size(5, 5), 0);
    //cv::bilateralFilter(upscaled, denoised, 5, 40, 40);

    cv::Mat binary;
    cv::threshold(denoised, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    //cv::adaptiveThreshold(denoised, binary, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
    //                      cv::THRESH_BINARY, 25, -5);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);

    cv::Mat bordered;
    cv::copyMakeBorder(binary, bordered, 15, 15, 15, 15, cv::BORDER_CONSTANT, cv::Scalar(255));

    return bordered;
}

void runOCR(cv::Mat processed, const char* whitelist) {
    tesseract::TessBaseAPI *ocr = new tesseract::TessBaseAPI();

    std::ofstream outFile("/var/www/mtgwebapp/test/run.log", std::ios::app);

    if (ocr->Init(NULL, "eng", tesseract::OEM_LSTM_ONLY)) {
        utils::log_error("Tesseract init failed");
        return;
    }

    ocr->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    ocr->SetVariable("tessedit_char_whitelist", whitelist);

    ocr->SetImage(processed.data, processed.cols, processed.rows, 1, processed.step);
    std::string outText = std::string(ocr->GetUTF8Text());
    outFile << outText;

    ocr->Recognize(NULL);

    int confidence = ocr->MeanTextConf();
    outFile << "Line Confidence: " << confidence << "\n";

    tesseract::ResultIterator* resi = ocr->GetIterator();
    std::vector<double> confVec;
    if (resi != NULL) {
        do {
            const char* symbol = resi->GetUTF8Text(tesseract::RIL_SYMBOL);
            float conf = resi->Confidence(tesseract::RIL_SYMBOL);
            if (symbol != NULL) {
                outFile << symbol << "  | ";
                confVec.push_back(double(conf));
                delete[] symbol;
            }
        } while (resi->Next(tesseract::RIL_SYMBOL));

        outFile << std::endl;

        for (const auto& c : confVec) {
            outFile << c << " | ";
        }
        outFile << "\n";
        delete resi;
    }

    outFile << std::endl;
    ocr->End();
    delete ocr;
    outFile.close();
}

bool saveImg(cv::Mat img, std::string name) {
    std::string file = "/var/www/mtgwebapp/test/" + name;
    return cv::imwrite(file, img);
}