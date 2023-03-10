#include <iostream>
#include <memory>
#include <thread>

#include <grpc++/grpc++.h>

#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/api.h>

#include <arrow/filesystem/api.h>
#include <arrow/flight/api.h>
#include <arrow/flight/client_middleware.h>

using arrow::Status;

std::string GetFlightMethodName(arrow::flight::FlightMethod method)
{
	if (method == arrow::flight::FlightMethod::Invalid)
	{
		return "Invalid";
	}
	else if (method == arrow::flight::FlightMethod::Handshake)
	{
		return "Handshake";
	}
	else if (method == arrow::flight::FlightMethod::ListFlights)
	{
		return "ListFlights";
	}
	else if (method == arrow::flight::FlightMethod::GetFlightInfo)
	{
		return "GetFlightInfo";
	}
	else if (method == arrow::flight::FlightMethod::GetSchema)
	{
		return "GetSchema";
	}
	else if (method == arrow::flight::FlightMethod::DoGet)
	{
		return "DoGet";
	}
	else if (method == arrow::flight::FlightMethod::DoPut)
	{
		return "DoPut";
	}
	else if (method == arrow::flight::FlightMethod::DoAction)
	{
		return "DoAction";
	}
	else if (method == arrow::flight::FlightMethod::ListActions)
	{
		return "ListActions";
	}
	else if (method == arrow::flight::FlightMethod::DoExchange)
	{
		return "DoExchange";
	}
	else
	{
		return "UNKNOWN";
	}
}

class HeaderInjectingMiddlewareFactory : public arrow::flight::ClientMiddlewareFactory
{
public:
	void StartCall(const arrow::flight::CallInfo &info,
				   std::unique_ptr<arrow::flight::ClientMiddleware> *middleware)
	{
		*middleware = std::unique_ptr<arrow::flight::ClientMiddleware>(
			new HeaderInjectingMiddleware(*this));
	}

private:
	class HeaderInjectingMiddleware : public arrow::flight::ClientMiddleware
	{
	public:
		explicit HeaderInjectingMiddleware(HeaderInjectingMiddlewareFactory &factory)
			: factory_(factory) {}

		void SendingHeaders(arrow::flight::AddCallHeaders *outgoing_headers) override
		{
			outgoing_headers->AddHeader("x-tracing-span-id", "my-span-id");
		}

		void ReceivedHeaders(const arrow::flight::CallHeaders
								 &incoming_headers) override {}

		void CallCompleted(const Status &status) override {}

	private:
		HeaderInjectingMiddlewareFactory &factory_;
	};
};

class DoPutFlightClient
{
public:
	DoPutFlightClient()
	{
	}

	arrow::Status ConnectClient(arrow::flight::Location location)
	{
		auto middleware =
			std::make_shared<HeaderInjectingMiddlewareFactory>();

		arrow::flight::FlightClientOptions options;
		// auto options =
		// arrow::flight::FlightClientOptions::Defaults();
		options.middleware.push_back(middleware);

		ARROW_ASSIGN_OR_RAISE(
			client,
			arrow::flight::FlightClient::Connect(location, options));
		std::cout << "Connected to " << location.ToString()
				  << std::endl;

		fs = std::make_shared<arrow::fs::LocalFileSystem>();
		root = std::make_shared<arrow::fs::SubTreeFileSystem>("./data/",
															  fs);

		return arrow::Status::OK();
	}

	arrow::Status ListFlights()
	{
				std::unique_ptr<arrow::flight::FlightListing> listing;
				ARROW_ASSIGN_OR_RAISE(listing, client->ListFlights());

				std::unique_ptr<arrow::flight::FlightInfo> flight_info;
				ARROW_ASSIGN_OR_RAISE(flight_info, listing->Next());

				if (!flight_info)
				{
				  return arrow::Status::OK();
				}

				std::cout << flight_info->descriptor().ToString() << std::endl;

				return arrow::Status::OK();
	}

private:
	std::unique_ptr<arrow::flight::FlightClient> client;
	std::shared_ptr<arrow::fs::LocalFileSystem> fs;
	std::shared_ptr<arrow::fs::SubTreeFileSystem> root;
};

void SleepFor(double seconds)
{
	std::this_thread::sleep_for(
		std::chrono::nanoseconds(static_cast<int64_t>(seconds * 1e9)));
}

Status RunMain(int argc, char **argv)
{

	arrow::flight::Location location;
	ARROW_ASSIGN_OR_RAISE(location,
						  arrow::flight::Location::ForGrpcTcp("localhost", 61234));

	auto myclient = new DoPutFlightClient();
	myclient->ConnectClient(location);
	myclient->ListFlights();

	return Status::OK();
}

int main(int argc, char **argv)
{
	signal(SIGTERM, [](int i)
		   { exit(i); });

	Status status = RunMain(argc, argv);

	if (!status.ok())
	{
		std::cerr << status << std::endl;
	}

	return 0;
}
