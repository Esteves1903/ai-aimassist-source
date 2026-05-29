#pragma once
#include "opencv2/opencv.hpp"
#include "opencv2/core/ocl.hpp"
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "defines.h"
#include "mainA.h"


inline cv::Mat frame;

class detector {
public:
	detector(std::string dataset_labels_path, std::string onnx_path);
	~detector() { };
	void start(cv::Mat& image);
	std::string getBackendName() const { return m_backend_name; }

private:
	void postprocess(cv::Mat& frame, const std::vector<cv::Mat>& outs);
	void draw_box(float conf, int left, int top, int right, int bottom, cv::Mat& frame, cv::Scalar color = cv::Scalar(0, 0, 255));
	std::vector<cv::String> get_outputs_names(const cv::dnn::Net& net);
	void setupOptimalBackend();

	cv::dnn::Net m_net;
	float m_confidence = 0.06f;
	float m_threshold = 0.25f;
	int m_activation_range = 125;
	std::vector<std::string> m_classes;
	std::string m_backend_name = "CPU";
};