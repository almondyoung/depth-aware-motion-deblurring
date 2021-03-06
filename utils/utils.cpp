#include <cmath>

#include "utils.hpp"

using namespace cv;
using namespace std;

namespace deblur {

    float crossCorrelation(Mat& X, Mat& Y, const Mat& mask) {
        assert(X.type() == CV_32F && "works on float images");
        assert(X.size() == Y.size() && "both image have the same size");

        Mat region;
        if (mask.empty()) {
            // mask with ones of image size
            region = Mat::ones(X.size(), CV_8U);
        } else {
            region = mask;
        }

        assert(region.type() == CV_8U && "works with on grayvalue mask");


        // compute mean of the matrices
        // use just the pixel inside the mask
        float meanX = mean(X, mask)[0];
        float meanY = mean(Y, mask)[0];

        float E = 0;

        // deviation = sqrt(1/N * sum_x(x - μx)²) -> do not use 1/N 
        float deviationX = 0;
        float deviationY = 0;

        assert(X.size() == Y.size() && "images of same size");
        
        // go through each gradient map and
        // compute the sums in the computation of expedted values and deviations
        for (int row = 0; row < X.rows; row++) {
            for (int col = 0; col < X.cols; col++) {
                // compute if inside mask
                if (region.at<uchar>(row, col) > 0) {
                    float valueX = X.at<float>(row, col) - meanX;
                    float valueY = Y.at<float>(row, col) - meanY;

                    // expected values (the way matlab calculates it)              
                    E += valueX * valueY;

                    // deviation
                    deviationX += (valueX * valueX);
                    deviationY += (valueY * valueY);
                }
            }
        }
           
        deviationX = sqrt(deviationX);
        deviationY = sqrt(deviationY);

        return E / (deviationX * deviationY);
    }

    void conv2(const Mat& src, Mat& dst, const Mat& kernel, ConvShape shape) {
        int padSizeX = kernel.cols - 1;
        int padSizeY = kernel.rows - 1;

        Mat zeroPadded;
        copyMakeBorder(src, zeroPadded, padSizeY, padSizeY, padSizeX, padSizeX,
                       BORDER_CONSTANT, Scalar::all(0));
        
        Point anchor(0, 0);

        // openCV is doing a correlation in their filter2D function ...
        Mat fkernel;
        flip(kernel, fkernel, -1);

        Mat tmp;
        filter2D(zeroPadded, tmp, -1, fkernel, anchor);

        // src =
        //     1 2 3 4
        //     1 2 3 4
        //     1 2 3 4
        // 
        // zeroPadded =
        //     0 0 1 2 3 4 0 0
        //     0 0 1 2 3 4 0 0
        //     0 0 1 2 3 4 0 0
        // 
        // kernel =
        //     0.5 0 0.5
        // 
        // tmp =
        //     0.5 1 2 3 1.5 2 0 2
        //     0.5 1 2 3 1.5 2 0 2
        //     0.5 1 2 3 1.5 2 0 2
        //     |<----------->|      full
        //         |<---->|         same
        //           |-|            valid
        // 
        // the last column is complete rubbish, because openCV's
        // filter2D uses reflected borders (101) by default.
        
        // crop padding
        Mat cropped;

        // variables cannot be declared in case statements
        int width  = -1;
        int height = -1;

        switch(shape) {
            case FULL:
                cropped = tmp(Rect(0, 0,
                                   tmp.cols - padSizeX,
                                   tmp.rows - padSizeY));
                break;

            case SAME:
                cropped = tmp(Rect((tmp.cols - padSizeX - src.cols + 1) / 2,  // +1 for ceil
                                   (tmp.rows - padSizeY - src.rows + 1) / 2,  // +1 for ceil
                                   src.cols,
                                   src.rows));
                break;

            case VALID:
                width  = src.cols - kernel.cols + 1;
                height = src.rows - kernel.rows + 1;
                cropped = tmp(Rect((tmp.cols - padSizeX - width) / 2,
                                   (tmp.rows - padSizeY - height) / 2,
                                   width,
                                   height));
                break;

            default:
                throw runtime_error("Invalid shape");
                break;
        }

        cropped.copyTo(dst);
    }


