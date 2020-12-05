#include "reader_hud_statistics.hpp"


#include <iostream>
#include <queue>
#include <regex>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace reader
{
const cv::Scalar hud_params::background_brown_light = cv::Scalar(126, 179, 216);
const cv::Rect2f hud_params::position_population_icon = cv::Rect2f(cv::Point2f(0, 0), cv::Point2f(0.1f, 1));
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


	cv::Mat roi = recog.get_pane(hud_params::pane_population, screenshot);

	if (recog.is_verbose()) {
		cv::imwrite("debug_images/global_population.png", roi);
	}

	recog.iterate_rows(roi, 0.75f, [&](const cv::Mat& row)
		{
			cv::Mat population_icon = recog.get_square_region(row, hud_params::position_population_icon);

#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
			cv::imwrite("debug_images/row.png", row);
			cv::imwrite("debug_images/population_icon.png", population_icon);
#endif
		

			std::vector<unsigned int> guids = recog.get_guid_from_icon(population_icon, recog.population_icons, hud_params::background_brown_light);
			if (guids.size() != 1)
				return;

			cv::Mat text_img = recog.binarize(recog.get_cell(row, 0.2f, 0.8f, 0.1f));
#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
			cv::imwrite("debug_images/pop_amount_text.png", text_img);
#endif


			int population = recog.number_from_region(text_img);

			if (population >= 0)
				result.emplace(guids.front(), population);

		});

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