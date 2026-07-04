#include "common/utils.hpp"
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>

cv::Mat processForOCR(cv::Mat roi, bool debug = false);
void runOCR(cv::Mat processed, const char* whitelist);
bool saveImg(cv::Mat img, std::string name);

int main(int argc, char** argv) {

    std::string path = "/var/www/mtgwebapp/test/";
    std::string imageName = argc >= 2 ? argv[1] : path + "test.jpg";
    int x = argc >= 3 ? std::stoi(argv[2]) : 40;
    int y = argc >= 4 ? std::stoi(argv[3]) : 635;
    int w = argc >= 5 ? std::stoi(argv[4]) : 55;
    int h = argc >= 6 ? std::stoi(argv[5]) : 12;

    cv::Rect numRoi(x, y, w, h);           // Offset of card number
    cv::Rect codeRoi(x-10, y+12, w-25, h); // Offset of set code
    cv::Mat src = cv::imread( cv::samples::findFile(imageName), cv::IMREAD_COLOR);
    
    if (src.empty()) {
        utils::log_error("Failed to load image");
        return 1;
    }

    cv::Mat numImg = processForOCR(src(numRoi));
    runOCR(numImg, "0123456789");

    cv::Mat codeImg = processForOCR(src(codeRoi));
    runOCR(codeImg, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    if (!saveImg(numImg, "num.jpg")) utils::log_error("Failed to save card num");
    if (!saveImg(codeImg, "code.jpg")) utils::log_error("Failed to save card code");

    return 0;
}

cv::Mat processForOCR(cv::Mat roi, bool debug) {

    cv::Mat inverted, resized, thresh, morphed;

    cv::bitwise_not(roi, inverted);
    if (debug) saveImg(inverted, "debug_1_inverted.jpg");
    cv::cvtColor(inverted, inverted, cv::COLOR_BGR2GRAY);
    if (debug) saveImg(inverted, "debug_2_grayscale.jpg");

    double scale = 8.0; // This results in 96px of height which tesseract won't scale for processing
    cv::resize(inverted, resized, cv::Size(), scale, scale, cv::INTER_CUBIC);
    if (debug) saveImg(resized, "debug_3_resized.jpg");

    cv::Mat kernel = (cv::Mat_<float>(3, 3) << 0, -1,  0, -1,  5, -1, 0, -1,  0);
    cv::filter2D(resized, resized, -1, kernel);
    if (debug) saveImg(resized, "debug_4_sharpened.jpg");

    cv::threshold(resized, thresh, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    if (debug) saveImg(thresh, "debug_5_thresh.jpg");

    cv::Mat crossKernel = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(2, 1));
    cv::erode(thresh, morphed, crossKernel, cv::Point(-1, -1), 1);
    if (debug) saveImg(morphed, "debug_6_crossErode.jpg");

    return morphed;
}

void runOCR(cv::Mat processed, const char* whitelist) {
    tesseract::TessBaseAPI *ocr = new tesseract::TessBaseAPI();

    if (ocr->Init(NULL, "eng", tesseract::OEM_LSTM_ONLY)) {
        utils::log_error("Tesseract init failed");
        return;
    }

    ocr->SetPageSegMode(tesseract::PSM_RAW_LINE);
    ocr->SetVariable("tessedit_char_whitelist", whitelist);

    ocr->SetImage(processed.data, processed.cols, processed.rows, 1, processed.step);
    std::string outText = std::string(ocr->GetUTF8Text());
    utils::log_info(outText);

    ocr->Recognize(NULL);

    int confidence = ocr->MeanTextConf();
    std::cout << "Line Confidence: " << confidence << std::endl;

    tesseract::ResultIterator* ri = ocr->GetIterator();
    if (ri != NULL) {
        do {
            const char* symbol = ri->GetUTF8Text(tesseract::RIL_SYMBOL);
            float conf = ri->Confidence(tesseract::RIL_SYMBOL);
            if (symbol != NULL) {
                std::cout << "Symbol: " << symbol << " | Confidence: " << conf << std::endl;
                delete[] symbol;
            }
        } while (ri->Next(tesseract::RIL_SYMBOL));
        delete ri;
    }

    ocr->End();
    delete ocr;
}

bool saveImg(cv::Mat img, std::string name) {
    std::string file = "/var/www/mtgwebapp/test/" + name;
    return cv::imwrite(file, img);
}