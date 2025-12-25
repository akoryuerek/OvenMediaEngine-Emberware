//==============================================================================
//
//  OvenMediaEngine - SRT Caller Provider
//
//  Created for Emberware
//  Copyright (c) 2024 Emberware. All rights reserved.
//
//==============================================================================

#include "srtc_provider.h"
#include "srtc_application.h"

#define OV_LOG_TAG "SrtcProvider"

namespace pvd
{
	std::shared_ptr<SrtcProvider> SrtcProvider::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router)
	{
		auto provider = std::make_shared<SrtcProvider>(server_config, router);
		if (!provider->Start())
		{
			return nullptr;
		}
		return provider;
	}

	SrtcProvider::SrtcProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router)
		: PullProvider(server_config, router)
	{
		// Read configuration
		// TODO: Add SRTC configuration to Server.xml schema
		// auto &srtc_config = server_config.GetBind().GetProviders().GetSrtc();
		// _worker_count = srtc_config.GetWorkerCount();
		// _reconnect_interval_ms = srtc_config.GetReconnectInterval();
		// _connection_timeout_ms = srtc_config.GetConnectionTimeout();

		logtd("Created SRTC (SRT Caller) Provider module.");
	}

	SrtcProvider::~SrtcProvider()
	{
		logti("Terminated SRTC Provider module.");
	}

	bool SrtcProvider::OnCreateHost(const info::Host &host_info)
	{
		return true;
	}

	bool SrtcProvider::OnDeleteHost(const info::Host &host_info)
	{
		return true;
	}

	std::shared_ptr<pvd::Application> SrtcProvider::OnCreateProviderApplication(const info::Application &app_info)
	{
		if (IsModuleAvailable() == false)
		{
			return nullptr;
		}

		auto application = SrtcApplication::Create(GetSharedPtrAs<pvd::PullProvider>(), app_info);
		if (application == nullptr)
		{
			logte("Could not create SRTC application for %s", app_info.GetName().CStr());
			return nullptr;
		}

		return application;
	}

	bool SrtcProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
	{
		return PullProvider::OnDeleteProviderApplication(application);
	}

}  // namespace pvd
