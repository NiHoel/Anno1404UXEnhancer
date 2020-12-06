#include "reader_statistics_screen.hpp"

#include <iostream>
#include <numeric>
#include <regex>

#include <boost/algorithm/string.hpp>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace reader
{
////////////////////////////////////////
//
// Class: statistics_screen_params
//
////////////////////////////////////////

const cv::Scalar statistics_screen_params::background_brown_dark = cv::Scalar(29,45,58,255);
const cv::Scalar statistics_screen_params::icon_background = cv::Scalar(184,196,198,255);

const cv::Rect2f statistics_screen_params::size_framed_icon = cv::Rect2f(cv::Point2f(0.26455f, 0.38384f), cv::Point2f(0.29399f, 0.43520f));
const cv::Rect2f statistics_screen_params::size_icon = cv::Rect2f(0.068493151f, 0.068493151f, 0.8630137f, 0.8630137f);
	
const cv::Rect2f statistics_screen_params::pane_title = cv::Rect2f(cv::Point2f(0.35460f, 0.15582f), cv::Point2f(0.43390f, 0.18032f));
const cv::Rect2f statistics_screen_params::pane_island = cv::Rect2f(cv::Point2f(0.45350f, 0.19592f), cv::Point2f(0.53259f, 0.21295f));
const cv::Rect2f statistics_screen_params::pane_production_left = cv::Rect2f(cv::Point2f(0.26210f, 0.37516f), cv::Point2f(0.45077f, 0.81145f));

const unsigned int statistics_screen_params::count_cols = 5;


////////////////////////////////////////
//
// Class: statistics_screen
//
////////////////////////////////////////


statistics_screen::statistics_screen(image_recognition& recog)
	:
	recog(recog),
	open(false)
{
}




void statistics_screen::update(const std::string& language, const cv::Mat& img)
{
	selected_island = std::string();

	recog.update(language);

	cv::Mat statistics_text_img = recog.binarize(recog.get_pane(statistics_screen_params::pane_title, img), true);
	if (recog.is_verbose()) {
		cv::imwrite("debug_images/statistics_text.png", statistics_text_img);
		cv::imwrite("debug_images/statistics_screenshot.png", img);
	}
	if (recog.get_guid_from_name(statistics_text_img, recog.make_dictionary({ phrase::REEVES_BOOK })).empty())
	{
		open = false;
		if (recog.is_verbose()) {
			std::cout << std::endl;
		}
		return;
	}

	if (recog.is_verbose()) {
		std::cout << std::endl;
	}

	img.copyTo(this->screenshot);

	open = true;

}

bool statistics_screen::is_open() const
{
	return open;
}


bool statistics_screen::is_all_islands_selected()
{
	if (!screenshot.size || !is_open())
		return false;

	if (selected_island.empty())
		get_selected_island();
	
		return selected_island.compare(recog.ALL_ISLANDS) == 0;
}




std::map<unsigned int, int> statistics_screen::get_population_amount() const
{
	return std::map<unsigned int, int>();
}

std::map<unsigned int, int> statistics_screen::get_average_productivities()
{
	const cv::Mat& im = screenshot;

	 return std::map<unsigned int, int>();
	/*
	if (get_open_tab() != statistics_screen::tab::PRODUCTION)
		return result;

	cv::Mat roi = get_center_pane();

	if (roi.empty())
		return result;

	if (recog.is_verbose()) {
		cv::imwrite("debug_images/statistics_window_scroll_area.png", roi);
	}

	if (recog.is_verbose()) {
		std::cout << "Average productivities" << std::endl;
	}

	recog.iterate_rows(roi, 0.9f, [&](const cv::Mat& row)
		{
			if (recog.is_verbose()) {
				cv::imwrite("debug_images/row.png", row);
			}

			cv::Mat product_icon = recog.get_square_region(row, statistics_screen_params::position_factory_icon);
			if (recog.is_verbose()) {
				cv::imwrite("debug_images/factory_icon.png", product_icon);
			}
			cv::Scalar background_color = statistics_screen::is_selected(product_icon.at<cv::Vec4b>(0, 0)) ? statistics_screen_params::background_blue_dark : statistics_screen_params::background_brown_light;

			std::vector<unsigned int> p_guids = recog.get_guid_from_icon(product_icon, recog.product_icons, background_color);
			if (p_guids.empty())
				return;

			if (recog.is_verbose()) {
				try {
					std::cout << recog.get_dictionary().products.at(p_guids.front()) << ":\t";
				}
				catch (...) {}
			}

			bool selected = is_selected(row.at<cv::Vec4b>(0.1f * row.rows, 0.5f * row.cols));
			cv::Mat productivity_text = recog.binarize(recog.get_cell(row, 0.7f, 0.1f, 0.4f), selected);
			if (recog.is_verbose()) {
				cv::imwrite("debug_images/productivity_text.png", productivity_text);
			}
			int prod = recog.number_from_region(productivity_text);

			if (prod > 500 && prod % 100 == 0)
				prod /= 100;

			if (prod >= 0)
			{
				for (unsigned int p_guid : p_guids)
					for (unsigned int f_guid : recog.product_to_factories[p_guid])
						result.emplace(f_guid, prod);
			}


			if (recog.is_verbose()) {
				std::cout << std::endl;
			}

		});


	return result;
	*/
}

std::map<unsigned int, int> statistics_screen::get_assets_existing_buildings()
{
	if(!is_open())
		return std::map<unsigned int, int>();

	std::map<unsigned int, int> result;
	cv::Mat icon_dummy = recog.get_pane(statistics_screen_params::size_framed_icon, screenshot);
	cv::Rect2i offering_size = cv::Rect2i(0, 0, icon_dummy.cols, icon_dummy.rows);
	cv::Mat production_img = recog.get_pane(statistics_screen_params::pane_production_left, screenshot);
	std::vector<cv::Rect2i> boxes(recog.detect_boxes(production_img, offering_size,cv::Rect2i(), 0.1f));

	std::sort(boxes.begin(), boxes.end(), [&offering_size](const cv::Rect2i& lhs, const cv::Rect2i& rhs) {
		if (lhs.y + offering_size.height < rhs.y)
			return true;
		else if (rhs.y + offering_size.height < lhs.y)
			return false;
		else if (lhs.x + offering_size.width < rhs.x)
			return true;
		return false;
		});

	for (const cv::Rect2i& box : boxes)
	{
		float dim = std::min(box.width, box.height);
		const auto& s = statistics_screen_params::size_icon;
		cv::Mat icon = production_img(cv::Rect2i(box.x + dim * s.x, box.y + dim * s.y, dim * s.width, dim * s.height));


#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
		cv::imwrite("debug_images/icon.png", icon);
#endif
		
		std::vector<unsigned int> building_candidates = recog.get_guid_from_icon(
			icon,
			recog.building_icons,
			statistics_screen_params::icon_background);

		if (building_candidates.empty() || box.y + 1.5f * box.height >= production_img.rows)
			continue;

		cv::Rect2i count_box(box.x, box.y + box.height + box.height / 8.f, box.width, box.height / 3.f);
		cv::Mat count_img = recog.binarize(production_img(count_box), true);

#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
			cv::imwrite("debug_images/factory_count.png", count_img);
#endif

			int count = recog.number_from_region(count_img);
			if(count > 0)
		
			result.emplace(building_candidates.front(), count);
	}

	return result;
}


std::string statistics_screen::get_selected_island()
{
	if (!selected_island.empty())
		return selected_island;

	cv::Mat roi = recog.binarize(recog.get_pane(statistics_screen_params::pane_island, screenshot), true);
	if (recog.is_verbose()) {
		cv::imwrite("debug_images/selected_island.png", roi);
	}

	if (!recog.get_guid_from_name(roi, recog.make_dictionary({ phrase::WORLD_STATISTICS })).empty())
	{
		selected_island = recog.ALL_ISLANDS;

		if (recog.is_verbose()) {
			std::cout << recog.ALL_ISLANDS << std::endl;
		}

		return selected_island;
	}

	auto words_and_boxes = recog.detect_words(roi, tesseract::PSM_SINGLE_LINE);


	// check whether mutliple islands are selected
	selected_island = recog.join(words_and_boxes, true);
	
	if (recog.is_verbose()) {
		std::cout << selected_island << std::endl;
	}
	return selected_island;
}

}
