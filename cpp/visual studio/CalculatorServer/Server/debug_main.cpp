#pragma once

#include <boost/assert.hpp>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#pragma comment(lib, "WS2_32.lib")


#include <iostream>

#include "reader_statistics.hpp"

using namespace reader;

template<typename T>
void print(const std::map<unsigned int, T>& map,
	const std::map<unsigned int, std::string>& dictionary = std::map<unsigned int, std::string>())
{
	for (const auto& entry : map)
	{
		if (dictionary.find(entry.first) != dictionary.cend())
			std::cout << dictionary.find(entry.first)->second << "\t=\t" << entry.second << std::endl;
		else
			std::cout << entry.first << "\t=\t" << entry.second << std::endl;
	}
}

void unit_tests(class statistics& image_recog)
{
	{
		image_recog.update("german", image_recognition::load_image("test_screenshots/screenshot0082.jpg"));
		const auto result = image_recog.get_population_amount();
		BOOST_ASSERT(result.at(51904) == 981);
		BOOST_ASSERT(result.at(51903) == 4090);
		BOOST_ASSERT(result.at(51902) == 1862);
		BOOST_ASSERT(result.at(51901) == 343);
		BOOST_ASSERT(result.at(51911) == 1164);
		BOOST_ASSERT(result.at(51910) == 725);
		BOOST_ASSERT(result.at(51909) == 675);
		
	}
	{
		image_recog.update("german", image_recognition::load_image("test_screenshots/screenshot0093.jpg"));
		const auto result = image_recog.get_population_amount();
		BOOST_ASSERT(result.at(51904) == 0);
		BOOST_ASSERT(result.at(51903) == 925);
		BOOST_ASSERT(result.at(51902) == 651);
		BOOST_ASSERT(result.at(51901) == 134);
		BOOST_ASSERT(result.at(51911) == 112);
		BOOST_ASSERT(result.at(51910) == 0);
		BOOST_ASSERT(result.at(51909) == 600);

	}

	std::cout << "all tests passed!" << std::endl;
}

int main(int argc, char** argv) {
	image_recognition recog(true);
	statistics image_recog(recog);
	//unit_tests(image_recog);

	cv::Mat src = image_recognition::load_image("test_screenshots/screenshot0098.jpg");

	image_recog.update("german", src);


	std::string island = image_recog.get_selected_island();
	std::cout << "Island: " << island << std::endl;
	std::cout << std::endl;

	std::cout << "Population amount" << std::endl;
	print(image_recog.get_population_amount(), image_recog.get_dictionary().population_levels);
	std::cout << std::endl;


	std::cout << "Average Productivity" << std::endl;
	print(image_recog.get_average_productivities(), image_recog.get_dictionary().buildings);
	std::cout << std::endl;

	//std::cout << "Optimal Productivity" << std::endl;
	//print(image_recog.get_optimal_productivities(), image_recog.get_dictionary().buildings);
	//std::cout << std::endl;

	std::cout << "Existing buildings" << std::endl;
	print(image_recog.get_assets_existing_buildings(), image_recog.get_dictionary().buildings);
	std::cout << std::endl;



	return 0;
}