#include "engine/api/json_factory.hpp"

#include "engine/hint.hpp"
#include "engine/polyline_compressor.hpp"
#include "util/integer_range.hpp"

#include "util/guidance/bearing_class.hpp"
#include "util/guidance/entry_class.hpp"
#include "util/guidance/toolkit.hpp"
#include "util/typedefs.hpp"

#include <boost/assert.hpp>
#include <boost/optional.hpp>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace TurnType = osrm::extractor::guidance::TurnType;
namespace DirectionModifier = osrm::extractor::guidance::DirectionModifier;
using TurnInstruction = osrm::extractor::guidance::TurnInstruction;

namespace osrm
{
namespace engine
{
namespace api
{
namespace json
{
namespace detail
{

const constexpr char *modifier_names[] = {"uturn",
                                          "sharp right",
                                          "right",
                                          "slight right",
                                          "straight",
                                          "slight left",
                                          "left",
                                          "sharp left"};

// translations of TurnTypes. Not all types are exposed to the outside world.
// invalid types should never be returned as part of the API
const constexpr char *turn_type_names[] = {
    "invalid",         "new name",   "continue", "turn",        "merge",
    "on ramp",         "off ramp",   "fork",     "end of road", "notification",
    "roundabout",      "roundabout", "rotary",   "rotary",      "roundabout turn",
    "roundabout turn", "use lane",   "invalid",  "invalid",     "invalid",
    "invalid",         "invalid",    "invalid",  "invalid",     "invalid",
    "invalid",         "invalid"};

const constexpr char *waypoint_type_names[] = {"invalid", "arrive", "depart"};

// Check whether to include a modifier in the result of the API
inline bool isValidModifier(const guidance::StepManeuver maneuver)
{
    return (maneuver.waypoint_type == guidance::WaypointType::None ||
            maneuver.instruction.direction_modifier != DirectionModifier::UTurn);
}

inline bool hasValidLanes(const guidance::Intersection &intersection)
{
    return intersection.lanes.lanes_in_turn > 0;
}

std::string instructionTypeToString(const TurnType::Enum type)
{
    static_assert(sizeof(turn_type_names) / sizeof(turn_type_names[0]) >= TurnType::MaxTurnType,
                  "Some turn types has not string representation.");
    return turn_type_names[static_cast<std::size_t>(type)];
}

util::json::Array lanesFromIntersection(const guidance::Intersection &intersection)
{
    BOOST_ASSERT(intersection.lanes.lanes_in_turn >= 1);
    util::json::Array result;
    LaneID lane_id = intersection.lane_description.size();

    for (const auto &lane_desc : intersection.lane_description)
    {
        --lane_id;
        util::json::Object lane;
        lane.values["indications"] = extractor::guidance::TurnLaneType::toJsonArray(lane_desc);
        if (lane_id >= intersection.lanes.first_lane_from_the_right &&
            lane_id <
                intersection.lanes.first_lane_from_the_right + intersection.lanes.lanes_in_turn)
            lane.values["valid"] = util::json::True();
        else
            lane.values["valid"] = util::json::False();

        result.values.push_back(lane);
    }

    return result;
}

std::string instructionModifierToString(const DirectionModifier::Enum modifier)
{
    static_assert(sizeof(modifier_names) / sizeof(modifier_names[0]) >=
                      DirectionModifier::MaxDirectionModifier,
                  "Some direction modifiers has not string representation.");
    return modifier_names[static_cast<std::size_t>(modifier)];
}

std::string waypointTypeToString(const guidance::WaypointType waypoint_type)
{
    static_assert(sizeof(waypoint_type_names) / sizeof(waypoint_type_names[0]) >=
                      static_cast<size_t>(guidance::WaypointType::MaxWaypointType),
                  "Some waypoint types has not string representation.");
    return waypoint_type_names[static_cast<std::size_t>(waypoint_type)];
}

util::json::Array coordinateToLonLat(const util::Coordinate coordinate)
{
    util::json::Array array;
    array.values.push_back(static_cast<double>(toFloating(coordinate.lon)));
    array.values.push_back(static_cast<double>(toFloating(coordinate.lat)));
    return array;
}

// FIXME this actually needs to be configurable from the profiles
std::string modeToString(const extractor::TravelMode mode)
{
    std::string token;
    switch (mode)
    {
    case TRAVEL_MODE_INACCESSIBLE:
        token = "inaccessible";
        break;
    case TRAVEL_MODE_DRIVING:
        token = "driving";
        break;
    case TRAVEL_MODE_CYCLING:
        token = "cycling";
        break;
    case TRAVEL_MODE_WALKING:
        token = "walking";
        break;
    case TRAVEL_MODE_FERRY:
        token = "ferry";
        break;
    case TRAVEL_MODE_TRAIN:
        token = "train";
        break;
    case TRAVEL_MODE_PUSHING_BIKE:
        token = "pushing bike";
        break;
    case TRAVEL_MODE_STEPS_UP:
        token = "steps up";
        break;
    case TRAVEL_MODE_STEPS_DOWN:
        token = "steps down";
        break;
    case TRAVEL_MODE_RIVER_UP:
        token = "river upstream";
        break;
    case TRAVEL_MODE_RIVER_DOWN:
        token = "river downstream";
        break;
    case TRAVEL_MODE_ROUTE:
        token = "route";
        break;
    default:
        token = "other";
        break;
    }
    return token;
}

} // namespace detail

util::json::Object makeStepManeuver(const guidance::StepManeuver &maneuver)
{
    util::json::Object step_maneuver;
    if (maneuver.waypoint_type == guidance::WaypointType::None)
        step_maneuver.values["type"] = detail::instructionTypeToString(maneuver.instruction.type);
    else
        step_maneuver.values["type"] = detail::waypointTypeToString(maneuver.waypoint_type);

    if (detail::isValidModifier(maneuver))
        step_maneuver.values["modifier"] =
            detail::instructionModifierToString(maneuver.instruction.direction_modifier);

    step_maneuver.values["location"] = detail::coordinateToLonLat(maneuver.location);
    step_maneuver.values["bearing_before"] = std::round(maneuver.bearing_before);
    step_maneuver.values["bearing_after"] = std::round(maneuver.bearing_after);
    if (maneuver.exit != 0)
        step_maneuver.values["exit"] = maneuver.exit;

    return step_maneuver;
}

util::json::Object makeIntersection(const guidance::Intersection &intersection)
{
    util::json::Object result;
    util::json::Array bearings;
    util::json::Array entry;

    bearings.values.reserve(intersection.bearings.size());
    std::copy(intersection.bearings.begin(),
              intersection.bearings.end(),
              std::back_inserter(bearings.values));

    entry.values.reserve(intersection.entry.size());
    std::transform(intersection.entry.begin(),
                   intersection.entry.end(),
                   std::back_inserter(entry.values),
                   [](const bool has_entry) -> util::json::Value {
                       if (has_entry)
                           return util::json::True();
                       else
                           return util::json::False();
                   });

    result.values["location"] = detail::coordinateToLonLat(intersection.location);
    result.values["bearings"] = bearings;
    result.values["entry"] = entry;
    if (intersection.in != guidance::Intersection::NO_INDEX)
        result.values["in"] = intersection.in;
    if (intersection.out != guidance::Intersection::NO_INDEX)
        result.values["out"] = intersection.out;

    if (detail::hasValidLanes(intersection))
        result.values["lanes"] = detail::lanesFromIntersection(intersection);

    return result;
}

util::json::Object makeRouteStep(guidance::RouteStep step, util::json::Value geometry)
{
    util::json::Object route_step;
    route_step.values["distance"] = std::round(step.distance * 10) / 10.;
    route_step.values["duration"] = std::round(step.duration * 10) / 10.;
    route_step.values["name"] = std::move(step.name);
    if (!step.ref.empty())
        route_step.values["ref"] = std::move(step.ref);
    if (!step.pronunciation.empty())
        route_step.values["pronunciation"] = std::move(step.pronunciation);
    if (!step.destinations.empty())
        route_step.values["destinations"] = std::move(step.destinations);
    if (!step.rotary_name.empty())
    {
        route_step.values["rotary_name"] = std::move(step.rotary_name);
        if (!step.rotary_pronunciation.empty())
        {
            route_step.values["rotary_pronunciation"] = std::move(step.rotary_pronunciation);
        }
    }

    route_step.values["mode"] = detail::modeToString(std::move(step.mode));
    route_step.values["maneuver"] = makeStepManeuver(std::move(step.maneuver));
    route_step.values["geometry"] = std::move(geometry);

    util::json::Array intersections;
    intersections.values.reserve(step.intersections.size());
    std::transform(step.intersections.begin(),
                   step.intersections.end(),
                   std::back_inserter(intersections.values),
                   makeIntersection);
    route_step.values["intersections"] = std::move(intersections);

    return route_step;
}

util::json::Object makeRoute(const guidance::Route &route,
                             util::json::Array legs,
                             boost::optional<util::json::Value> geometry)
{
    util::json::Object json_route;
    json_route.values["distance"] = std::round(route.distance * 10) / 10.;
    json_route.values["duration"] = std::round(route.duration * 10) / 10.;
    json_route.values["legs"] = std::move(legs);
    if (geometry)
    {
        json_route.values["geometry"] = *std::move(geometry);
    }
    return json_route;
}

util::json::Object makeWaypoint(const util::Coordinate location, std::string name, const Hint &hint)
{
    util::json::Object waypoint;
    waypoint.values["location"] = detail::coordinateToLonLat(location);
    waypoint.values["name"] = std::move(name);
    waypoint.values["hint"] = hint.ToBase64();
    return waypoint;
}

util::json::Object makeRouteLeg(guidance::RouteLeg leg, util::json::Array steps)
{
    util::json::Object route_leg;
    route_leg.values["distance"] = std::round(leg.distance * 10) / 10.;
    route_leg.values["duration"] = std::round(leg.duration * 10) / 10.;
    route_leg.values["summary"] = std::move(leg.summary);
    route_leg.values["steps"] = std::move(steps);
    return route_leg;
}

util::json::Object
makeRouteLeg(guidance::RouteLeg leg, util::json::Array steps, util::json::Object annotation)
{
    util::json::Object route_leg = makeRouteLeg(std::move(leg), std::move(steps));
    route_leg.values["annotation"] = std::move(annotation);
    return route_leg;
}

util::json::Array makeRouteLegs(std::vector<guidance::RouteLeg> legs,
                                std::vector<util::json::Value> step_geometries,
                                std::vector<util::json::Object> annotations)
{
    util::json::Array json_legs;
    auto step_geometry_iter = step_geometries.begin();
    for (const auto idx : util::irange<std::size_t>(0UL, legs.size()))
    {
        auto leg = std::move(legs[idx]);
        util::json::Array json_steps;
        json_steps.values.reserve(leg.steps.size());
        std::transform(std::make_move_iterator(leg.steps.begin()),
                       std::make_move_iterator(leg.steps.end()),
                       std::back_inserter(json_steps.values),
                       [&step_geometry_iter](guidance::RouteStep step) {
                           return makeRouteStep(std::move(step), std::move(*step_geometry_iter++));
                       });
        if (annotations.size() > 0)
        {
            json_legs.values.push_back(
                makeRouteLeg(std::move(leg), std::move(json_steps), annotations[idx]));
        }
        else
        {
            json_legs.values.push_back(makeRouteLeg(std::move(leg), std::move(json_steps)));
        }
    }
    return json_legs;
}
} // namespace json
} // namespace api
} // namespace engine
} // namespace osrm
