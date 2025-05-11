// MaskUtils.hpp
#pragma once

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

// A simple bounding box mask
struct AssetMask {
    int xMin, yMin, xMax, yMax;
};

// Load all PNGs, extract their alpha channels, OR them together,
// then compute the tight bounding box of the non-zero region.
inline AssetMask computeAssetMask(const std::vector<std::string>& framePaths) {
    cv::Mat combined;
    for (auto& p : framePaths) {
        cv::Mat img = cv::imread(p, cv::IMREAD_UNCHANGED);
        CV_Assert(!img.empty() && img.channels()==4);
        std::vector<cv::Mat> channels;
        cv::split(img, channels);
        // channels[3] is the alpha
        cv::Mat bw;
        cv::threshold(channels[3], bw, 1, 255, cv::THRESH_BINARY);
        if (combined.empty()) combined = bw;
        else                 cv::bitwise_or(combined, bw, combined);
    }
    // find bounding rect
    cv::Rect bb = cv::boundingRect(combined);
    return { bb.x, bb.y, bb.x+bb.width-1, bb.y+bb.height-1 };
}

// Dilate the mask by `range` pixels (interaction radius), then return the largest contour.
inline std::vector<cv::Point> computeInteractionArea(const AssetMask& mask,
                                                     const std::vector<std::string>& framePaths,
                                                     int range)
{
    // Recreate binary mask mat
    cv::Mat combined;
    for (auto& p : framePaths) {
        cv::Mat img = cv::imread(p, cv::IMREAD_UNCHANGED);
        std::vector<cv::Mat> c;
        cv::split(img,c);
        cv::Mat bw; cv::threshold(c[3], bw, 1,255,cv::THRESH_BINARY);
        if (combined.empty()) combined = bw;
        else                 cv::bitwise_or(combined,bw,combined);
    }

    // Dilate
    cv::Mat dilated;
    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE,
        cv::Size(range*2+1, range*2+1)
    );
    cv::dilate(combined, dilated, kernel);

    // Find external contours
    std::vector<std::vector<cv::Point>> cs;
    cv::findContours(dilated, cs, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (cs.empty()) return {};
    // Return the largest
    return *std::max_element(cs.begin(), cs.end(),
        [](auto &a, auto &b){ return cv::contourArea(a) < cv::contourArea(b); });
}

// Crop the bottom `threshold` fraction of the mask and return its contour.
inline std::vector<cv::Point> computeCollisionBoundary(const AssetMask& mask,
                                                       const std::vector<std::string>& framePaths,
                                                       double threshold)
{
    // Recreate binary mask mat
    cv::Mat combined;
    for (auto& p : framePaths) {
        cv::Mat img = cv::imread(p, cv::IMREAD_UNCHANGED);
        std::vector<cv::Mat> c; cv::split(img,c);
        cv::Mat bw; cv::threshold(c[3], bw, 1,255,cv::THRESH_BINARY);
        if (combined.empty()) combined = bw;
        else                 cv::bitwise_or(combined,bw,combined);
    }

    int cropLine = int(combined.rows * (1.0 - threshold));
    cv::Mat cropped = cv::Mat::zeros(combined.size(), combined.type());
    combined(cv::Range(cropLine, combined.rows), cv::Range::all()).copyTo(
        cropped(cv::Range(cropLine, combined.rows), cv::Range::all())
    );

    std::vector<std::vector<cv::Point>> cs;
    cv::findContours(cropped, cs, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (cs.empty()) return {};
    return *std::max_element(cs.begin(), cs.end(),
        [](auto &a, auto &b){ return cv::contourArea(a) < cv::contourArea(b); });
}
