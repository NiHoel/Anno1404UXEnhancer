﻿#include "reader_util.hpp"

#include <windows.h>

#include <algorithm>
#include <codecvt>
#include <filesystem>
#include <iostream>
#include <list>
#include <numeric>
#include <psapi.h>
#include <regex>
#include <stdio.h>
#include <tchar.h>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/cxx17/transform_reduce.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <tesseract/genericvector.h>
#include "reader_statistics_screen.hpp"


namespace reader
{

struct comparePoints {
	bool operator()(const cv::Point& a, const cv::Point& b) const {
		if (a.x != b.x)
			return a.x < b.x;
		return a.y < b.y;
	}
};



////////////////////////////////////////
//
// Class: image_recognition
//
////////////////////////////////////////


image_recognition::image_recognition(bool verbose, std::string window_regex)
	:
	window_regex(window_regex.empty() ? "A[Nn][Nn][Oo] 1404.*" : std::move(window_regex)),
	verbose(verbose),
	ocr(nullptr),
	ocr_language("english")/*,
	number_mode(false)*/
{
	boost::property_tree::ptree pt;
	boost::property_tree::read_json("texts/params.json", pt);

	for (const auto& language : pt.get_child("languages"))
	{
		std::string key(language.second.get_value<std::string>());
		keyword_dictionary value;
		dictionaries.emplace(key, value);
	}

	std::map<std::string, cv::Mat> icon_tables;
	for (const auto& icon_map : pt.get_child("icons"))
	{
		std::string key(icon_map.second.get_value<std::string>());
		icon_tables.emplace(key, load_image("icons/" + key));
	}

	auto load_and_save_icon = [&](unsigned int guid,
		const boost::property_tree::ptree& asset,
		std::map<unsigned int, cv::Mat>& container)
	{
		if (asset.get_child_optional("icon").has_value())
		{
			try
			{
				auto config = asset.get_child("icon");
				auto& icon_map = icon_tables.at(config.get_child("path").get_value<std::string>());
				int x = config.get_child("x").get_value<int>();
				int y = config.get_child("y").get_value<int>();
				int width = config.get_child("width").get_value<int>();
				int height = config.get_child("height").get_value<int>();
				
				cv::Mat icon = icon_map(cv::Rect2i(x,y,width,height));
				container.emplace(guid, icon);

				if (verbose) {
					cv::imwrite("debug_images/icon_template.png", icon);
				}
			}
			catch (const std::invalid_argument& e)
			{
				std::cout << e.what() << std::endl;
			}
		}
	};

	// load buildings
	auto process_buildings = [&](const boost::property_tree::ptree& root) {
		for (const auto& factory : root)
		{
			unsigned int guid = factory.second.get_child("guid").get_value<unsigned int>();

			load_and_save_icon(guid, factory.second, building_icons);

			for (const auto& language : factory.second.get_child("locaText"))
			{

				dictionaries.at(language.first).buildings.emplace(guid, language.second.get_value<std::string>());
			}
		}
	};

	if (verbose) {
		std::cout << "Load buildings." << std::endl;
	}
	process_buildings(pt.get_child("factories"));
	process_buildings(pt.get_child("residenceBuildings"));
	process_buildings(pt.get_child("publicBuildings"));

	// load products
	for (const auto& product : pt.get_child("products"))
	{
		unsigned int guid = product.second.get_child("guid").get_value<unsigned int>();
		if (!product.second.get_child_optional("producers").has_value())
			continue;

		// store buildings and regions
		std::vector<unsigned int> factories;
		for (const auto& factory_entry : product.second.get_child("producers"))
		{
			unsigned int factory_id = factory_entry.second.get_value<unsigned int>();
			factories.push_back(factory_id);
		}
		product_to_factories.emplace(guid, std::move(factories));

		for (const auto& language : product.second.get_child("locaText"))
		{
			dictionaries.at(language.first).products.emplace(guid, language.second.get_value<std::string>());
		}

		load_and_save_icon(guid, product.second, product_icons);
	}

	if (verbose) {
		std::cout << "Load population levels." << std::endl;
	}
	// load population levels
	for (const auto& level : pt.get_child("populationLevels"))
	{
		unsigned int guid = level.second.get_child("guid").get_value<unsigned int>();
		load_and_save_icon(guid, level.second, population_icons);
		for (const auto& language : level.second.get_child("locaText"))
		{

			dictionaries.at(language.first).population_levels.emplace(guid, language.second.get_value<std::string>());
		}
	}

	if (verbose) {
		std::cout << "Load texts." << std::endl;
	}
	pt.clear();
	if (boost::filesystem::exists("texts/ui_texts.json"))
	{
		boost::property_tree::read_json("texts/ui_texts.json", pt);
		for (const auto& text_node : pt)
		{
			std::string language = text_node.first;
			for (const auto& entry : text_node.second)
			{
				unsigned int guid = std::atoi(entry.first.c_str());
				dictionaries.at(language).ui_texts.emplace(guid, entry.second.get_value<std::string>());
			}
		}
	}
	else
	{
		throw std::runtime_error("ui texts not found");
	}
}

std::string image_recognition::to_string(const std::wstring& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.to_bytes(str);
}

std::wstring image_recognition::to_wstring(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.from_bytes(str);
}

cv::Mat image_recognition::get_square_region(const cv::Mat& img, const cv::Rect2f& rect)
{
	if (!img.size)
		return cv::Mat();

	float cols = img.cols - 1.f;
	float rows = img.rows - 1.f;
	float normal_cols = 16 * rows / 9.f;

	float x = normal_cols * rect.x;
	if (rect.x + 0.5f * rect.width > 0.66f) // right aligned
		x = cols - normal_cols + x;
	else if (rect.x + 0.5f * rect.width > 0.33f) // center aligned
		x = x - 0.5f * normal_cols + 0.5f * cols;

	int dim = static_cast<int>(std::lround(rect.height * img.rows));
	cv::Rect scaled(static_cast<int>(x),
		static_cast<int>(rect.y * img.rows),
		dim, dim);
	return img(scaled);
}

cv::Mat image_recognition::get_cell(const cv::Mat& img, float crop_left, float width, float crop_vertical)
{
	if (!img.size)
		return cv::Mat();

	cv::Rect scaled(static_cast<int>(crop_left * img.cols), static_cast<int>(0.5f * crop_vertical * img.rows), static_cast<int>(width * img.cols), static_cast<int>((1 - crop_vertical) * img.rows));
	return img(scaled);
}

cv::Mat image_recognition::get_pane(const cv::Rect2f& rect, const cv::Mat& img)
{
	if (!img.size)
		return img;

	float cols = img.cols - 1.f;
	float rows = img.rows - 1.f;
	float normal_cols = 16 * rows / 9.f;

	float x = normal_cols * rect.x;
	if (rect.x + 0.5f * rect.width > 0.66f) // right aligned
		x = cols - normal_cols + x;
	else if(rect.x + 0.5f * rect.width > 0.33f) // center aligned
		x = x - 0.5f * normal_cols + 0.5f * cols;
	
	cv::Rect scaled(static_cast<int>(x), static_cast<int>(rect.y * rows), static_cast<int>(rect.width * normal_cols), static_cast<int>(rect.height * rows));
	return img(scaled);
}

bool image_recognition::closer_to(const cv::Scalar& color, const cv::Scalar& ref, const cv::Scalar& other)
{
	return (color - ref).dot(color - ref) < (color - other).dot(color - other);
}

bool image_recognition::is_button(const cv::Mat& image, const cv::Scalar& button_color, const cv::Scalar& background_color)
{
	const float margin = 0.125f;

	int matches = 0;
	for (const cv::Point2f& p : { cv::Point2f{0.5f, margin}, cv::Point2f{1 - margin, 0.5f}, cv::Point2f{0.5f, 1 - margin}, cv::Point2f{margin, 0.5f} })
	{
		cv::Point2i pos(static_cast<int>(p.x * image.cols), static_cast<int>(p.y * image.rows));
		matches += closer_to(image.at<cv::Vec4b>(pos), button_color, background_color);
	}

	return matches >= 3;
}



std::list<cv::Point> image_recognition::find_rgb_region(const cv::Mat& in, const cv::Point& seed, float threshold)
{
	cv::Mat input = in;
	std::list<cv::Point> ret;

	if (seed.x >= input.cols || seed.y >= input.rows)
		return ret;

	const cv::Rect img_rect = cv::Rect({ 0,0 }, in.size());

	std::set < cv::Point, comparePoints> open;
	open.insert(seed);
	const auto seed_color = input.at<cv::Vec4b>(seed);

	std::set<cv::Point, comparePoints> closed;



	while (!open.empty()) {
		const cv::Point current_point = *open.begin();
		open.erase(open.begin());


		const auto cc = input.at<cv::Vec4b>(current_point);
		const int color_diff = (cc.val[0] - int(seed_color.val[0])) * (cc.val[0] - int(seed_color.val[0]))
			+ (cc.val[1] - int(seed_color.val[1])) * (cc.val[1] - int(seed_color.val[1]))
			+ (cc.val[2] - int(seed_color.val[2])) * (cc.val[2] - int(seed_color.val[2]))
			+ (cc.val[3] - int(seed_color.val[3])) * (cc.val[3] - int(seed_color.val[3]));
		if (color_diff
			<= threshold)
		{
			ret.push_back(current_point);
			cv::Point new_candidates[4] = {
				current_point + cv::Point(1,0),
				current_point + cv::Point(0,1),
				current_point + cv::Point(-1,0),
				current_point + cv::Point(0,-1)
			};
			for (int i = 0; i < 4; i++) {
				if (new_candidates[i].inside(img_rect)
					&& closed.find(new_candidates[i]) == closed.end())
				{
					open.insert(new_candidates[i]);
				}
			}
		}
		closed.insert(current_point);
	}
	return ret;
}

cv::Mat image_recognition::blend_icon(const cv::Mat& icon, const cv::Scalar& background_color)
{
	cv::Mat background_img = cv::Mat(icon.rows, icon.cols, CV_8UC4);
	cv::Mat zeros = cv::Mat(icon.rows, icon.cols, CV_8UC1, cv::Scalar(0));
	background_img = background_color;

	return blend_icon(icon, background_img);
}

cv::Mat image_recognition::blend_icon(const cv::Mat& icon, const cv::Mat& background)
{
	cv::Mat zeros = cv::Mat(icon.rows, icon.cols, CV_8UC1, cv::Scalar(0));
	cv::Mat background_resized;

	cv::resize(background, background_resized, cv::Size(icon.cols, icon.rows));

	std::vector<cv::Mat> icon_channels;
	cv::split(icon, icon_channels);
	cv::Mat alpha;
	cv::merge(std::vector<cv::Mat>({ icon_channels[3],icon_channels[3],icon_channels[3],zeros }), alpha);

	return background_resized.mul(cv::Scalar(255, 255, 255, 255) - alpha, 1. / 255) + icon.mul(alpha, 1. / 255);
}

cv::Mat image_recognition::dye_icon(const cv::Mat& icon, cv::Scalar color)
{
	cv::Mat blue = cv::Mat(icon.rows, icon.cols, CV_8UC1, cv::Scalar(color[0]));
	cv::Mat green = cv::Mat(icon.rows, icon.cols, CV_8UC1, cv::Scalar(color[1]));
	cv::Mat red = cv::Mat(icon.rows, icon.cols, CV_8UC1, cv::Scalar(color[2]));

	std::vector<cv::Mat> icon_channels;
	cv::split(icon, icon_channels);
	cv::Mat result;
	cv::merge(std::vector<cv::Mat>({ blue, green, red, icon_channels[3] }), result);

	return result;
}

std::pair<cv::Rect, float> image_recognition::find_icon(const cv::Mat& source, const cv::Mat& icon, cv::Scalar background_color)
{
	float scaling = (source.cols * 0.027885f) / icon.cols;

	cv::Mat template_resized;
	cv::resize(blend_icon(icon, background_color), template_resized, cv::Size(static_cast<int>(scaling * icon.cols), static_cast<int>(scaling * icon.rows)));



#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
	cv::imwrite("debug_images/icon_template.png", template_resized);
#endif
	return match_template(source, template_resized);
}


std::vector<unsigned int> image_recognition::get_guid_from_icon(const cv::Mat& icon,
	const std::map<unsigned int, cv::Mat>& dictionary,
	const cv::Mat& background) const
{
	if (icon.empty())
		return std::vector<unsigned int>();

	cv::Mat background_resized;
	cv::resize(background, background_resized, cv::Size(icon.cols, icon.rows));

	cv::Mat diff;
	cv::absdiff(icon, background_resized, diff);
	float best_match = static_cast<float>(cv::sum(diff).ddot(cv::Scalar::ones()) / icon.rows / icon.cols);
	std::vector<unsigned int> guids;


	for (auto& entry : dictionary)
	{
		cv::Mat template_resized;
		cv::resize(blend_icon(entry.second, background_resized), template_resized, cv::Size(icon.cols, icon.rows));

#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
		cv::imwrite("debug_images/icon_template.png", template_resized);
#endif
		
		cv::Mat diff;
		cv::absdiff(icon, template_resized, diff);
		float match = cv::sum(diff).ddot(cv::Scalar::ones()) / icon.rows / icon.cols;
		if (match == best_match)
		{

			guids.push_back(entry.first);
		}
		else if (match < best_match)
		{
			guids.clear();
			guids.push_back(entry.first);
			best_match = match;
		}
	}

	if (best_match > 150)
		return std::vector<unsigned int>();

	if (verbose) {
		for (unsigned int guid : guids)
			std::cout << guid << ", ";
		std::cout << "(" << best_match << ")\t";
	}
	return guids;
}


std::vector<unsigned int> image_recognition::get_guid_from_hu_moments(const cv::Mat& icon,
	const std::map<unsigned int, std::vector<double>>& dictionary) const
{
	std::vector<double> icon_moments = get_hu_moments(icon);

	float best_match = 0;
	std::vector<unsigned int> guids;


	for (auto& entry : dictionary)
	{

		float match = compare_hu_moments(icon_moments, entry.second);
		if (match == best_match)
		{
			guids.push_back(entry.first);
		}
		else if (match > best_match)
		{
			guids.clear();
			guids.push_back(entry.first);
			best_match = match;
		}
	}

	//if (best_match > 150)
	//	return std::vector<unsigned int>();

	if (verbose) {
		for (unsigned int guid : guids)
			std::cout << guid << ", ";
		std::cout << "(" << best_match << ")\t";
	}
	return guids;
}


std::vector<unsigned int> image_recognition::get_guid_from_icon(const cv::Mat& icon, const std::map<unsigned int, cv::Mat>& dictionary, const cv::Scalar& background_color) const
{
	if (icon.empty())
		return std::vector<unsigned int>();

	return get_guid_from_icon(icon, dictionary,
		cv::Mat(icon.rows, icon.cols, CV_8UC4, background_color));
}


std::vector<unsigned int> image_recognition::get_guid_from_name(const cv::Mat& text_img,
	const std::map<unsigned int, std::string>& dictionary)
{
#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
	cv::imwrite("debug_images/text_img.png", text_img);
#endif
	std::vector<std::pair<std::string, cv::Rect>> words = detect_words(text_img, tesseract::PageSegMode::PSM_SINGLE_LINE);
	std::string building_string;
	for (const auto& word : words)
	{
		//stop concatenation before opening bracket
		std::vector<std::string> split_string;
		boost::split(split_string, word.first, [](char c) {return c == '('; });
		if (split_string.size() > 1)
		{
			building_string += split_string.front();
			break;
		}
		else
		{
			building_string += word.first;
		}
	}

#ifdef CONSOLE_DEBUG_OUTPUT
	std::cout << building_string << "\t";
#endif

	return get_guid_from_name(building_string, dictionary);
}

std::vector<unsigned int> image_recognition::get_guid_from_name(const std::string& building_string, const std::map<unsigned int, std::string>& dictionary)
{
	std::vector<unsigned int> guids;
	float best_match = 0.f;
	for (const auto& entry : dictionary)
	{
		std::vector<std::string> split_string;
		boost::split(split_string, entry.second, [](char c) {return c == ' '; });
		std::string kw = boost::join(split_string, "");

		float total_length = std::max(kw.size(), building_string.size());
		int min_lcs_length = total_length - static_cast<int>(std::roundf(-0.677f + 1.51 * std::logf(total_length)));
		int lcs_length = image_recognition::lcs_length(kw, building_string);
		float match = lcs_length / total_length;

		if (lcs_length >= min_lcs_length)
		{
			if (match == best_match)
			{
				guids.push_back(entry.first);
			}
			else if (match > best_match)
			{
				guids.clear();
				guids.push_back(entry.first);
				best_match = match;
			}
		}
	}
	return guids;
}


double image_recognition::compare_hu_moments(const std::vector<double>& ma, const std::vector<double>& mb)
{
	// execute return cv::matchShapes(ma, mb, cv::CONTOURS_MATCH_I2, 0.);
	// with pre-computed hu moments

	int i, sma, smb;
	double eps = 1.e-5;
	double result = 0;
	bool anyA = false, anyB = false;


	for (i = 0; i < 7; i++)
	{
		double ama = fabs(ma[i]);
		double amb = fabs(mb[i]);

		if (ama > 0)
			anyA = true;
		if (amb > 0)
			anyB = true;

		if (ma[i] > 0)
			sma = 1;
		else if (ma[i] < 0)
			sma = -1;
		else
			sma = 0;
		if (mb[i] > 0)
			smb = 1;
		else if (mb[i] < 0)
			smb = -1;
		else
			smb = 0;

		if (ama > eps && amb > eps)
		{
			ama = sma * log10(ama);
			amb = smb * log10(amb);
			result += fabs(-ama + amb);
		}
	}

	if (anyA != anyB)
		result = DBL_MAX;

	return result;
}



std::pair<cv::Rect, float> image_recognition::match_template(const cv::Mat& source, const cv::Mat& template_img)
{
	cv::Mat src_hs = convert_color_space_for_template_matching(source);
	cv::Mat tmpl_hs = convert_color_space_for_template_matching(template_img);

	cv::Mat result;
	cv::matchTemplate(src_hs, tmpl_hs, result, cv::TM_SQDIFF);
	result /= tmpl_hs.size().area();
	cv::Point min_loc, max_loc;
	double min, max;
	cv::minMaxLoc(result, &min, &max, &min_loc, &max_loc);

	cv::Point template_position = min_loc;

#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
	cv::imwrite("debug_images/match_image.bmp", (result) * 256 / max);
#endif //SHOW_CV_DEBUG_IMAGE_VIEW

	return { cv::Rect(template_position, tmpl_hs.size()), min };
}



cv::Mat image_recognition::load_image(const std::string& path)
{
	cv::Mat img = cv::imread(path, cv::IMREAD_UNCHANGED);
	if (img.size().area() < 1) {
		throw std::invalid_argument("failed to load " + path);
	}
	if (!img.empty())
		cv::cvtColor(img, img, cv::COLOR_BGR2BGRA);

	return img;
}

cv::Mat image_recognition::crop_widescreen(const cv::Mat& img)
{
	float width = img.cols;
	float height = img.rows;
	if (img.rows && std::abs(16.f / 9.f - (width / height)) >= 1.f / 32.f)
	{
		if (9 * width > 16 * height)
		{
			int new_width = height * 16 / 9;
			int crop = (width - new_width) / 2;
			cv::Rect roi(crop, 0, new_width, height);
			return img(roi);
		}
		else
		{
			int new_height = width * 9 / 16;
			int crop = (height - new_height) / 2;
			cv::Rect roi(0, crop, width, new_height);
			return img(roi);
		}
	}
	else
	{
		return img;
	}
}

cv::Mat image_recognition::binarize(const cv::Mat& input, bool invert, bool multi_channel)
{
	if (input.empty())
		return input;

	cv::Mat resized, thresholded;
	if (input.rows < 40)
	{
		float scale = 45.f / input.rows;
		cv::resize(input, resized, cv::Size(), scale, scale, cv::INTER_CUBIC);
	}
	else
		resized = input;

	cv::cvtColor(resized, thresholded, cv::COLOR_BGRA2GRAY);
	cv::threshold(thresholded, thresholded, 128, 255, (invert ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY) | cv::THRESH_OTSU);
	if (multi_channel)
		cv::cvtColor(thresholded, thresholded, cv::COLOR_GRAY2RGBA);

	return thresholded;
}



cv::Mat image_recognition::binarize_icon(const cv::Mat& input, cv::Size target_size)
{
	if (input.empty())
		return input;

	cv::Mat thresholded;
	cv::Mat input_alpha_applied = blend_icon(input, cv::Scalar(0, 0, 0));
	cv::cvtColor(input_alpha_applied, thresholded, cv::COLOR_BGRA2GRAY);
	cv::threshold(thresholded, thresholded, 128, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
	if (thresholded.at<unsigned char>(thresholded.rows - 1, thresholded.cols - 1) > 128)
		thresholded = 255 - thresholded;

#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
	cv::imwrite("debug_images/icon_gray.png", thresholded);
#endif

	cv::Mat edge_image;
	cv::Canny(thresholded, edge_image, 20, 60, 3);
	std::vector < std::vector<cv::Point>> contours;
	std::vector<cv::Vec4i> hierarchy;
	cv::findContours(edge_image, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_NONE);
	contours.erase(std::remove_if(
		contours.begin(),
		contours.end(),
		[](const std::vector<cv::Point>& e) {return e.size() < 10; }),
		contours.end());

	if (contours.empty())
		return cv::Mat(target_size, CV_8UC4);

	std::vector<cv::Point > contour_points;
	for (auto& contour : contours)
	{
		contour.push_back(contour.front());
		contour_points.insert(contour_points.end(), contour.begin(), contour.end());
	}
	cv::Rect roi = boundingRect(contour_points);

	cv::Mat output = thresholded(roi);

	if (target_size.width && target_size.height)
		cv::resize(output, output, target_size, 0, 0, cv::INTER_NEAREST);

#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
	cv::imwrite("debug_images/icon_shilouette.png", output);
#endif

	return output;
}

cv::Mat image_recognition::convert_color_space_for_template_matching(const cv::Mat& bgr_in)
{
	std::vector<cv::Mat> channels;
	cv::split(bgr_in, channels);
	for (auto& c : channels)
	{
		double min, max;
		cv::minMaxLoc(c, &min, &max);
		cv::threshold(c, c, 0.5f * (min + max), 255.f, cv::THRESH_BINARY);
	}
	cv::Mat ret;
	cv::merge(channels.data(), 3, ret);

	return ret;
}

cv::Mat image_recognition::gamma_invariant_hue_finlayson(const cv::Mat& bgr_in)
{
	std::vector<cv::Mat> channels;
	cv::split(bgr_in, channels);
	for (auto& c : channels) {
		c.convertTo(c, CV_32F);
		c = c / 255.f;
	}
	/*{
		std::ofstream of("pixel_red.csv", std::ios::out);
		for (int i = 0; i < channels[0].rows; i++)
			for (int j = 0; j < channels[0].cols; j++)
				of << channels[0].at<float>(i, j) << std::endl;

	}*/
	for (auto& c : channels) {
		cv::log(c, c);
	}
	cv::Mat ret[3];
	cv::divide((channels[2] - channels[1]), (channels[2] + channels[1] - 2.f * channels[0]), ret[0]);
	cv::divide((channels[0] - channels[2]), (channels[0] + channels[2] - 2.f * channels[1]), ret[1]);
	cv::divide((channels[1] - channels[0]), (channels[1] + channels[0] - 2.f * channels[2]), ret[2]);
	for (int i = 0; i < 3; i++) {
		/*{
			std::ofstream of("pixel_values_raw.csv", std::ios::out);
			for (int i = 0; i < ret.rows; i++)
				for (int j = 0; j < ret.cols; j++)
					of << ret.at<float>(i, j) << std::endl;

		}*/
		cv::threshold(ret[i], ret[i], 2.f, 1.f, cv::THRESH_TRUNC);
		ret[i] = -1.f * ret[i];
		cv::threshold(ret[i], ret[i], 2.f, 1.f, cv::THRESH_TRUNC);
		ret[i] = -1.f * ret[i];
		/*{
			std::ofstream of("pixel_values_pre_norm.csv", std::ios::out);
			for (int i = 0; i < ret[i].rows; i++)
				for (int j = 0; j < ret[i].cols; j++)
					of << ret[i].at<float>(i, j) << std::endl;

		}*/

		//cv::normalize(ret[i], ret[i], 0, 255, cv::NORM_MINMAX);
		ret[i] = ret[i] + 2.f;
		ret[i] = ret[i] / 4.f;
		ret[i] = ret[i] * 255.f;
		cv::imwrite("debug_images/ret" + std::to_string(i) + ".png", ret[i]);
		//H = (log(R)-log(G))/(log(R)+log(G)-2log(B))

		/*{
			std::ofstream of("pixel_values_post_norm.csv", std::ios::out);

			for (int i = 0; i < ret.rows; i++)
				for (int j = 0; j < ret.cols; j++)
					of << ret.at<float>(i, j) << std::endl;
		}*/
	}
#ifdef CONSOLE_DEBUG_OUTPUT
	std::cout << "done with gamma" << std::endl;
#endif
	return ret[0];
}

void image_recognition::write_image_per_channel(const std::string& path, const cv::Mat& img)
{
	cv::Mat mat = img;
	std::vector<cv::Mat> channels;
	cv::split(mat, channels);
	for (int i = 0; i < channels.size(); i++) {
		cv::imwrite(path + "_" + std::to_string(i) + ".png", channels[i]);
	}
}

cv::Rect image_recognition::get_aa_bb(const std::list<cv::Point>& input)
{
	if (input.empty())
		return cv::Rect(cv::Point(0, 0), cv::Point(0, 0));

	cv::Point min(*input.begin());
	cv::Point max(*input.begin());

	for (const cv::Point& p : input) {
		min.x = std::min(min.x, p.x);
		min.y = std::min(min.y, p.y);
		max.x = std::max(max.x, p.x);
		max.y = std::max(max.y, p.y);
	}
	return cv::Rect(min, max + cv::Point(1, 1));
}

void image_recognition::update(const std::string& language)
{
	auto my_language = has_language(language) ? language : "english";

	update_ocr(my_language/*, number_mode*/);


}

cv::Rect2i image_recognition::find_anno()
{
	try {
		std::string window_name_regex_string(window_regex);
		std::regex window_name_regex(window_name_regex_string.data());

		HWND hwnd = NULL;
		int area = 0;
		struct lambda_parameter {
			HWND* hwnd_p;
			std::regex window_name_regex;
			int* area;
		} params{ &hwnd, window_name_regex, &area };

		EnumWindows(
			[](HWND local_hwnd, LPARAM lparam) {
				size_t length = GetWindowTextLength(local_hwnd);

				if (length == 0) {
					return TRUE;
				}

				wchar_t* buffer = new wchar_t[length + 1];
				GetWindowText(local_hwnd, buffer, length + 1);
				std::wstring  windowTitle(buffer);

				if (std::regex_match(to_string(windowTitle), ((lambda_parameter*)lparam)->window_name_regex)) {
					RECT windowsize;    // get the height and width of the screen
					GetWindowRect(local_hwnd, &windowsize);

					int area = (windowsize.bottom - windowsize.top) * (windowsize.right - windowsize.left);
					if (*(((lambda_parameter*)lparam)->area))
					{
						std::cout << "WARNING: Multiple windows with title 'Anno 1404' detected. The server may not work because it captures the wrong one." << std::endl;
					}

					if (*(((lambda_parameter*)lparam)->area) < area)
					{
						*(((lambda_parameter*)lparam)->area) = area;
						*(((lambda_parameter*)lparam)->hwnd_p) = local_hwnd;
					}
				}
				return TRUE;
			}, (LPARAM)&params);

		if (hwnd == NULL)
		{
			if (verbose) {
				std::cout << "Can't find window with regex " << window_name_regex_string << std::endl
					<< "open windows are:" << std::endl;

				//print all open window titles
				EnumWindows([](HWND local_hwnd, LPARAM lparam) {
					size_t length = GetWindowTextLength(local_hwnd);

					if (length == 0) {
						return TRUE;
					}

					wchar_t* buffer = new wchar_t[length + 1];
					GetWindowText(local_hwnd, buffer, length + 1);
					std::wstring  windowTitle(buffer);

					std::cout << local_hwnd << ":  " << to_string(windowTitle) << std::endl;
					if (length > 0)
					{
						std::cout << "match result "
							<< std::regex_match(to_string(windowTitle), ((lambda_parameter*)lparam)->window_name_regex) << std::endl;
					}
					return TRUE;
					}, (LPARAM)&params);
			}
			else {
				std::cout << "Anno 1404 window not found" << std::endl;
			}

			return cv::Rect2i();
		}

		RECT windowsize;    // get the height and width of the screen
		GetWindowRect(hwnd, &windowsize);

		return cv::Rect2i(windowsize.left, windowsize.top, windowsize.right - windowsize.left, windowsize.bottom - windowsize.top);
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
		return cv::Rect2i();
	}
}

cv::Rect2i image_recognition::get_desktop()
{
	RECT windowsize;

	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &windowsize);

	return cv::Rect2i(0, 0, windowsize.right, windowsize.bottom);
}

cv::Mat image_recognition::take_screenshot(cv::Rect2i rect)
{
	cv::Rect window = rect;

	if (!rect.area())
		window = get_desktop();

	HDC hwindowDC = GetDC(NULL);
	HDC hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);

