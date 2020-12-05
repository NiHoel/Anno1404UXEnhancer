#pragma once

#include "reader_util.hpp"

namespace reader
{

class statistics_screen_params
{
public:
	static const cv::Scalar background_brown_dark;
	static const cv::Scalar icon_background;

	
	static const cv::Rect2f size_framed_icon;
	static const cv::Rect2f size_icon;
	
	/* pane rectangles relative to [0,1]² image */
	static const cv::Rect2f pane_title;
	static const cv::Rect2f pane_island;
	static const cv::Rect2f pane_production_left;

	static const unsigned int count_cols;

};

/*
* Stores resolution independent properties of the statistics menu
* Allows to perform elementary boolean tests
* use update() to pass a new screenshot
*/
class statistics_screen
{
public:


	statistics_screen(image_recognition& recog);

	void update(const std::string& language, const cv::Mat& img);

	

	/*
	* Accessor functions to read data
	**/
	std::map < unsigned int, int> get_population_amount() const;

	/*
	* Returns percentile productivity for buildings.
	* Returns an empty map in case no information is found.
	*/
	std::map < unsigned int, int> get_average_productivities();

	/*
	* Returns count of existing buildings (houses/buildings).
	* Returns an empty map in case no information is found.
	*/
	std::map < unsigned int, int> get_assets_existing_buildings();


	/*
* Returns the name of the selected island
* Returns ALL_ISLANDS if world statistics is selected
*/
	std::string get_selected_island();


	bool is_open() const;

	bool is_all_islands_selected();


private:
	image_recognition& recog;
	bool open;

	cv::Mat screenshot;

	// empty if not yet evaluated, use get_selected_island()
	std::string selected_island;

};

}
