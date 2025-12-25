//==============================================================================
//
//  OvenMediaEngine - SRT Caller Provider
//
//  Created for Emberware
//  Copyright (c) 2024 Emberware. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/mediarouter/media_buffer.h>
#include <base/mediarouter/media_type.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/provider/pull_provider/application.h>
#include <base/provider/pull_provider/provider.h>
#include <orchestrator/orchestrator.h>

/*
 * SrtcProvider (SRT Caller/Pull Provider)
 *     : Actively connects to remote SRT sources in Listener mode
 *     : Creates SrtcApplication for each vhost/app
 *
 * SrtcApplication
 *     : Creates MediaRouterApplicationConnector, SrtcStream
 *
 * SrtcStream
 *     : Creates SRT socket in Caller mode
 *     : Connects to remote SRT source
 *     : Receives MPEG-TS data and demuxes it
 */

namespace pvd
{
	class SrtcProvider : public pvd::PullProvider
	{
	public:
		static std::shared_ptr<SrtcProvider> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router);

		explicit SrtcProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router);

		~SrtcProvider() override;

		ProviderStreamDirection GetProviderStreamDirection() const override
		{
			return ProviderStreamDirection::Pull;
		}

		ProviderType GetProviderType() const override
		{
			return ProviderType::SrtPull;  // New provider type
		}

		const char* GetProviderName() const override
		{
			return "SRTCProvider";
		}

		// Get worker count for SRT socket handling
		int GetWorkerCount() const { return _worker_count; }

	protected:
		bool OnCreateHost(const info::Host &host_info) override;
		bool OnDeleteHost(const info::Host &host_info) override;
		std::shared_ptr<pvd::Application> OnCreateProviderApplication(const info::Application &app_info) override;
		bool OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application) override;

	private:
		int _worker_count = 1;
		int _reconnect_interval_ms = 5000;  // Default 5 seconds
		int _connection_timeout_ms = 10000; // Default 10 seconds
	};
}  // namespace pvd