	// create a bitmap
	HBITMAP hbwindow = CreateCompatibleBitmap(hwindowDC, window.width, window.height);

	BITMAPINFOHEADER  bi;
	bi.biSize = sizeof(BITMAPINFOHEADER);    //http://msdn.microsoft.com/en-us/library/windows/window/dd183402%28v=vs.85%29.aspx
	bi.biWidth = window.width;
	bi.biHeight = -window.height;  //this is the line that makes it draw upside down or not
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	cv::Mat src(window.height, window.width, CV_8UC4);

	// use the previously created device context with the bitmap
	SelectObject(hwindowCompatibleDC, hbwindow);
	// copy from the window device context to the bitmap device context
	StretchBlt(hwindowCompatibleDC, 0, 0, window.width, window.height, hwindowDC, window.x, window.y, window.width, window.height, SRCCOPY); //change SRCCOPY to NOTSRCCOPY for wacky colors !
	GetDIBits(hwindowCompatibleDC, hbwindow, 0, window.height, src.data, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);  //copy from hwindowCompatibleDC to hbwindow

	// avoid memory leak
	DeleteObject(hbwindow);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(nullptr, hwindowDC);

	if (verbose) {
		if (!std::filesystem::is_directory("debug_images") || !std::filesystem::exists("debug_images")) { // Check if src folder exists
			std::filesystem::create_directory("debug_images"); // create src folder
		}

		cv::imwrite("debug_images/screenshot-" + std::to_string(verbose_screenshot_counter) + ".png", src);
		if (++verbose_screenshot_counter > 20)
			verbose_screenshot_counter = 0;
	}

