#include "BruteForceHammingMatcher.hpp"

#include <iostream>
#include <limits>
#include <typeinfo>
#include <opencv2/core/cuda.hpp>
#include <opencv2/core/cuda_stream_accessor.hpp>

template<typename featureType, unsigned int featureLength>
GPUBruteForceHammingMatcher<featureType, featureLength>::GPUBruteForceHammingMatcher(const float matchThreshold) :
	m_matchThreshold(matchThreshold){
	m_matcher = cv::cuda::DescriptorMatcher::createBFMatcher(cv::NORM_HAMMING);
	
	if (cudaStreamCreate(&m_stream) == cudaErrorInvalidValue )
    std::cerr << "Unable to create streams" << std::endl;
}

template <typename featureType, unsigned int featureLength>
void GPUBruteForceHammingMatcher<featureType, featureLength>::match(featureType* descriptors1, featureType* descriptors2, int h_numKP1, int h_numKP2) {
	const int largerKPNum = h_numKP1 >= h_numKP2 ? h_numKP1 : h_numKP2;

	// Because hash_code is silly but works
	int typeMat; 
	if (typeid(featureType).hash_code() == typeid(float).hash_code())
			typeMat = CV_32F;
	else if (typeid(featureType).hash_code() == typeid(unsigned char).hash_code())
			typeMat = CV_8U;

	const cv::Mat h_descriptors1(h_numKP1, featureLength, typeMat, descriptors1);
	const cv::Mat h_descriptors2(h_numKP2, featureLength, typeMat, descriptors2);

	cv::cuda::GpuMat d_descriptors1(h_numKP1, featureLength, typeMat);
  cv::cuda::GpuMat d_descriptors2(h_numKP2, featureLength, typeMat);

	cv::cuda::Stream stream = cv::cuda::StreamAccessor::wrapStream(m_stream);
	
	d_descriptors1.upload(h_descriptors1, stream);
	d_descriptors2.upload(h_descriptors2, stream);

	cv::cuda::GpuMat d_results;

	m_matcher->knnMatchAsync(d_descriptors1, d_descriptors2, d_results, 2, cv::noArray(), stream);

	stream.waitForCompletion();

	std::vector<std::vector<cv::DMatch>> matchVector;
	m_matcher->knnMatchConvert(d_results, matchVector);

	m_goodMatches.clear();

	for (size_t i = 0; i < std::min(d_descriptors1.rows - 1, static_cast<int>(matchVector.size())); i++) {
		if ((matchVector[i][0].distance < m_matchThreshold*(matchVector[i][1].distance)) 
				&& (static_cast<int>(matchVector[i].size()) <= 2 && static_cast<int>(matchVector[i].size()  > 0 )))
				m_goodMatches.push_back(LatchBitMatcherMatch(matchVector[i][0].queryIdx, matchVector[i][0].trainIdx, matchVector[i][0].distance));
	}
}

template<typename featureType, unsigned int featureLength>
const std::vector<LatchBitMatcherMatch> GPUBruteForceHammingMatcher<featureType, featureLength>::retrieveMatches() {
	return m_goodMatches;
}

template<typename featureType, unsigned int featureLength>
GPUBruteForceHammingMatcher<featureType, featureLength>::~GPUBruteForceHammingMatcher() {
	m_matcher.release();
	cudaStreamDestroy(m_stream);
}

// When adding extra features, you need to add their instantiation here
// ORB
template class GPUBruteForceHammingMatcher<unsigned char, 32>;
