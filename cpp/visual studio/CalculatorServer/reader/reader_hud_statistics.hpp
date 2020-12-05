#pragma once

#include "reader_util.hpp"

namespace reader
{

class hud_params
{
public:
	static const cv::Scalar background_brown_light;

	static const cv::Rect2f position_population_top_icon;
	static const cv::Rect2f position_population_bottom_icon;

	/* pane rectangles relative to [0,1]² image */
	static const cv::Rect2f pane_population;
};

class hud_statistics
{
public:
	hud_statistics(image_recognition& recog);

	void update(const std::string& language,
		const cv::Mat& img);

	/**
*
* returns a map with entries for all detected population types referred by their GUID
*/
	std::map < unsigned int, int> get_population_amount() const;

	std::string get_selected_island() const;


private:
	image_recognition& recog;
	cv::Mat screenshot;
	std::string selected_island;
};

}