#include "extractor/guidance/turn_discovery.hpp"
#include "extractor/guidance/constants.hpp"

namespace osrm
{
namespace extractor
{
namespace guidance
{
namespace lanes
{

bool findPreviousIntersection(const NodeID node_v,
                              const EdgeID via_edge,
                              const Intersection intersection,
                              const TurnAnalysis &turn_analysis,
                              const util::NodeBasedDynamicGraph &node_based_graph,
                              // output parameters
                              NodeID &result_node,
                              EdgeID &result_via_edge,
                              Intersection &result_intersection)
{
    /* We need to find the intersection that is located prior to via_edge.

     *
     * NODE_U  -> PREVIOUS_ID            -> NODE_V -> VIA_EDGE -> NODE_W:INTERSECTION
     * NODE_U? <- STRAIGHTMOST           <- NODE_V <- UTURN
     * NODE_U? -> UTURN == PREVIOUSE_ID? -> NODE_V -> VIA_EDGE
     *
     * To do so, we first get the intersection atNODE and find the straightmost turn from that
     * node. This will result in NODE_X. The uturn in the intersection at NODE_X should be
     * PREVIOUS_ID. To verify that find, we check the intersection using our PREVIOUS_ID candidate
     * to check the intersection at NODE for via_edge
     */
    const constexpr double COMBINE_DISTANCE_CUTOFF = 30;

    // we check if via-edge is too short. In this case the previous turn cannot influence the turn
    // at via_edge and the intersection at NODE_W
    if (node_based_graph.GetEdgeData(via_edge).distance > COMBINE_DISTANCE_CUTOFF)
        return false;

    // Node -> Via_Edge -> Intersection[0 == UTURN] -> reverse_of(via_edge) -> Intersection at node
    // (looking at the reverse direction).
    const auto node_w = node_based_graph.GetTarget(via_edge);
    const auto u_turn_at_node_w = intersection[0].turn.eid;
    const auto node_v_reverse_intersection =
        turn_analysis.getIntersection(node_w, u_turn_at_node_w);

    // Continue along the straightmost turn. If there is no straight turn, we cannot find a valid
    // previous intersection.
    const auto straightmost_at_v_in_reverse =
        findClosestTurn(node_v_reverse_intersection, STRAIGHT_ANGLE);

    // TODO evaluate if narrow turn is the right criterion here... Might be that other angles are
    // valid
    if (angularDeviation(straightmost_at_v_in_reverse->turn.angle, STRAIGHT_ANGLE) > GROUP_ANGLE)
        return false;

    const auto node_u = node_based_graph.GetTarget(straightmost_at_v_in_reverse->turn.eid);
    const auto node_u_reverse_intersection =
        turn_analysis.getIntersection(node_v, straightmost_at_v_in_reverse->turn.eid);

    // now check that the u-turn at the given intersection connects to via-edge
    // The u-turn at the now found intersection should, hopefully, represent the previous edge.
    result_node = node_u;
    result_via_edge = node_u_reverse_intersection[0].turn.eid;

    // if the edge is not traversable, we obviously don't have a previous intersection or couldn't
    // find it.
    if (node_based_graph.GetEdgeData(result_via_edge).reversed)
    {
        result_via_edge = SPECIAL_EDGEID;
        result_node = SPECIAL_NODEID;
        return false;
    }

    result_intersection = turn_analysis.getIntersection(node_u, result_via_edge);
    const auto check_via_edge =
        result_intersection.end() !=
        std::find_if(result_intersection.begin(),
                     result_intersection.end(),
                     [via_edge](const ConnectedRoad &road) { return road.turn.eid == via_edge; });

    if (!check_via_edge)
    {
        result_via_edge = SPECIAL_EDGEID;
        result_node = SPECIAL_NODEID;
        return false;
    }

    result_intersection =
        turn_analysis.assignTurnTypes(node_u, result_via_edge, std::move(result_intersection));

    return true;
}

} // namespace lanes
} // namespace guidance
} // namespace extractor
} // namespace osrm