	return src;

}

std::vector<std::pair<std::string, cv::Rect>> image_recognition::detect_words(const cv::Mat& in, const tesseract::PageSegMode mode, bool numbers_only)
{
	cv::Mat input = in;
	std::vector<std::pair<std::string, cv::Rect>> ret;

	update_ocr(ocr_language/*, numbers_only*/);

	try {
		const auto& cr = ocr;
		cr->SetPageSegMode(mode);

		// Set image data
		cr->SetImage(input.data, input.cols, input.rows, 4, input.step);

		cr->Recognize(0);
		tesseract::ResultIterator* ri = cr->GetIterator();
		tesseract::PageIteratorLevel level = tesseract::RIL_WORD;



		if (ri != 0) {
			ret.reserve(10);
			do {
				const char* word = ri->GetUTF8Text(level);
				float conf = ri->Confidence(level);
				int x1, y1, x2, y2;
				ri->BoundingBox(level, &x1, &y1, &x2, &y2);
				//if(verbose){
				//				printf("word: '%s';\t\tconf: %.2f; BoundingBox: %d,%d,%d,%d;\n",
				//					word, conf, x1, y1, x2, y2);
				//}
				std::string word_s = word ? std::string(word) : std::string();
				cv::Rect aa_bb(cv::Point(x1, y1), cv::Point(x2, y2));
				ret.push_back(std::make_pair(word_s, aa_bb));
				delete[] word;
			} while (ri->Next(level));
		}
	}
	catch (...) {}

	return ret;
}



