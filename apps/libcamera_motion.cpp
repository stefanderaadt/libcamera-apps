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

// The main even loop for the application.

static void event_loop(LibcameraEncoder &app)
{
	VideoOptions const *options = app.GetOptions();
	app.OpenCamera();
	app.ConfigureViewfinder();
	app.StartCamera();
	auto start_time = std::chrono::high_resolution_clock::now();
	unsigned int last_capture_frame = 0;

	for (unsigned int count = 0;; count++)
	{
		LibcameraApp::Msg msg = app.Wait();
		if (msg.type == LibcameraApp::MsgType::Quit)
			return;

		CompletedRequestPtr &completed_request = std::get<CompletedRequestPtr>(msg.payload);

		auto now = std::chrono::high_resolution_clock::now();
		if (options->timeout && now - start_time > std::chrono::milliseconds(options->timeout))
			return;

		bool motion_detected = false;
		completed_request->post_process_metadata.Get("motion_detect.result", motion_detected);

		if (motion_detected)
		{
			app.StopCamera();
			app.Teardown();
			app.ConfigureStill();
			app.StartCamera();
			LOG(1, "MOTION detected");
		}

		// Stream video
		app.EncodeBuffer(completed_request, app.VideoStream());
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
