#ifndef ROUTE_STEP_HPP
#define ROUTE_STEP_HPP

#include "extractor/travel_mode.hpp"
#include "engine/guidance/step_maneuver.hpp"
#include "util/coordinate.hpp"
#include "util/guidance/bearing_class.hpp"
#include "util/guidance/entry_class.hpp"

#include "extractor/guidance/turn_lane_types.hpp"
#include "util/guidance/turn_lanes.hpp"

#include <cstddef>

#include <string>
#include <vector>

namespace osrm
{
namespace engine
{
namespace guidance
{
// Given the following turn from a,b to b,c over b:
//  a --> b --> c
// this struct saves the information of the segment b,c.
// Notable exceptions are Departure and Arrival steps.
// Departure: s --> a --> b. Represents the segment s,a with location being s.
// Arrive: a --> b --> t. The segment (b,t) is already covered by the previous segment.

// A representation of intermediate intersections
struct Intersection
{
    static const constexpr std::size_t NO_INDEX = std::numeric_limits<std::size_t>::max();
    util::Coordinate location;
    std::vector<short> bearings;
    std::vector<bool> entry;
    std::size_t in;
    std::size_t out;

    // turn lane information
    util::guidance::LaneTupel lanes;
    extractor::guidance::TurnLaneDescription lane_description;
};

inline Intersection getInvalidIntersection()
{
    return {util::Coordinate{util::FloatLongitude{0.0}, util::FloatLatitude{0.0}},
            {},
            {},
            Intersection::NO_INDEX,
            Intersection::NO_INDEX,
            util::guidance::LaneTupel(),
            {}};
}

struct RouteStep
{
    unsigned name_id;
    std::string name;
    std::string ref;
    std::string pronunciation;
    std::string destinations;
    std::string rotary_name;
    std::string rotary_pronunciation;
    double duration;
    double distance;
    extractor::TravelMode mode;
    StepManeuver maneuver;
    // indices into the locations array stored the LegGeometry
    std::size_t geometry_begin;
    std::size_t geometry_end;
    std::vector<Intersection> intersections;
};

inline RouteStep getInvalidRouteStep()
{
    return {0,
            "",
            "",
            "",
            "",
            "",
            "",
            0,
            0,
            TRAVEL_MODE_INACCESSIBLE,
            getInvalidStepManeuver(),
            0,
            0,
            {getInvalidIntersection()}};
}
}
}
}

#endif