    /**
     * Just for debugging.
     *
     */
    void test() {
        Mat I = (Mat_<float>(3,4) << 1,2,3,4,1,2,3,4,1,2,3,4);
        cout << endl << "I: " << endl;
        for (int row = 0; row < I.rows; row++) {
            for (int col = 0; col < I.cols; col++) {
                cout << " " << I.at<float>(row, col);
            }
            cout << endl;
        }

        Mat k = (Mat_<float>(1,3) << 0.3, 0, 0.7);
        // Mat k = (Mat_<float>(1,4) << 0.5, 0, 0, 0.5);
        cout << endl << "k: " << endl;
        for (int row = 0; row < k.rows; row++) {
            for (int col = 0; col < k.cols; col++) {
                cout << " " << k.at<float>(row, col);
            }
            cout << endl;
        }

        Mat normal;
        filter2D(I, normal, -1, k);
        cout << endl << "normal (reflected border): " << endl;
        for (int row = 0; row < normal.rows; row++) {
            for (int col = 0; col < normal.cols; col++) {
                cout << " " << normal.at<float>(row, col);
            }
            cout << endl;
        }

        Mat full, same, valid;

        conv2(I, full, k, FULL);
        cout << endl << "full: " << endl;
        for (int row = 0; row < full.rows; row++) {
            for (int col = 0; col < full.cols; col++) {
                cout << " " << full.at<float>(row, col);
            }
            cout << endl;
        }

        conv2(I, same, k, SAME);
        cout << endl << "same: " << endl;
        for (int row = 0; row < same.rows; row++) {
            for (int col = 0; col < same.cols; col++) {
                cout << " " << same.at<float>(row, col);
            }
            cout << endl;
        }

        conv2(I, valid, k, VALID);
        cout << endl << "valid: " << endl;
        for (int row = 0; row < valid.rows; row++) {
            for (int col = 0; col < valid.cols; col++) {
                cout << " " << valid.at<float>(row, col);
            }
            cout << endl;
        }
    }


    void fft(const Mat& src, Mat& dst) {
        if (src.type() == CV_32F) {
            // for fast DFT expand image to optimal size
            Mat padded;
            int m = getOptimalDFTSize( src.rows );
            int n = getOptimalDFTSize( src.cols );

            // on the border add zero pixels
            copyMakeBorder(src, padded, 0, m - src.rows, 0, n - src.cols,
                               BORDER_CONSTANT, Scalar::all(0));

            // add to the expanded real plane another imagniary plane with zeros
            Mat planes[] = {padded,
                                Mat::zeros(padded.size(), CV_32F)};
            merge(planes, 2, dst);
        } else if (src.type() == CV_32FC2) {
            src.copyTo(dst);
        } else {
            assert(false && "fft works on 32FC1- and 32FC1-images");
        }

        // this way the result may fit in the source matrix
        // 
        // DFT_COMPLEX_OUTPUT suppress to creation of a dense CCS matrix
        // but we want a simple complex matrix
        dft(dst, dst, DFT_COMPLEX_OUTPUT);

        // assert(padded.size() == dst.size() && "Resulting complex matrix must be of same size");
    }


    void dft(const Mat& src, Mat& dst){
        // create complex matrix
        Mat planes[] = {src,
                        Mat::zeros(src.size(), CV_32F)};
        merge(planes, 2, dst);

        // DFT_COMPLEX_OUTPUT suppress to creation of a dense CCS matrix
        // but we want a simple complex matrix
        dft(dst, dst, DFT_COMPLEX_OUTPUT);
    }


    void convertFloatToUchar(const Mat& src, Mat& dst) {
        // find min and max value
        double min; double max;
        minMaxLoc(src, &min, &max);

        // check if negativ values has to be handeled
        if (min >= 0 && max < 1) {
            // if the matrix is in the range [0, 1] just scale with 255
            src.convertTo(dst, CV_8U, 255.0);
        } else {
            // don't work on the original src
            Mat copy;
            src.copyTo(copy);

            // handling that floats could be negative
            copy -= min;

            // new mininum and maximum
            minMaxLoc(copy, &min, &max);

            // convert
            if (max < 1) {
                copy.convertTo(dst, CV_8U, 255.0);
            } else {
                // FIXME: what if max - min == 0 ?
                copy.convertTo(dst, CV_8U, 255.0/(max-min));
            }
        }
    }


    void showFloat(const string name, const Mat& src, const bool write){
        assert(src.type() == CV_32F && "works for single channel ");

        Mat srcUchar;
        convertFloatToUchar(src, srcUchar);
        imshow(name, srcUchar);

        if (write){
            string filename = name + ".png";
            imwrite(filename, srcUchar);
        }
    }


