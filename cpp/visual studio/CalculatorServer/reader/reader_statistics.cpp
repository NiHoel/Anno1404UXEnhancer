#include "reader_statistics.hpp"

#include <iostream>

namespace reader
{
statistics::statistics(image_recognition& recog)
	:
	recog(recog),
	stats_screen(recog),
	hud(recog)
{
}

void statistics::update(const std::string& language, const cv::Mat& img)
{
	stats_screen.update(language, img);
	hud.update(language, img);
}

std::map<unsigned int, int> statistics::get_population_amount()
{
	if (stats_screen.is_open())
		return stats_screen.get_population_amount();
	else
		return hud.get_population_amount();
}

std::map<unsigned int, int> statistics::get_average_productivities()
{
	return stats_screen.get_average_productivities();
}


std::string statistics::get_selected_island()
{


	if (recog.is_verbose()) {
		std::cout << "Island:\t";
	}
	if (stats_screen.is_open())
	{
		return stats_screen.get_selected_island();

	}
	else // statistics screen not open
	{
		return hud.get_selected_island();
	}

	return std::string();
}



const keyword_dictionary& statistics::get_dictionary()
{
	return recog.get_dictionary();
}

bool statistics::has_language(const std::string& language)
{
	return recog.has_language(language);
}

std::map<unsigned int, int> statistics::get_assets_existing_buildings()
{
	return stats_screen.get_assets_existing_buildings();
}

}
