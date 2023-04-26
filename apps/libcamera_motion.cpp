/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * libcamera_vid.cpp - libcamera video record app.
 */

#include <chrono>
#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/stat.h>

#include "core/libcamera_encoder.hpp"
#include "output/circular_output.hpp"
#include "output/output.hpp"

using namespace std::placeholders;

// Some keypress/signal handling.

static int signal_received;
static void default_signal_handler(int signal_number)
{
	signal_received = signal_number;
	LOG(1, "Received signal " << signal_number);
}

static int get_key_or_signal(VideoOptions const *options, pollfd p[1])
{
	int key = 0;
	if (options->keypress)
	{
		poll(p, 1, 0);
		if (p[0].revents & POLLIN)
		{
			char *user_string = nullptr;
			size_t len;
			[[maybe_unused]] size_t r = getline(&user_string, &len, stdin);
			key = user_string[0];
		}
	}
	if (options->signal)
	{
		if (signal_received == SIGUSR1)
			key = '\n';
		else if (signal_received == SIGUSR2)
			key = 'x';
		signal_received = 0;
	}
	return key;
}

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
	// Default output
	VideoOptions const *options = app.GetOptions();
	std::unique_ptr<Output> output = std::unique_ptr<Output>(Output::Create(options));

	// Circular output
	std::unique_ptr<Output> circular_output = std::unique_ptr<Output>((Output *)new CircularOutput(options));
	app.SetEncodeOutputReadyCallback(
		[&output, &circular_output](void *mem, size_t size, int64_t timestamp_us, bool keyframe)
		{
			output.get()->OutputReady(mem, size, timestamp_us, keyframe);
			if (circular_output.get() != nullptr)
			{
				circular_output.get()->OutputReady(mem, size, timestamp_us, keyframe);
			}
		});
	app.SetMetadataReadyCallback(
		[&output, &circular_output](libcamera::ControlList &metadata)
		{
			output.get()->MetadataReady(metadata);
			if (circular_output.get() != nullptr)
			{
				circular_output.get()->MetadataReady(metadata);
			}
		});

	bool recording_motion = false;

	// Open camera
	app.OpenCamera();
	app.ConfigureVideo(get_colourspace_flags(options->codec));
	app.StartEncoder();
	app.StartCamera();
	auto last_motion_time = std::chrono::high_resolution_clock::now();

	// Monitoring for keypresses and signals.
	signal(SIGUSR1, default_signal_handler);
	signal(SIGUSR2, default_signal_handler);
	pollfd p[1] = { { STDIN_FILENO, POLLIN, 0 } };

	for (unsigned int count = 0;; count++)
	{
		LibcameraEncoder::Msg msg = app.Wait();
		if (msg.type == LibcameraApp::MsgType::Timeout)
		{
			LOG_ERROR("ERROR: Device timeout detected, attempting a restart!!!");
			app.StopCamera();
			app.StartCamera();
			continue;
		}
		if (msg.type == LibcameraEncoder::MsgType::Quit)
			return;
		else if (msg.type != LibcameraEncoder::MsgType::RequestComplete)
			throw std::runtime_error("unrecognised message!");
		int key = get_key_or_signal(options, p);
		if (key == '\n')
			output->Signal();

		// LOG(1, "Viewfinder frame " << count);
		auto now = std::chrono::high_resolution_clock::now();

		CompletedRequestPtr &completed_request = std::get<CompletedRequestPtr>(msg.payload);

		app.EncodeBuffer(completed_request, app.VideoStream());

		bool motion_detected = false;
		completed_request->post_process_metadata.Get("motion_detect.result", motion_detected);

		if (motion_detected && now - last_motion_time > std::chrono::milliseconds(options->motion_delay))
		{
			LOG(1, "motion detected recording... Delay: " << options->motion_delay);
			recording_motion = true;
			last_motion_time = std::chrono::high_resolution_clock::now();
		}

		// Wait for half of motion delay to get a recording around the detected motion
		if (recording_motion && now - last_motion_time > std::chrono::milliseconds(options->motion_delay / 2))
		{
			LOG(1, "motion detected saving...");
			recording_motion = false;

			// Delete old circular_output and trigger destructor to save to file
			circular_output.reset();
			// Create new circular_output
			circular_output = std::unique_ptr<Output>((Output *)new CircularOutput(options));
		}
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
