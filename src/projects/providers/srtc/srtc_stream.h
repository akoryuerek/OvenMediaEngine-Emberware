//==============================================================================
//
//  OvenMediaEngine - SRT Caller Stream
//
//  Created for Emberware
//  Copyright (c) 2024 Emberware. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/url.h>
#include <base/provider/pull_provider/stream.h>
#include <base/provider/pull_provider/application.h>

#include <modules/containers/mpegts/mpegts_depacketizer.h>

#include <srt/srt.h>

#define DEFAULT_SRT_LATENCY_MS          120
#define DEFAULT_SRT_CONNECTION_TIMEOUT  10000
#define DEFAULT_SRT_RECV_BUFFER_SIZE    (188 * 7 * 16)  // Multiple of MPEG-TS packet size

namespace pvd
{
	class SrtcProvider;

	class SrtcStream : public pvd::PullStream
	{
	public:
		static std::shared_ptr<SrtcStream> Create(
			const std::shared_ptr<pvd::PullApplication> &application,
			const uint32_t stream_id,
			const ov::String &stream_name,
			const std::vector<ov::String> &url_list,
			const std::shared_ptr<pvd::PullStreamProperties> &properties);

		SrtcStream(
			const std::shared_ptr<pvd::PullApplication> &application,
			const info::Stream &stream_info,
			const std::vector<ov::String> &url_list,
			const std::shared_ptr<pvd::PullStreamProperties> &properties);

		~SrtcStream() override;

		ProcessMediaEventTrigger GetProcessMediaEventTriggerMode() override
		{
			// SRT sockets don't work with regular epoll, use interval-based polling
			return ProcessMediaEventTrigger::TRIGGER_INTERVAL;
		}

		// PullStream Implementation
		int GetFileDescriptorForDetectingEvent() override;

		// If this stream belongs to the Pull provider,
		// this function is called periodically by the StreamMotor of application.
		// Media data has to be processed here.
		PullStream::ProcessMediaResult ProcessMediaPacket() override;

	private:
		std::shared_ptr<pvd::SrtcProvider> GetSrtcProvider();

		// Stream lifecycle
		bool StartStream(const std::shared_ptr<const ov::Url> &url) override;
		bool RestartStream(const std::shared_ptr<const ov::Url> &url) override;
		bool StopStream() override;

		// SRT Connection
		bool ConnectTo();
		bool Disconnect();
		void Release();

		// Receive and process MPEG-TS data
		bool ReceiveData();
		void OnDataReceived(const std::shared_ptr<const ov::Data> &data);

		// URL parsing
		bool ParseSrtUrl(const ov::String &url, ov::String &host, int &port, ov::String &streamid);

		// Current URL info
		std::vector<std::shared_ptr<const ov::Url>> _url_list;
		std::shared_ptr<const ov::Url> _curr_url;

		// SRT Socket
		SRTSOCKET _srt_socket = SRT_INVALID_SOCK;

		// SRT Options (parsed from URL or config)
		int _latency_ms = DEFAULT_SRT_LATENCY_MS;
		int _connection_timeout_ms = DEFAULT_SRT_CONNECTION_TIMEOUT;
		ov::String _streamid;
		ov::String _passphrase;

		// Receive buffer
		std::vector<uint8_t> _recv_buffer;

		// MPEG-TS Depacketizer
		std::shared_ptr<mpegts::MpegTsDepacketizer> _mpegts_depacketizer;

		// Statistics
		int64_t _origin_request_time_msec = 0;
		int64_t _origin_response_time_msec = 0;
		int64_t _bytes_received = 0;
		std::shared_ptr<mon::StreamMetrics> _stream_metrics;

		// Reconnect handling
		ov::StopWatch _reconnect_timer;
		int _reconnect_attempts = 0;
		static constexpr int MAX_RECONNECT_ATTEMPTS = 10;

		// Track publishing state
		bool _tracks_published = false;
	};
}  // namespace pvd
