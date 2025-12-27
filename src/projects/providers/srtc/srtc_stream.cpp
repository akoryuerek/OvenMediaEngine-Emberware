//==============================================================================
//
//  OvenMediaEngine - SRT Caller Stream
//
//  Created for Emberware
//  Copyright (c) 2024 Emberware. All rights reserved.
//
//==============================================================================

#include "srtc_stream.h"
#include "srtc_provider.h"

#include <base/info/application.h>
#include <base/ovlibrary/byte_io.h>
#include <modules/srt/srt.h>

#define OV_LOG_TAG "SrtcStream"

namespace pvd
{
	std::shared_ptr<SrtcStream> SrtcStream::Create(
		const std::shared_ptr<pvd::PullApplication> &application,
		const uint32_t stream_id,
		const ov::String &stream_name,
		const std::vector<ov::String> &url_list,
		const std::shared_ptr<pvd::PullStreamProperties> &properties)
	{
		info::Stream stream_info(*std::static_pointer_cast<info::Application>(application), StreamSourceType::SrtPull);

		stream_info.SetId(stream_id);
		stream_info.SetName(stream_name);

		auto stream = std::make_shared<SrtcStream>(application, stream_info, url_list, properties);
		if (!stream->PullStream::Start())
		{
			stream.reset();
			return nullptr;
		}

		return stream;
	}

	SrtcStream::SrtcStream(
		const std::shared_ptr<pvd::PullApplication> &application,
		const info::Stream &stream_info,
		const std::vector<ov::String> &url_list,
		const std::shared_ptr<pvd::PullStreamProperties> &properties)
		: pvd::PullStream(application, stream_info, url_list, properties)
	{
		// Parse URLs
		for (const auto &url_str : url_list)
		{
			auto url = ov::Url::Parse(url_str);
			if (url != nullptr)
			{
				_url_list.push_back(url);
			}
		}

		// Initialize receive buffer
		_recv_buffer.resize(DEFAULT_SRT_RECV_BUFFER_SIZE);

		SetState(State::IDLE);

		logtd("Created SrtcStream for %s", stream_info.GetName().CStr());
	}

	SrtcStream::~SrtcStream()
	{
		PullStream::Stop();
		Release();
	}

	std::shared_ptr<pvd::SrtcProvider> SrtcStream::GetSrtcProvider()
	{
		return std::static_pointer_cast<SrtcProvider>(GetApplication()->GetParentProvider());
	}

	void SrtcStream::Release()
	{
		Disconnect();

		if (_mpegts_depacketizer != nullptr)
		{
			_mpegts_depacketizer.reset();
		}
	}

	bool SrtcStream::StartStream(const std::shared_ptr<const ov::Url> &url)
	{
		// Only start from IDLE, ERROR, STOPPED states
		if (!(GetState() == State::IDLE || GetState() == State::ERROR || GetState() == State::STOPPED))
		{
			return true;
		}

		_curr_url = url;
		_reconnect_attempts = 0;

		ov::StopWatch stop_watch;
		stop_watch.Start();

		if (ConnectTo() == false)
		{
			SetState(State::ERROR);
			return false;
		}

		_origin_request_time_msec = stop_watch.Elapsed();

		// Initialize MPEG-TS depacketizer
		_mpegts_depacketizer = std::make_shared<mpegts::MpegTsDepacketizer>();
		if (_mpegts_depacketizer == nullptr)
		{
			logte("Failed to create MPEG-TS depacketizer");
			Disconnect();
			return false;
		}

		_origin_response_time_msec = stop_watch.Elapsed();

		// Stream created completely
		_stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(PullStream::GetSharedPtr()));
		if (_stream_metrics != nullptr)
		{
			_stream_metrics->SetOriginConnectionTimeMSec(_origin_request_time_msec);
			_stream_metrics->SetOriginSubscribeTimeMSec(_origin_response_time_msec);
		}

		logti("SRT Caller connected to %s:%d (streamid: %s)",
			  _curr_url->Host().CStr(),
			  _curr_url->Port(),
			  _streamid.CStr());