bool image_recognition::has_language(const std::string& language) const
{
	return dictionaries.find(language) != dictionaries.end() && tesseract_languages.find(language) != tesseract_languages.end();
}

bool image_recognition::is_verbose() const
{
	return verbose;
}

const keyword_dictionary& image_recognition::get_dictionary() const
{
	auto iter = dictionaries.find(ocr_language);
	if (iter == dictionaries.end())
		throw std::exception("language not found");
	return iter->second;
}

std::map<unsigned int, std::string>  image_recognition::make_dictionary(const std::vector<phrase>& list) const
{
	std::map<unsigned int, std::string> result;
	const std::map<unsigned int, std::string>& source = get_dictionary().ui_texts;

	for (phrase w : list)
	{
		result[(unsigned int)w] = source.at((unsigned int)w);
	}

	return result;
}

std::vector<double> image_recognition::get_hu_moments(cv::Mat img)
{
	cv::Moments moments = cv::moments(detect_edges(img));
	std::vector<double> hu_moments;
	cv::HuMoments(moments, hu_moments);

	return hu_moments;
}

cv::Mat image_recognition::detect_edges(const cv::Mat& im)
{
	cv::Mat canny, grey, edges;
	if (!im.empty())
	{
		cv::cvtColor(im, grey, cv::COLOR_BGRA2GRAY);
		cv::Canny(grey, edges, 30, 50);
	}

	return edges;
}

