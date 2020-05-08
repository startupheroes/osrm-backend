#include "server/service_handler.hpp"

#include "server/service/match_service.hpp"
#include "server/service/nearest_service.hpp"
#include "server/service/route_service.hpp"
#include "server/service/table_service.hpp"
#include "server/service/tile_service.hpp"
#include "server/service/trip_service.hpp"

#include "server/api/parsed_url.hpp"
#include "util/json_util.hpp"

#include <memory>

#define BAD_SERVICE_NAME "invalid"

namespace osrm
{
namespace server
{
ServiceHandler::ServiceHandler(osrm::EngineConfig &config)
    : routing_machine(config), errors(0), load(0)
{
    service_map["route"] = std::make_unique<service::RouteService>(routing_machine);
    service_map["table"] = std::make_unique<service::TableService>(routing_machine);
    service_map["nearest"] = std::make_unique<service::NearestService>(routing_machine);
    service_map["trip"] = std::make_unique<service::TripService>(routing_machine);
    service_map["match"] = std::make_unique<service::MatchService>(routing_machine);
    service_map["tile"] = std::make_unique<service::TileService>(routing_machine);
}

engine::Status ServiceHandler::RunQuery(api::ParsedURL parsed_url,
                                        osrm::engine::api::ResultT &result)
{
    load++;
    const auto &service_iter = service_map.find(parsed_url.service);
    if (service_iter == service_map.end())
    {
        errors++;
        result = util::json::Object();
        auto &json_result = result.get<util::json::Object>();
        json_result.values["code"] = "InvalidService";
        json_result.values["message"] = "Service " + parsed_url.service + " not found!";
        load--;
        return engine::Status::Error;
    }
    auto &service = service_iter->second;

    if (service->GetVersion() != parsed_url.version)
    {
        errors++;
        result = util::json::Object();
        auto &json_result = result.get<util::json::Object>();
        json_result.values["code"] = "InvalidVersion";
        json_result.values["message"] = "Service " + parsed_url.service + " not found!";
        load--;
        return engine::Status::Error;
    }

    const auto status = service->RunQuery(parsed_url.prefix_length, parsed_url.query, result);
    load--;
    return status;
}

const engine::EngineInfo &ServiceHandler::GetEngineInfo() const { return routing_machine.Info(); }

std::uint32_t ServiceHandler::GetLoad() const { return load; }

const HandlersCounter ServiceHandler::GetUsage() const
{
    HandlersCounter handlers_counter;
    for (const auto &service : service_map)
    {
        handlers_counter[service.first] = service.second->GetUsage();
    }
    handlers_counter[BAD_SERVICE_NAME] = errors;
    return handlers_counter;
}
} // namespace server
} // namespace osrm