    void showGradients(const string name, const Mat& src, const bool write){
        assert(src.type() == CV_32F && "works for single channel ");

        Mat srcUchar;
        // find min and max value
        double min; double max;
        minMaxLoc(src, &min, &max);

        // don't work on the original src
        Mat copy;
        src.copyTo(copy);

        // handling that floats could be negative
        copy -= min;

        // new mininum and maximum
        minMaxLoc(copy, &min, &max);

        // convert
        copy.convertTo(srcUchar, CV_8U, 255.0/(max-min));

        imshow(name, srcUchar);

        if (write){
            string filename = name + ".png";
            imwrite(filename, srcUchar);
        }
    }


    void swapQuadrants(Mat& image) {
        // rearrange the quadrants of Fourier image  so that the origin is at the image center
        int cx = image.cols/2;
        int cy = image.rows/2;

        Mat q0(image, Rect(0, 0, cx, cy));   // Top-Left - Create a ROI per quadrant
        Mat q1(image, Rect(cx, 0, cx, cy));  // Top-Right
        Mat q2(image, Rect(0, cy, cx, cy));  // Bottom-Left
        Mat q3(image, Rect(cx, cy, cx, cy)); // Bottom-Right

        Mat tmp;                           // swap quadrants (Top-Left with Bottom-Right)
        q0.copyTo(tmp);
        q3.copyTo(q0);
        tmp.copyTo(q3);

        q1.copyTo(tmp);                    // swap quadrant (Top-Right with Bottom-Left)
        q2.copyTo(q1);
        tmp.copyTo(q2);
    }


    void showComplexImage(const string windowName, const Mat& complex) {
        // compute the magnitude and switch to logarithmic scale
        // => log(1 + sqrt(Re(DFT(I))^2 + Im(DFT(I))^2))
        Mat planes[] = {Mat::zeros(complex.size(), CV_32F), Mat::zeros(complex.size(), CV_32F)};
        split(complex, planes);                   // planes[0] = Re(DFT(I), planes[1] = Im(DFT(I))
        magnitude(planes[0], planes[1], planes[0]);// planes[0] = magnitude
        Mat magI = planes[0];

        magI += Scalar::all(1);                    // switch to logarithmic scale
        log(magI, magI);

        // crop the spectrum, if it has an odd number of rows or columns
        magI = magI(Rect(0, 0, magI.cols & -2, magI.rows & -2));

        swapQuadrants(magI);

        normalize(magI, magI, 0, 1, CV_MINMAX); // Transform the matrix with float values into a
                                                // viewable image form (float between values 0 and 1).

        imshow(windowName, magI);
    }


    void normalizeOne(Mat& src, Mat& dst) {
        if (src.channels() == 1) {
            double min, max;
            minMaxLoc(src, &min, &max);
            const double scale = std::max(abs(min), abs(max));
            normalize(src, dst, min / scale, max / scale, NORM_MINMAX);
        } else if (src.channels() == 2) {
            vector<Mat> channels;
            vector<Mat> tmp(2);

            split(src, channels);
            normalizeOne(channels, tmp);
            merge(tmp, dst);

        } else {
            assert(false && "Input must have 1- or 2-channels");
        }
    }


    void normedGradients(array<Mat, 2>& gradients, Mat& gradient) {
        gradient = Mat::zeros(gradients[0].size(), CV_32F);

        for (int row = 0; row < gradient.rows; row++) {
            for (int col = 0; col < gradient.cols; col++) {
                float value = gradients[0].at<float>(row, col) * gradients[0].at<float>(row, col) 
                              + gradients[1].at<float>(row, col) * gradients[1].at<float>(row, col);
                gradient.at<float>(row, col) = sqrt(value);
            }
        }
    }


    Mat realMat(const Mat& src) {
        assert(src.type() == CV_32FC2 && "Input must be complex floating point matrix");

        Mat planes[] = { Mat::zeros(src.size(), CV_32F),
                         Mat::zeros(src.size(), CV_32F) };

        
        // planes[0] = Re(DFT(I)
        // planes[1] = Im(DFT(I))
        split(src, planes);

        return planes[0];
    }


    void fillPixel(Mat &image, const Point start, const Point end, const uchar color) {
        for (int row = start.y; row <= end.y; row++) {
            for (int col = start.x; col <= end.x; col++) {
                image.at<uchar>(row, col) = color;
            }
        }
    }