std::vector<cv::Rect2i> image_recognition::detect_boxes(const cv::Mat& im, const cv::Rect2i& box, const cv::Rect2i& ignore_region, float tolerance,
	double threshold1, double threshold2)
{
	return detect_boxes(im, box.width, box.height, ignore_region, tolerance, threshold1, threshold2);
}

std::vector<cv::Rect2i> image_recognition::detect_boxes(const cv::Mat& im, unsigned int width, unsigned int height, const cv::Rect2i& ignore_region, float tolerance,
	double threshold1, double threshold2)
{
	cv::Mat edge_image;
	cv::Canny(im, edge_image, threshold1, threshold2, 3);
	cv::rectangle(edge_image, ignore_region, cv::Scalar::all(0), cv::FILLED);

#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
	cv::Mat colored_edge_image;
	cv::cvtColor(edge_image, colored_edge_image, cv::COLOR_GRAY2BGRA);
#endif

	std::vector < std::vector<cv::Point>> contours;
	std::vector<cv::Vec4i> hierarchy;
	cv::findContours(edge_image, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

	std::vector<cv::Point> approx;
	std::vector<std::pair<cv::Rect2i, std::vector<cv::Point>>> box_candidates;
	std::vector<cv::Rect2i> boxes;

	cv::Rect2i min_bb(0, 0, width * (1.f - tolerance), height * (1.f - tolerance));
	cv::Rect2i max_bb(0, 0, width * (1.f + tolerance), height * (1.f + tolerance));

	auto angle = [](cv::Point& pt1, cv::Point& pt2, cv::Point& pt0)
	{
		double dx1 = pt1.x - pt0.x;
		double dy1 = pt1.y - pt0.y;
		double dx2 = pt2.x - pt0.x;
		double dy2 = pt2.y - pt0.y;
		return (dx1 * dx2 + dy1 * dy2) / sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-10);
	};

	for (size_t i = 0; i < contours.size(); i++)
	{
		cv::Rect2i bb(cv::boundingRect(contours[i]));

		if (bb.height > 0.5f * min_bb.height || bb.width > 0.5f * min_bb.width)
		{
			bool added = false;
			for (auto iter = box_candidates.begin(); iter != box_candidates.end(); ++iter)
			{
				if (((iter->first) & bb).area())
				{
					added = true;
					std::copy(contours[i].begin(), contours[i].end(), std::back_inserter(iter->second));
					iter->first = cv::boundingRect(iter->second);
					break;
				}
			}

			if (!added)
			{
				box_candidates.emplace_back(bb, contours[i]);
			}
		}
	}

	for(const auto& entry : box_candidates){
		const auto& bb = entry.first;


		if (min_bb.width <= bb.width && bb.width <= max_bb.width &&
			min_bb.height <= bb.height && bb.height <= max_bb.height)
		{

			boxes.push_back(bb);

#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
			cv::rectangle(colored_edge_image, bb, cv::Scalar(0, 0, 255, 255));
#endif
			
		}
	}

#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
	cv::imwrite("debug_images/boxes.png", colored_edge_image);
#endif


	return boxes;
}

