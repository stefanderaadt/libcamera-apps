/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021, Raspberry Pi (Trading) Ltd.
 *
 * libcamera_detect.cpp - take pictures when objects are detected
 */

// Example: libcamera-detect --post-process-file object_detect_tf.json --lores-width 400 --lores-height 300 -t 0 --object cat -o cat%03d.jpg

#include <chrono>

#include "core/libcamera_app.hpp"
#include "core/still_options.hpp"

#include "image/image.hpp"

#include "core/libcamera_encoder.hpp"
#include "output/output.hpp"

static int get_colourspace_flags(std::string const &codec)
{
	if (codec == "mjpeg" || codec == "yuv420")
		return LibcameraEncoder::FLAG_VIDEO_JPEG_COLOURSPACE;
	else
		return LibcameraEncoder::FLAG_VIDEO_NONE;
}

// The main even loop for the application.
static void event_loop(LibcameraEncoder &app)
{
	VideoOptions const *options = app.GetOptions();
	app.OpenCamera();
	app.ConfigureVideo(get_colourspace_flags(options->codec));
	app.StartEncoder();
	app.StartCamera();
	auto start_time = std::chrono::high_resolution_clock::now();

	for (unsigned int count = 0;; count++)
	{
		LibcameraEncoder::Msg msg = app.Wait();
		if (msg.type == LibcameraEncoder::MsgType::Quit)
			return;
		else if (msg.type != LibcameraEncoder::MsgType::RequestComplete)
			throw std::runtime_error("unrecognised message!");

		CompletedRequestPtr &completed_request = std::get<CompletedRequestPtr>(msg.payload);

		auto now = std::chrono::high_resolution_clock::now();
		if (options->timeout && now - start_time > std::chrono::milliseconds(options->timeout))
			return;

		LOG(1, "Start1");

		bool motion_detected = false;
		LOG(1, "Start2");
		completed_request->post_process_metadata.Get("motion_detect.result", motion_detected);
		LOG(1, "Start3");

		if (motion_detected)
		{
			app.StopCamera();
			app.Teardown();
			app.ConfigureStill();
			app.StartCamera();
			LOG(1, "MOTION detected");
		}

		LOG(1, "Start4");

		// Stream video
		app.EncodeBuffer(completed_request, app.VideoStream());

		LOG(1, "Start5");
	}
}

int main(int argc, char *argv[])
{
	try
	{
		LibcameraEncoder app;
		VideoOptions *options = app.GetOptions();
		if (options->Parse(argc, argv))
		{
			if (options->verbose >= 2)
				options->Print();

			event_loop(app);
		}
	}
	catch (std::exception const &e)
	{
		LOG_ERROR("ERROR: *** " << e.what() << " ***");
		return -1;
	}
	return 0;
}
