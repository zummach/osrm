#ifndef OSRM_EXTRACTOR_GUIDANCE_INTERSECTION_HANDLER_HPP_
#define OSRM_EXTRACTOR_GUIDANCE_INTERSECTION_HANDLER_HPP_

#include "extractor/guidance/intersection.hpp"
#include "extractor/guidance/intersection_generator.hpp"
#include "extractor/query_node.hpp"
#include "extractor/suffix_table.hpp"

#include "util/name_table.hpp"
#include "util/node_based_graph.hpp"

#include <cstddef>
#include <vector>

namespace osrm
{
namespace extractor
{
namespace guidance
{

// Intersection handlers deal with all issues related to intersections.
// They assign appropriate turn operations to the TurnOperations.
// This base class provides both the interface and implementations for
// common functions.
class IntersectionHandler
{
  public:
    IntersectionHandler(const util::NodeBasedDynamicGraph &node_based_graph,
                        const std::vector<QueryNode> &node_info_list,
                        const util::NameTable &name_table,
                        const SuffixTable &street_name_suffix_table,
                        const IntersectionGenerator &intersection_generator);

    virtual ~IntersectionHandler() = default;

    // check whether the handler can actually handle the intersection
    virtual bool
    canProcess(const NodeID nid, const EdgeID via_eid, const Intersection &intersection) const = 0;

    // process the intersection
    virtual Intersection
    operator()(const NodeID nid, const EdgeID via_eid, Intersection intersection) const = 0;

  protected:
    const util::NodeBasedDynamicGraph &node_based_graph;
    const std::vector<QueryNode> &node_info_list;
    const util::NameTable &name_table;
    const SuffixTable &street_name_suffix_table;
    const IntersectionGenerator &intersection_generator;

    // counts the number on allowed entry roads
    std::size_t countValid(const Intersection &intersection) const;

    // Decide on a basic turn types
    TurnType::Enum findBasicTurnType(const EdgeID via_edge, const ConnectedRoad &candidate) const;

    // Find the most obvious turn to follow. The function returns an index into the intersection
    // determining whether there is a road that can be seen as obvious turn in the presence of many
    // other possible turns. The function will consider road categories and other inputs like the
    // turn angles.
    std::size_t findObviousTurn(const EdgeID via_edge, const Intersection &intersection) const;

    // Obvious turns can still take multiple forms. This function looks at the turn onto a road
    // candidate when coming from a via_edge and determines the best instruction to emit.
    // `through_street` indicates if the street turned onto is a through sreet (think mergees and
    // similar)
    TurnInstruction getInstructionForObvious(const std::size_t number_of_candidates,
                                             const EdgeID via_edge,
                                             const bool through_street,
                                             const ConnectedRoad &candidate) const;

    // Treating potential forks
    void assignFork(const EdgeID via_edge, ConnectedRoad &left, ConnectedRoad &right) const;
    void assignFork(const EdgeID via_edge,
                    ConnectedRoad &left,
                    ConnectedRoad &center,
                    ConnectedRoad &right) const;

    // Trivial Turns use findBasicTurnType and getTurnDirection as only criteria
    void assignTrivialTurns(const EdgeID via_eid,
                            Intersection &intersection,
                            const std::size_t begin,
                            const std::size_t end) const;

    bool isThroughStreet(const std::size_t index, const Intersection &intersection) const;
};

} // namespace guidance
} // namespace extractor
} // namespace osrm

#endif /*OSRM_EXTRACTOR_GUIDANCE_INTERSECTION_HANDLER_HPP_*/