    void edgeTaper(Mat& src, Mat& dst, Mat& mask, Mat& image) {
        assert(src.type() == CV_8U && "gray values needed");

        Mat taperedHorizontal;
        src.copyTo(taperedHorizontal);

        // search for black regions
        uchar threshold = 0;

        uchar left = 0;
        Point start(-1,-1);

        // fill the black regions with the gray values
        // go through each pixel
        for (int row = 0; row < src.rows; row++) {
            for (int col = 0; col < src.cols; col++) {
                uchar value = src.at<uchar>(row, col);

                // check if in black region
                if (start != Point(-1, -1)) {
                    // found next colored pixel or reached end of the row
                    if (value > threshold || col == src.cols - 1) {
                        // fill first half of the detected region
                        Point end(col - ((col - start.x) / 2),
                                  row - ((row - start.y) / 2));

                        // if at the start of the row set the color for the 
                        // first half to the same color as for the second one
                        if (start.x == 0) {
                            left = value;
                        }

                        fillPixel(taperedHorizontal, start, end, left);

                        // if at the end of the row set the color for the 
                        // second half to the same color as for the first one
                        if (col == src.cols - 1) {
                            value = left;
                        }

                        // fill second half of the detected region
                        fillPixel(taperedHorizontal, end, Point(col, row), value);
                        
                        // reset the start point of the region
                        start = Point(-1,-1);
                    }
                } else {
                    // found new occluded pixel
                    if (value <= threshold) {
                        // there is no left neighbor at column 0 so check it
                        left = (col > 0) ? src.at<uchar>(row, col - 1) : 0;
                        start = Point(col, row);
                    }
                }
            }
        }

        Mat taperedVertical;
        src.copyTo(taperedVertical);

        left = 0;
        start = Point(-1,-1);

        // second run for vertical filling
        for (int col = 0; col < src.cols; col++) {
            for (int row = 0; row < src.rows; row++) {
                uchar value = src.at<uchar>(row, col);

                // check if in black region
                if (start != Point(-1, -1)) {
                    // found next colored pixel or reached end of the row
                    if (value > threshold || row == src.rows - 1) {
                        // fill first half of the detected region
                        Point end(col - ((col - start.x) / 2),
                                  row - ((row - start.y) / 2));

                        // if at the start of the row set the color for the 
                        // first half to the same color as for the second one
                        if (start.y == 0) {
                            left = value;
                        }

                        fillPixel(taperedVertical, start, end, left);

                        // if at the end of the row set the color for the 
                        // second half to the same color as for the first one
                        if (row == src.rows - 1) {
                            value = left;
                        }

                        // fill second half of the detected region
                        fillPixel(taperedVertical, end, Point(col, row), value);
                        
                        // reset the start point of the region
                        start = Point(-1,-1);
                    }
                } else {
                    // found new occluded pixel
                    if (value <= threshold) {
                        // there is no left neighbor at column 0 so check it
                        left = (row > 0) ? src.at<uchar>(row - 1, col) : 0;
                        start = Point(col, row);
                    }
                }
            }
        }

        // add the horizontal and vertical filled images
        addWeighted(taperedHorizontal, 0.5, taperedVertical, 0.5, 0.0, dst);

        // fill the region within the mask to avoid blurring the inside of the region
        // over its borders (this will reduce the frequency at the border of the region)
        left = 0;
        start = Point(-1,-1);

        for (int row = 0; row < src.rows; row++) {
            for (int col = 0; col < src.cols; col++) {
                uchar value = dst.at<uchar>(row, col);

                // check if inside mask
                if (start != Point(-1, -1)) {
                    // found pixel next to region inside mask or reached end of the row
                    if (mask.at<uchar>(row, col) == 0 || col == src.cols - 1) {
                        // cout << (int)value << endl;
                        // fill first half of the detected region
                        Point end(col - ((col - start.x) / 2),
                                  row - ((row - start.y) / 2));

                        // if at the start of the row set the color for the 
                        // first half to the same color as for the second one
                        if (start.x == 0) {
                            left = value;
                        }

                        fillPixel(dst, start, end, left);

                        // if at the end of the row set the color for the 
                        // second half to the same color as for the first one
                        if (col == src.cols - 1) {
                            value = left;
                        }

                        // fill second half of the detected region
                        fillPixel(dst, end, Point(col, row), value);
                        
                        // reset the start point of the region
                        start = Point(-1,-1);
                    }
                } else {
                    // src inside mask begins
                    if (mask.at<uchar>(row, col) > 0) {
                        // there is no left neighbor at column 0 so check it
                        left = (col > 0) ? dst.at<uchar>(row, col - 1) : 0;
                        start = Point(col, row);
                    }
                }
            }
        }
     
        // add the original image 
        Mat imageGauss;
        GaussianBlur(image, imageGauss, Size(19, 19), 0, 0, BORDER_DEFAULT);
        addWeighted(dst, 0.7, imageGauss, 0.3, 0.0, dst);
        GaussianBlur(dst, dst, Size(51, 51), 0, 0, BORDER_DEFAULT);
        
        src.copyTo(dst, mask);
    }

}