		SetState(State::PLAYING);
		return true;
	}

	bool SrtcStream::RestartStream(const std::shared_ptr<const ov::Url> &url)
	{
		// Disconnect existing connection
		Disconnect();

		// Try to reconnect
		_reconnect_attempts++;
		if (_reconnect_attempts > MAX_RECONNECT_ATTEMPTS)
		{
			logte("Max reconnect attempts reached for %s", GetName().CStr());
			SetState(State::ERROR);
			return false;
		}

		logtw("Attempting to reconnect to %s (attempt %d/%d)",
			  url->ToUrlString().CStr(),
			  _reconnect_attempts,
			  MAX_RECONNECT_ATTEMPTS);

		return StartStream(url);
	}

	bool SrtcStream::StopStream()
	{
		Disconnect();
		SetState(State::STOPPED);
		return true;
	}

	bool SrtcStream::ParseSrtUrl(const ov::String &url_str, ov::String &host, int &port, ov::String &streamid)
	{
		// Parse SRT URL format: srt://host:port?streamid=xxx&latency=xxx&passphrase=xxx
		auto url = ov::Url::Parse(url_str);
		if (url == nullptr)
		{
			return false;
		}

		if (url->Scheme().UpperCaseString() != "SRT")
		{
			return false;
		}

		host = url->Host();
		port = url->Port() != 0 ? url->Port() : 9000;  // Default SRT port

		// Parse query parameters
		auto query_string = url->Query();
		if (!query_string.IsEmpty())
		{
			// Simple query parsing
			auto params = query_string.Split("&");
			for (const auto &param : params)
			{
				auto kv = param.Split("=");
				if (kv.size() == 2)
				{
					if (kv[0] == "streamid")
					{
						streamid = kv[1];
					}
					else if (kv[0] == "latency")
					{
						_latency_ms = ov::Converter::ToInt32(kv[1]);
					}
					else if (kv[0] == "passphrase")
					{
						_passphrase = kv[1];
					}
				}
			}
		}

		return true;
	}

	bool SrtcStream::ConnectTo()
	{
		if (GetState() == State::PLAYING || GetState() == State::TERMINATED)
		{
			return false;
		}

		if (_curr_url == nullptr)
		{
			logte("No URL specified for SRT connection");
			return false;
		}

		logtd("Connecting to SRT source: %s", _curr_url->ToUrlString().CStr());

		// Parse URL
		ov::String host;
		int port;
		if (!ParseSrtUrl(_curr_url->ToUrlString(), host, port, _streamid))
		{
			logte("Invalid SRT URL: %s", _curr_url->ToUrlString().CStr());
			return false;
		}

		// Create SRT socket
		_srt_socket = srt_create_socket();
		if (_srt_socket == SRT_INVALID_SOCK)
		{
			logte("Failed to create SRT socket: %s", srt_getlasterror_str());
			return false;
		}

		// Set socket options BEFORE connecting
		int yes = 1;
		int no = 0;

		// Sender mode = false (we are receiving)
		srt_setsockflag(_srt_socket, SRTO_SENDER, &no, sizeof(no));

		// Set latency
		srt_setsockflag(_srt_socket, SRTO_LATENCY, &_latency_ms, sizeof(_latency_ms));

		// Set connection timeout
		srt_setsockflag(_srt_socket, SRTO_CONNTIMEO, &_connection_timeout_ms, sizeof(_connection_timeout_ms));

		// Enable TSBPD (Timestamp-Based Packet Delivery)
		srt_setsockflag(_srt_socket, SRTO_TSBPDMODE, &yes, sizeof(yes));

		// Set streamid if specified
		if (!_streamid.IsEmpty())
		{
			srt_setsockflag(_srt_socket, SRTO_STREAMID, _streamid.CStr(), _streamid.GetLength());
		}

		// Set passphrase if specified
		if (!_passphrase.IsEmpty())
		{
			srt_setsockflag(_srt_socket, SRTO_PASSPHRASE, _passphrase.CStr(), _passphrase.GetLength());
		}

		// Resolve address
		struct sockaddr_in sa;
		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		sa.sin_port = htons(port);

		if (inet_pton(AF_INET, host.CStr(), &sa.sin_addr) != 1)
		{
			// Try to resolve hostname
			struct hostent *he = gethostbyname(host.CStr());
			if (he == nullptr)
			{
				logte("Failed to resolve hostname: %s", host.CStr());
				srt_close(_srt_socket);
				_srt_socket = SRT_INVALID_SOCK;
				return false;
			}
			memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
		}

		// Connect (blocking)
		logti("Connecting to SRT source %s:%d...", host.CStr(), port);

		int result = srt_connect(_srt_socket, (struct sockaddr *)&sa, sizeof(sa));
		if (result == SRT_ERROR)
		{
			logte("Failed to connect to SRT source %s:%d: %s",
				  host.CStr(), port, srt_getlasterror_str());
			srt_close(_srt_socket);
			_srt_socket = SRT_INVALID_SOCK;
			return false;
		}

		// Set to non-blocking mode after connection
		srt_setsockflag(_srt_socket, SRTO_RCVSYN, &no, sizeof(no));

		SetState(State::CONNECTED);
		logti("SRT connection established to %s:%d", host.CStr(), port);

		return true;
	}

	bool SrtcStream::Disconnect()
	{
		if (_srt_socket != SRT_INVALID_SOCK)
		{
			srt_close(_srt_socket);
			_srt_socket = SRT_INVALID_SOCK;
			logtd("SRT socket closed");
		}
		return true;
	}

	int SrtcStream::GetFileDescriptorForDetectingEvent()
	{
		// Return SRT socket for epoll
		return _srt_socket;
	}

	PullStream::ProcessMediaResult SrtcStream::ProcessMediaPacket()
	{
		if (_srt_socket == SRT_INVALID_SOCK)
		{
			return ProcessMediaResult::PROCESS_MEDIA_FAILURE;
		}

		if (!ReceiveData())
		{
			// Connection lost or error
			return ProcessMediaResult::PROCESS_MEDIA_FAILURE;
		}

		return ProcessMediaResult::PROCESS_MEDIA_SUCCESS;
	}

	bool SrtcStream::ReceiveData()
	{
		// Receive data from SRT socket
		int received = srt_recv(_srt_socket, reinterpret_cast<char*>(_recv_buffer.data()), _recv_buffer.size());

		if (received == SRT_ERROR)
		{
			int err = srt_getlasterror(nullptr);
			if (err == SRT_EASYNCRCV)
			{
				// No data available (non-blocking mode)
				return true;
			}

			logte("SRT receive error: %s", srt_getlasterror_str());
			return false;
		}

		if (received == 0)
		{
			// Connection closed
			logti("SRT connection closed by remote");
			return false;
		}

		_bytes_received += received;

		// Process received MPEG-TS data
		auto data = std::make_shared<ov::Data>(_recv_buffer.data(), received);
		OnDataReceived(data);

		return true;
	}

	void SrtcStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
	{
		if (_mpegts_depacketizer == nullptr)
		{
			return;
		}

		// Feed data to MPEG-TS depacketizer
		_mpegts_depacketizer->AddPacket(data);

		// First, check if track info is available and we haven't published yet
		if (_tracks_published == false && _mpegts_depacketizer->IsTrackInfoAvailable())
		{
			std::map<uint16_t, std::shared_ptr<MediaTrack>> track_list;
			if (_mpegts_depacketizer->GetTrackList(&track_list))
			{
				logti("SRTC: Discovered %zu tracks in MPEG-TS stream", track_list.size());
				for (const auto &pair : track_list)
				{
					auto track = pair.second;
					logtd("SRTC Track: PID=%d Type=%d Codec=%d",
						  track->GetId(),
						  static_cast<int>(track->GetMediaType()),
						  static_cast<int>(track->GetCodecId()));
					AddTrack(track);
				}
				_tracks_published = true;

				// Notify MediaRouter that the stream has been updated with new tracks
				// This triggers TranscoderStream::UpdateInternal() which creates output streams
				UpdateStream();
			}
		}

		// Process Elementary Streams
		while (_mpegts_depacketizer->IsESAvailable())
		{
			auto es = _mpegts_depacketizer->PopES();
			if (es == nullptr)
			{
				continue;
			}

			auto track = GetTrack(es->PID());
			if (track == nullptr)
			{
				logtd("SRTC: No track for PID %d, skipping", es->PID());
				continue;
			}

			int64_t pts = es->Pts();
			int64_t dts = es->Dts();

			// Adjust timestamps
			AdjustTimestampByBase(track->GetId(), pts, dts, 0x1FFFFFFFFLL);

			if (es->IsVideoStream())
			{
				auto bitstream = cmn::BitstreamFormat::Unknown;
				switch (track->GetCodecId())
				{
					case cmn::MediaCodecId::H264:
						bitstream = cmn::BitstreamFormat::H264_ANNEXB;
						break;
					case cmn::MediaCodecId::H265:
						bitstream = cmn::BitstreamFormat::H265_ANNEXB;
						break;
					default:
						break;
				}

				auto payload = std::make_shared<ov::Data>(es->Payload(), es->PayloadLength());
				auto media_packet = std::make_shared<MediaPacket>(
					GetMsid(),
					cmn::MediaType::Video,
					es->PID(),
					payload,
					pts,
					dts,
					-1LL,
					MediaPacketFlag::Unknown,
					bitstream,
					cmn::PacketType::NALU);

				SendFrame(media_packet);
			}
			else if (es->IsAudioStream())
			{
				auto payload = std::make_shared<ov::Data>(es->Payload(), es->PayloadLength());
				auto media_packet = std::make_shared<MediaPacket>(
					GetMsid(),
					cmn::MediaType::Audio,
					es->PID(),
					payload,
					pts,
					dts,
					-1LL,
					MediaPacketFlag::Unknown,
					cmn::BitstreamFormat::AAC_ADTS,
					cmn::PacketType::RAW);

				SendFrame(media_packet);
			}

			logtd("Frame - PID(%d) PTS(%lld) DTS(%lld) Size(%d)",
				  es->PID(), pts, dts, es->PayloadLength());
		}
	}

}  // namespace pvd