std::vector<int> image_recognition::find_horizontal_lines(const cv::Mat& im, float line_density)
{
#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
	cv::imwrite("debug_images/scroll_area.png", im);
#endif

	cv::Mat edges = detect_edges(im);

#ifdef SHOW_CV_DEBUG_IMAGE_VIEW
	cv::imwrite("debug_images/scroll_area_edges.png", edges);
#endif

	std::vector<cv::Vec4i> lines;
	cv::HoughLinesP(edges, lines, 2, CV_PI / 2, im.cols / 2.f, im.cols * line_density, im.cols * 0.15f);

	std::vector<int> hlines;
	for (auto& line : lines)
	{
		if (std::abs(line[1] - line[3]) <= 2 &&
			std::abs(line[2] - line[0]) > 0.8f * im.cols)
		{
			hlines.push_back(line[1]);
		}
	}
	std::sort(hlines.begin(), hlines.end());
	return hlines;
}

void image_recognition::iterate_rows(const cv::Mat& im, float line_density,
	const std::function<void(const cv::Mat& row)> f)
{
	std::vector<int> lines(find_horizontal_lines(im));

	if (!lines.size())
		return;

	std::vector<int> heights;
	int prev_hline = 0;
	for (int hline : lines)
	{
		int height = hline - prev_hline;
		if (height > 10)
		{
			heights.push_back(height);
		}
		prev_hline = hline;
	}

	std::sort(heights.begin(), heights.end());
	if (heights.empty())
		return;

	prev_hline = 0;
	int mean_row_height = heights[heights.size() / 2];
	int row_height = 0;

	for (auto hline = lines.begin(); hline != lines.end(); ++hline)
	{
		int height = *hline - prev_hline;
		if (height < 10)
		{
			prev_hline = *hline;
			continue;
		}

		auto next_hline = hline;
		while (height <= 0.9 * mean_row_height && next_hline != lines.end())
		{
			++next_hline;
			height = *next_hline - prev_hline;
		}

		if (height > 0.9 * mean_row_height && height < 1.1 * mean_row_height)
		{
			row_height = height;
			f(im(cv::Rect(0, prev_hline, im.cols, height)));
		}
		else
			next_hline = hline;

		prev_hline = *next_hline;
	}

	if (row_height)
	{
		int height = lines.back() + row_height < im.rows ? row_height : im.rows - lines.back();
		if (height > 10 && height > row_height * 0.95f)
			f(im(cv::Rect(0, lines.back(), im.cols, height)));
	}
}




