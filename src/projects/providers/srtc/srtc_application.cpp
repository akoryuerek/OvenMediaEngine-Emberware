//==============================================================================
//
//  OvenMediaEngine - SRT Caller Application
//
//  Created for Emberware
//  Copyright (c) 2024 Emberware. All rights reserved.
//
//==============================================================================

#include "srtc_application.h"
#include "srtc_stream.h"

#define OV_LOG_TAG "SrtcApplication"

namespace pvd
{
	std::shared_ptr<SrtcApplication> SrtcApplication::Create(
		const std::shared_ptr<pvd::PullProvider> &provider,
		const info::Application &app_info)
	{
		auto application = std::make_shared<SrtcApplication>(provider, app_info);
		if (!application->Start())
		{
			return nullptr;
		}
		return application;
	}

	SrtcApplication::SrtcApplication(
		const std::shared_ptr<pvd::PullProvider> &provider,
		const info::Application &info)
		: PullApplication(provider, info)
	{
	}

	std::shared_ptr<pvd::PullStream> SrtcApplication::CreateStream(
		const uint32_t stream_id,
		const ov::String &stream_name,
		const std::vector<ov::String> &url_list,
		const std::shared_ptr<pvd::PullStreamProperties> &properties)
	{
		return SrtcStream::Create(
			std::static_pointer_cast<pvd::PullApplication>(GetSharedPtr()),
			stream_id,
			stream_name,
			url_list,
			properties);
	}
}  // namespace pvd
