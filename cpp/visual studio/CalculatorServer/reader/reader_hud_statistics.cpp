#include "reader_hud_statistics.hpp"


#include <iostream>
#include <queue>
#include <regex>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace reader
{
const cv::Scalar hud_params::background_brown_light = cv::Scalar(126, 179, 216, 255);
const cv::Rect2f hud_params::position_population_top_icon = cv::Rect2f(cv::Point2f(0.12656, 0.73929), cv::Point2f(0.13906, 0.76188));
const cv::Rect2f hud_params::position_population_bottom_icon = cv::Rect2f(cv::Point2f(0.12629, 0.89781), cv::Point2f(0.13903, 0.92024));
const cv::Rect2f hud_params::pane_population = cv::Rect2f(cv::Point2f(0.12792f, 0.73848f), cv::Point2f(0.18238f, 0.98427f));

hud_statistics::hud_statistics(image_recognition& recog)
	:
	recog(recog)
{}

void hud_statistics::update(const std::string& language,
	const cv::Mat& img)
{
	selected_island.clear();
	recog.update(language);
	img.copyTo(this->screenshot);
}



std::map<unsigned int, int> hud_statistics::get_population_amount() const
{

	std::map<unsigned int, int> result;

	cv::Rect2f dimensions = hud_params::position_population_bottom_icon;
	float y = hud_params::position_population_bottom_icon.y;
	float dy = hud_params::position_population_top_icon.y - hud_params::position_population_bottom_icon.y;

	for (int i = 0; i < 7; i++)
	{
		cv::Rect2f population_icon_pane(hud_params::position_population_bottom_icon.x,
			y + i / 6.f * dy,
			dimensions.width,
			dimensions.height);

		cv::Mat population_icon = recog.get_square_region(screenshot, population_icon_pane);


#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
		cv::imwrite("debug_images/population_icon.png", population_icon);
#endif


		std::vector<unsigned int> guids = recog.get_guid_from_icon(population_icon, recog.population_icons, hud_params::background_brown_light);
		if (guids.size() != 1)
		{
			if (!i)
				return result;
			else
				break;
		}

		cv::Rect2f population_text_pane(dimensions.x + 1.5f * dimensions.width,
			population_icon_pane.y + 0.2f * dimensions.height,
			2.7f * dimensions.width,
			0.75f * dimensions.height);

		cv::Mat text_img = recog.binarize(recog.get_pane(population_text_pane, screenshot));
#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
		cv::imwrite("debug_images/pop_amount_text.png", text_img);
#endif


		int population = recog.number_from_region(text_img);

		if (population >= 0)
			result.emplace(guids.front(), population);

	}

	for (const auto& entry : recog.get_dictionary().population_levels)
	{
		if (result.find(entry.first) == result.end())
			result[entry.first] = 0;
	}
	return result;
}


std::string hud_statistics::get_selected_island() const
{
	return recog.ALL_ISLANDS;
}





}