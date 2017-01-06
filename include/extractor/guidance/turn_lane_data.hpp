#ifndef OSRM_EXTRACTOR_GUIDANCE_TURN_LANE_DATA_HPP_
#define OSRM_EXTRACTOR_GUIDANCE_TURN_LANE_DATA_HPP_

#include "extractor/guidance/turn_lane_types.hpp"
#include "util/attributes.hpp"
#include "util/typedefs.hpp"
#include <string>
#include <vector>

namespace osrm
{
namespace extractor
{
namespace guidance
{
namespace lanes
{

struct TurnLaneData
{
    TurnLaneType::Mask tag;
    LaneID from;
    LaneID to;

    // a temporary data entry that does not need to be assigned to an entry.
    // This is the case in situations that use partition and require the entry to perform the
    // one-to-one mapping.
    bool suppress_assignment;
    bool operator<(const TurnLaneData &other) const;
};
typedef std::vector<TurnLaneData> LaneDataVector;

// convertes a string given in the OSM format into a TurnLaneData vector
OSRM_ATTR_WARN_UNUSED
LaneDataVector laneDataFromDescription(TurnLaneDescription turn_lane_description);

// Locate A Tag in a lane data vector (if multiple tags are set, the first one found is returned)
LaneDataVector::const_iterator findTag(const TurnLaneType::Mask tag, const LaneDataVector &data);
LaneDataVector::iterator findTag(const TurnLaneType::Mask tag, LaneDataVector &data);

// Returns true if any of the queried tags is contained
bool hasTag(const TurnLaneType::Mask tag, const LaneDataVector &data);
} // namespace lane_data_generation

} // namespace guidance
} // namespace extractor
} // namespace osrm

#endif /* OSRM_EXTRACTOR_GUIDANCE_TURN_LANE_DATA_HPP_ */
