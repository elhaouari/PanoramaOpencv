#include <iostream>
#include <string>
#include <opencv2/highgui.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/stitching/detail/blenders.hpp>
#include <opencv2/stitching/detail/camera.hpp>
#include <opencv2/stitching/detail/exposure_compensate.hpp>
#include <opencv2/stitching/detail/matchers.hpp>
#include <opencv2/stitching/detail/motion_estimators.hpp>
#include <opencv2/stitching/detail/seam_finders.hpp>
#include <opencv2/stitching/detail/util.hpp>
#include <opencv2/stitching/detail/warpers.hpp>
#include <opencv2/stitching/warpers.hpp>
#include "opencv2/opencv.hpp"   
#include "pannorama.hpp"

using namespace std;
using namespace cv;
using namespace cv::detail;

int pannorama() {

    // Default parameters
    vector<String> img_names;
    double scale = 1;
    float match_conf = 0.3f;
    float conf_thresh = 1.f;
    bool do_wave_correct = true;
    WaveCorrectKind wave_correct_type = WAVE_CORRECT_HORIZ;
    int expos_comp_type = ExposureCompensator::GAIN_BLOCKS;
    float blend_strength = 5;
    string result_name = "panorama_result.jpg";

    // 1-Input images    
    img_names.push_back("img/p11.png");
    img_names.push_back("img/p12.png");
    
    img_names.push_back("img/p21.png");
    img_names.push_back("img/p22.png");
    img_names.push_back("img/p23.png");
    img_names.push_back("img/p31.png");
    img_names.push_back("img/p32.png");
    img_names.push_back("img/p4.png");
    img_names.push_back("img/p5.png");
    
    // Check if have enough images
    int num_images = img_names.size();
    if (num_images < 2) {
        cout << "Need more images" << endl;
        return -1;
    }


    // 2- Resize images and find features steps (SURF)
    Ptr<FeaturesFinder> finder = makePtr<SurfFeaturesFinder>();
    
    Mat full_img, img;
    vector<ImageFeatures> features(num_images);
    vector<Mat> images(num_images);
    vector<Size> full_img_sizes(num_images);
    for (int i = 0; i < num_images; ++i) {
        full_img = imread(img_names[i], cv::IMREAD_COLOR);
        full_img_sizes[i] = full_img.size();
        
        resize(full_img, img, Size(), scale, scale);
        images[i] = img.clone();
        (*finder)(img, features[i]); // finds the features
        features[i].img_idx = i;
    }
    finder->collectGarbage();
    full_img.release();
    img.release();
    

    // 3- Match features
    vector<MatchesInfo> pairwise_matches;
    //A matcher is created with the float match_conf = 0.3f parameter
    BestOf2NearestMatcher matcher(false, match_conf);
    matcher(features, pairwise_matches);
    matcher.collectGarbage();


    // 4- Select images and matches subset to build panorama
    vector<int> indices = leaveBiggestComponent(features, pairwise_matches, conf_thresh);
    vector<Mat> img_subset;
    vector<String> img_names_subset;
    vector<Size> full_img_sizes_subset;
    for (size_t i = 0; i < indices.size(); ++i) {
        img_names_subset.push_back(img_names[indices[i]]);
        img_subset.push_back(images[indices[i]]);
        full_img_sizes_subset.push_back(full_img_sizes[indices[i]]);
    }
    images = img_subset;
    img_names = img_names_subset;
    full_img_sizes = full_img_sizes_subset;
    // Estimate camera parameters rough
    HomographyBasedEstimator estimator;
    vector<CameraParams> cameras;
    if (!estimator(features, pairwise_matches, cameras)) {
        cout << "Homography estimation failed." << endl;
        return -1;
    }
    for (size_t i = 0; i < cameras.size(); ++i) {
        Mat R;
        cameras[i].R.convertTo(R, CV_32F);
        cameras[i].R = R;
        cout << "Initial intrinsic #" << indices[i] + 1 << ":\n" << cameras[i].K() << endl;
    }


    // 5- Refine camera parameters globally
    Ptr<BundleAdjusterBase> adjuster = makePtr<BundleAdjusterRay>(); // or makePtr<BundleAdjusterReproj>()
    adjuster->setConfThresh(conf_thresh);
    if (!(*adjuster)(features, pairwise_matches, cameras)) {
        cout << "Camera parameters adjusting failed." << endl;
        return -1;
    }
    // Find median focal length
    vector<double> focals;
    for (size_t i = 0; i < cameras.size(); ++i) {
        cout << "Camera #" << indices[i] + 1 << ":\n" << cameras[i].K() << endl;
        focals.push_back(cameras[i].focal);
    }
    sort(focals.begin(), focals.end());
    float warped_image_scale;
    if (focals.size() % 2 == 1)
        warped_image_scale = (float) (focals[focals.size() / 2]);
    else
        warped_image_scale = (float) (focals[focals.size() / 2 - 1] + focals[focals.size() / 2]) * 0.5f;


    // 6- Wave correlation (opt)
    if (do_wave_correct) {
        vector<Mat> rmats;
        for (size_t i = 0; i < cameras.size(); ++i)
            rmats.push_back(cameras[i].R.clone());
        waveCorrect(rmats, wave_correct_type);
        for (size_t i = 0; i < cameras.size(); ++i)
            cameras[i].R = rmats[i];
    }


    // 7- Warp images
    vector<Point> corners(num_images);
    vector<UMat> masks_warped(num_images);
    vector<UMat> images_warped(num_images);
    vector<Size> sizes(num_images);
    vector<UMat> masks(num_images);
    // Prepare images masks
    for (int i = 0; i < num_images; ++i) {
        masks[i].create(images[i].size(), CV_8U);
        masks[i].setTo(Scalar::all(255));
    }

    // Map projections
    Ptr<WarperCreator> warper_creator;
    warper_creator = makePtr<cv::SphericalWarper>();
    Ptr<RotationWarper> warper = warper_creator->create((float)(warped_image_scale * scale));
    for (int i = 0; i < num_images; ++i) {
        Mat_<float> K;
        cameras[i].K().convertTo(K, CV_32F);
        float swa = (float) scale;
        K(0, 0) *= swa; K(0, 2) *= swa;
        K(1, 1) *= swa; K(1, 2) *= swa;
        corners[i] = warper->warp(images[i], K, cameras[i].R, INTER_LINEAR,
                BORDER_REFLECT, images_warped[i]);
        sizes[i] = images_warped[i].size();
        warper->warp(masks[i], K, cameras[i].R, INTER_NEAREST, BORDER_CONSTANT, masks_warped[i]);
    }
    vector<UMat> images_warped_f(num_images);
    for (int i = 0; i < num_images; ++i)
        images_warped[i].convertTo(images_warped_f[i], CV_32F);


    // 8- Compensate exposure errors
    Ptr<ExposureCompensator> compensator = 
            ExposureCompensator::createDefault(expos_comp_type);
    compensator->feed(corners, images_warped, masks_warped);


    // 9- Find seam masks
    Ptr<SeamFinder> seam_finder;
    seam_finder = makePtr<GraphCutSeamFinder>(GraphCutSeamFinderBase::COST_COLOR);
    seam_finder->find(images_warped_f, corners, masks_warped);
    

    // 10- Create a blender
    Ptr<Blender> blender = Blender::createDefault(Blender::MULTI_BAND, false);
    Size dst_sz = resultRoi(corners, sizes).size();
    float blend_width = sqrt((float)(dst_sz.area())) * blend_strength / 100.f;
    MultiBandBlender* mb = (MultiBandBlender*)(blender.get());
    mb->setNumBands((int)(ceil(log(blend_width) / log(2.)) - 1.));
    cout << "Multi-band blender, number of bands: " << mb->numBands() << endl;
    blender->prepare(corners, sizes);


    // 11- Compositing step
    Mat img_warped, img_warped_s;
    Mat dilated_mask, seam_mask, mask, mask_warped;
    for (int img_idx = 0; img_idx < num_images; ++img_idx) {
        cout << "Compositing image #" << indices[img_idx] + 1 << endl;
        // 11.1- Read image and resize
        full_img = imread(img_names[img_idx]);
        resize(full_img, img, Size(), scale, scale);
        full_img.release();
        Size img_size = img.size();
        Mat K;
        cameras[img_idx].K().convertTo(K, CV_32F);
        // 11.2- Warp the current image
        warper->warp(img, K, cameras[img_idx].R, INTER_LINEAR, BORDER_REFLECT, img_warped);
        // Warp the current image mask
        mask.create(img_size, CV_8U);
        mask.setTo(Scalar::all(255));
        warper->warp(mask, K, cameras[img_idx].R, INTER_NEAREST, BORDER_CONSTANT, mask_warped);
        // 11.3- Compensate exposure error step
        compensator->apply(img_idx, corners[img_idx], img_warped, mask_warped);
        img_warped.convertTo(img_warped_s, CV_16S);
        img_warped.release();
        img.release();
        mask.release();
        dilate(masks_warped[img_idx], dilated_mask, Mat());
        resize(dilated_mask, seam_mask, mask_warped.size());
        mask_warped = seam_mask & mask_warped;
        // 11.4- Blending images step
        blender->feed(img_warped_s, mask_warped, corners[img_idx]);
    }
    Mat result, result_mask;
    blender->blend(result, result_mask);


    imwrite(result_name, result);
    result = imread(result_name, 1);
    imshow(result_name, result);
    waitKey(0);
    return 0;
}