int image_recognition::number_from_region(const cv::Mat& im)
{
	std::string number_string = join(detect_words(im, tesseract::PageSegMode::PSM_SINGLE_LINE, true));

#ifdef CONSOLE_DEBUG_OUTPUT
	std::cout << number_string << "\t";
#endif

	int number = number_from_string(number_string);
	if (verbose)
		std::cout << " (" << number_string << ", " << number << ") ";

	return number;
}

int image_recognition::number_from_string(const std::string& word)
{
	std::string number_string = word;
	for(const auto& entry : letter_to_digit)
	{
		std::regex expr(entry.first);
		number_string = std::regex_replace(number_string, expr, entry.second);
	}


	number_string = std::regex_replace(number_string, std::regex("\\D"), "");

	try { return std::stoi(number_string); }
	catch (...) {
#ifdef CONSOLE_DEBUG_OUTPUT
		std::cout << "could not match number string: " << number_string << std::endl;
#endif
	}
	return std::numeric_limits<int>::lowest();
}






// Function to find length of Longest Common Subsequence of substring
// X[0..m-1] and Y[0..n-1]
// From https://www.techiedelight.com/longest-common-subsequence/
int image_recognition::lcs_length(std::string X, std::string Y)
{
	int m = X.length(), n = Y.length();

	// lookup table stores solution to already computed sub-problems
	// i.e. lookup[i][j] stores the length of LCS of substring
	// X[0..i-1] and Y[0..j-1]
	std::vector<std::vector<int>> lookup;
	lookup.resize(m + 1);
	for (auto& row : lookup)
		row.resize(n + 1);

	// first column of the lookup table will be all 0
	for (int i = 0; i <= m; i++)
		lookup[i][0] = 0;

	// first row of the lookup table will be all 0
	for (int j = 0; j <= n; j++)
		lookup[0][j] = 0;

	// fill the lookup table in bottom-up manner
	for (int i = 1; i <= m; i++)
	{
		for (int j = 1; j <= n; j++)
		{
			// if current character of X and Y matches
			if (X[i - 1] == Y[j - 1])
				lookup[i][j] = lookup[i - 1][j - 1] + 1;

			// else if current character of X and Y don't match
			else
				lookup[i][j] = std::max(lookup[i - 1][j], lookup[i][j - 1]);
		}
	}

	// LCS will be last entry in the lookup table
	return lookup[m][n];
}

