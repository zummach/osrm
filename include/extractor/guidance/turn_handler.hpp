#ifndef OSRM_EXTRACTOR_GUIDANCE_TURN_HANDLER_HPP_
#define OSRM_EXTRACTOR_GUIDANCE_TURN_HANDLER_HPP_

#include "extractor/guidance/intersection.hpp"
#include "extractor/guidance/intersection_generator.hpp"
#include "extractor/guidance/intersection_handler.hpp"
#include "extractor/query_node.hpp"

#include "util/attributes.hpp"
#include "util/name_table.hpp"
#include "util/node_based_graph.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace osrm
{
namespace extractor
{
namespace guidance
{

// Intersection handlers deal with all issues related to intersections.
// They assign appropriate turn operations to the TurnOperations.
class TurnHandler : public IntersectionHandler
{
  public:
    TurnHandler(const util::NodeBasedDynamicGraph &node_based_graph,
                const std::vector<QueryNode> &node_info_list,
                const util::NameTable &name_table,
                const SuffixTable &street_name_suffix_table,
                const IntersectionGenerator &intersection_generator);

    ~TurnHandler() override final = default;

    // check whether the handler can actually handle the intersection
    bool canProcess(const NodeID nid,
                    const EdgeID via_eid,
                    const Intersection &intersection) const override final;

    // process the intersection
    Intersection operator()(const NodeID nid,
                            const EdgeID via_eid,
                            Intersection intersection) const override final;

  private:
    bool isObviousOfTwo(const EdgeID via_edge,
                        const ConnectedRoad &road,
                        const ConnectedRoad &other) const;
    // Dead end.
    OSRM_ATTR_WARN_UNUSED
    Intersection handleOneWayTurn(Intersection intersection) const;

    // Mode Changes, new names...
    OSRM_ATTR_WARN_UNUSED
    Intersection handleTwoWayTurn(const EdgeID via_edge, Intersection intersection) const;

    // Forks, T intersections and similar
    OSRM_ATTR_WARN_UNUSED
    Intersection handleThreeWayTurn(const EdgeID via_edge, Intersection intersection) const;

    // Handling of turns larger then degree three
    OSRM_ATTR_WARN_UNUSED
    Intersection handleComplexTurn(const EdgeID via_edge, Intersection intersection) const;

    void
    handleDistinctConflict(const EdgeID via_edge, ConnectedRoad &left, ConnectedRoad &right) const;

    // Classification
    std::pair<std::size_t, std::size_t> findFork(const EdgeID via_edge,
                                                 const Intersection &intersection) const;

    OSRM_ATTR_WARN_UNUSED
    Intersection assignLeftTurns(const EdgeID via_edge,
                                 Intersection intersection,
                                 const std::size_t starting_at) const;

    OSRM_ATTR_WARN_UNUSED
    Intersection assignRightTurns(const EdgeID via_edge,
                                  Intersection intersection,
                                  const std::size_t up_to) const;
};

} // namespace guidance
} // namespace extractor
} // namespace osrm

#endif /*OSRM_EXTRACTOR_GUIDANCE_TURN_HANDLER_HPP_*/
