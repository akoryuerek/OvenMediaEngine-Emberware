//==============================================================================
//
//  OvenMediaEngine - SRT Caller Application
//
//  Created for Emberware
//  Copyright (c) 2024 Emberware. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/provider/pull_provider/application.h>
#include <base/provider/pull_provider/stream.h>

namespace pvd
{
	class SrtcApplication : public pvd::PullApplication
	{
	public:
		static std::shared_ptr<SrtcApplication> Create(
			const std::shared_ptr<pvd::PullProvider> &provider,
			const info::Application &app_info);

		explicit SrtcApplication(
			const std::shared_ptr<pvd::PullProvider> &provider,
			const info::Application &info);

		~SrtcApplication() override = default;

	protected:
		std::shared_ptr<pvd::PullStream> CreateStream(
			const uint32_t stream_id,
			const ov::String &stream_name,
			const std::vector<ov::String> &url_list,
			const std::shared_ptr<pvd::PullStreamProperties> &properties) override;
	};
}  // namespace pvd