std::string image_recognition::join(const std::vector<std::pair<std::string, cv::Rect>>& words, bool insert_spaces) const
{
	std::string result;

	for (const auto& pair : words)
	{
		result += pair.first;
		if (insert_spaces)
			result += " ";
	}

	if (insert_spaces)
		result.pop_back();

	return result;
}

void image_recognition::update_ocr(const std::string& language/*, bool numbers_only*/)
{
	if (ocr && !ocr_language.compare(language) /*&& numbers_only == number_mode*/)
		return;

	if (verbose) {
		std::cout << "Update tesseract language " << language /*<< " number only " << numbers_only*/ << std::endl;
	}



	const char* lang = tesseract_languages.find(language)->second.c_str();
	ocr.reset(new tesseract::TessBaseAPI());

	GenericVector<STRING> keys;
	GenericVector<STRING> values;

	keys.push_back("user_defined_dpi");
	values.push_back("70");

	//if (numbers_only)
	//{
	//	keys.push_back("tessedit_char_whitelist");
	//	values.push_back("0123456789,.;:()%/");
	//}

	/*	keys.push_back("textord_min_xheight"); values.push_back("8");
		keys.push_back("stopper_smallword_size"); values.push_back("1");
		keys.push_back("quality_min_initial_alphas_reqd"); values.push_back("1");
		keys.push_back("tessedit_preserve_min_wd_len"); values.push_back("1");

		keys.push_back("language_model_penalty_non_dict_word"); values.push_back("1");
		keys.push_back("segment_penalty_dict_nonword"); values.push_back("10");*/
		//keys.push_back("load_system_dawg"); values.push_back("F");
		//keys.push_back("load_freq_dawg"); values.push_back("F");
		//keys.push_back("user_words_suffix"); values.push_back((std::string(lang) + std::string(".user-words.txt")).c_str());
		//keys.push_back("user_patterns_suffix"); values.push_back((std::string(lang) + std::string(".user-patterns.txt")).c_str());

	if (ocr->Init(NULL, lang ? lang : "eng", tesseract::OEM_DEFAULT, nullptr, 0, &keys, &values, false))
	{
		std::cout << "error initialising tesseract" << std::endl;
	}

	//		ocr_->SetVariable("CONFIGFILE", "bazaar");
	ocr_language = lang ? language : "english";
	//number_mode = numbers_only;

}

const std::map<std::string, std::string> image_recognition::tesseract_languages = {
	{"german", "deu"},
	{"english", "eng"},
	{"french", "fra"},	
	{"spanish", "spa"},
	{"italian", "ita"},
	{"polish", "pol"},
	{"russian", "rus"}
};

const std::map<std::string, std::string> image_recognition::letter_to_digit = {
		{"Â£", "5"},
		{"O", "0"},
		{"Q", "0"},
		{"I", "1"},
		{"Z", "2"},
		{"B", "8"}
};

const std::string image_recognition::ALL_ISLANDS = std::string("All Islands");

}
