#include <iostream>
#include <stdio.h>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "coins.hpp"

using namespace std;
using namespace cv;

const int PIECE_10(10);
const int PIECE_20(20);
const int PIECE_50(50);
const int PIECE_100(100);
const int PIECE_200(200);
const int PIECE_500(500);
const int PIECE_1000(1000);

int getPiece(int);
int *getValue(int, int);

double coinsRapport[7][7] = 
        {{0, 0, 0, 0, 0, 0, 0},
        {0.9523809524, 0, 0, 0, 0, 0, 0},
        {0.8695652174, 1.9130434783, 0, 0, 0, 0, 0},
        {0.8333333333, 0.875, 0.9583333333, 0, 0, 0, 0},
        {0.8, 0.84, 0.92, 0.96, 0, 0, 0},
        {0.7692307692, 0.8076923077, 0.8846153846, 0.9230769231, 0.9615384615, 0, 0},
        {0.7407407407, 0.7777777778, 0.8518518519, 0.8888888889, 0.9259259259, 0.962962963, 0}};


int *getValue(int r1, int r2) {
    double r = (double)r1/r2;
    int a, b;
    double min = 2;
    for (int i = 1; i < 7; i++) {
        for (int j = 0; j < i; j++) {
            double m = (r - coinsRapport[i][j]) > 0 ? r - coinsRapport[i][j] : coinsRapport[i][j] - r;
            if (m < min ) {
                cout << m << endl;
                min = m;
                a = i; b = j;
            }
        }
    }
    int result[2];   
    result[0] = getPiece(a);
    result[1] = getPiece(b);
    return result;
}

int getPiece(int i){
    switch(i) {
        case 0: return PIECE_10; break;
        case 1: return PIECE_50; break;
        case 2: return PIECE_20; break;
        case 3: return PIECE_100; break;
        case 4: return PIECE_500; break;
        case 5: return PIECE_20; break;
        case 6: return PIECE_1000; break;
    }
}
int coins() {
    
    Mat src, src_gray;
    
    src = imread("img/b2.png", cv::IMREAD_COLOR);

    cvtColor(src, src_gray, CV_BGR2GRAY);

    // Rèduire le bruit et éviter le détection fausse de circle
    GaussianBlur(src_gray, src_gray, Size(9, 9), 2, 2);

    vector<Vec3f> circles;

    // Appliqué le hough transforment pour trouver les cercles
    HoughCircles(
            src_gray, // gray image
            circles, // stockage des circle détecté
            CV_HOUGH_GRADIENT, // méthode de detection 
            2, //Inverse ratio
            50, // Minimum distance détecté entre les centers   
            100, // un seuil plus élèvé pour le détecteur canny edge 
            100, // seuil au center 
            15, // rayon minimum
            500 // rayon maximum
            );

    
    // Traces les cercles détectès
    for (int i = 0; i < circles.size(); i++) {
        Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
        int radius = cvRound(circles[i][2]);
        
        circle(src, center, 3, Scalar(0, 255, 0), -1, 8, 0);
        circle(src, center, radius, Scalar(0, 0, 255), 3, 8, 0);
        
        cout << "center " << center << " - radius " << radius << endl;
    }
    
    // Ordonner les circles par le rayon
    for (int i = 0; i < circles.size() - 1; i++) {
        for (int j = i; j < circles.size(); j++) {
            Vec3f circle = circles[i];
            int radius = cvRound(circles[i][2]);
            if (radius > cvRound(circles[j][2])) {
                circles[i] = circles[j];
                circles[j] = circle;
            }
        }
    }
    
    // calculez les valeurs 
    int values[circles.size()];
    for (int i = 0; i < circles.size() ; i++) {
        for (int j = i+1; j < circles.size(); j++) {
            int r1 = circles[i][2];
            int r2 = circles[j][2];
            
           
            int *v = getValue(r1, r2); 
            
            values[i] = v[0];
            values[j] = v[1];
        }
    }
    
    cout << endl << "Resultat:" << endl;
    int result = 0;
    for (int i = 0; i < circles.size(); i++) {
        result += values[i];
        cout << values[i] << endl;
    }
    cout << "Le resultat est : " << result << " cent" << endl << endl;
    
    /// Affiche le resultat
    namedWindow("Count Coins", CV_WINDOW_AUTOSIZE);
    imshow("Count Coins", src);

    waitKey(0);
    return 0;
}
