#ifndef MOCK_DATAFACADE_HPP
#define MOCK_DATAFACADE_HPP

// implements all data storage when shared memory _IS_ used

#include "contractor/query_edge.hpp"
#include "extractor/guidance/turn_instruction.hpp"
#include "extractor/guidance/turn_lane_types.hpp"
#include "engine/datafacade/datafacade_base.hpp"
#include "util/guidance/bearing_class.hpp"
#include "util/guidance/entry_class.hpp"
#include "util/typedefs.hpp"

namespace osrm
{
namespace test
{

class MockDataFacade final : public engine::datafacade::BaseDataFacade
{
  private:
    EdgeData foo;

  public:
    unsigned GetNumberOfNodes() const override { return 0; }
    unsigned GetNumberOfEdges() const override { return 0; }
    unsigned GetOutDegree(const NodeID /* n */) const override { return 0; }
    NodeID GetTarget(const EdgeID /* e */) const override { return SPECIAL_NODEID; }
    const EdgeData &GetEdgeData(const EdgeID /* e */) const override { return foo; }
    EdgeID BeginEdges(const NodeID /* n */) const override { return SPECIAL_EDGEID; }
    EdgeID EndEdges(const NodeID /* n */) const override { return SPECIAL_EDGEID; }
    osrm::engine::datafacade::EdgeRange GetAdjacentEdgeRange(const NodeID /* node */) const override
    {
        return util::irange(static_cast<EdgeID>(0), static_cast<EdgeID>(0));
    }
    EdgeID FindEdge(const NodeID /* from */, const NodeID /* to */) const override
    {
        return SPECIAL_EDGEID;
    }
    EdgeID FindEdgeInEitherDirection(const NodeID /* from */, const NodeID /* to */) const override
    {
        return SPECIAL_EDGEID;
    }
    EdgeID FindEdgeIndicateIfReverse(const NodeID /* from */,
                                     const NodeID /* to */,
                                     bool & /* result */) const override
    {
        return SPECIAL_EDGEID;
    }
    util::Coordinate GetCoordinateOfNode(const unsigned /* id */) const override
    {
        return {util::FixedLongitude{0}, util::FixedLatitude{0}};
    }
    OSMNodeID GetOSMNodeIDOfNode(const unsigned /* id */) const override { return OSMNodeID{0}; }
    bool EdgeIsCompressed(const unsigned /* id */) const { return false; }
    unsigned GetGeometryIndexForEdgeID(const unsigned /* id */) const override
    {
        return SPECIAL_NODEID;
    }
    void GetUncompressedGeometry(const EdgeID /* id */,
                                 std::vector<NodeID> & /* result_nodes */) const override
    {
    }
    void GetUncompressedWeights(const EdgeID /* id */,
                                std::vector<EdgeWeight> &result_weights) const override
    {
        result_weights.resize(1);
        result_weights[0] = 1;
    }
    void GetUncompressedDatasources(const EdgeID /*id*/,
                                    std::vector<uint8_t> & /*data_sources*/) const override
    {
    }
    std::string GetDatasourceName(const uint8_t /*datasource_name_id*/) const override
    {
        return "";
    }
    extractor::guidance::TurnInstruction
    GetTurnInstructionForEdgeID(const unsigned /* id */) const override
    {
        return extractor::guidance::TurnInstruction::NO_TURN();
    }
    extractor::TravelMode GetTravelModeForEdgeID(const unsigned /* id */) const override
    {
        return TRAVEL_MODE_INACCESSIBLE;
    }
    std::vector<RTreeLeaf> GetEdgesInBox(const util::Coordinate /* south_west */,
                                         const util::Coordinate /*north_east */) const override
    {
        return {};
    }

    std::vector<engine::PhantomNodeWithDistance>
    NearestPhantomNodesInRange(const util::Coordinate /*input_coordinate*/,
                               const float /*max_distance*/,
                               const int /*bearing*/,
                               const int /*bearing_range*/) const override
    {
        return {};
    }

    std::vector<engine::PhantomNodeWithDistance>
    NearestPhantomNodesInRange(const util::Coordinate /*input_coordinate*/,
                               const float /*max_distance*/) const override
    {
        return {};
    }

    std::vector<engine::PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate /*input_coordinate*/,
                        const unsigned /*max_results*/,
                        const double /*max_distance*/,
                        const int /*bearing*/,
                        const int /*bearing_range*/) const override
    {
        return {};
    }

    std::vector<engine::PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate /*input_coordinate*/,
                        const unsigned /*max_results*/,
                        const int /*bearing*/,
                        const int /*bearing_range*/) const override
    {
        return {};
    }

    std::vector<engine::PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate /*input_coordinate*/,
                        const unsigned /*max_results*/) const override
    {
        return {};
    }

    std::vector<engine::PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate /*input_coordinate*/,
                        const unsigned /*max_results*/,
                        const double /*max_distance*/) const override
    {
        return {};
    }

    std::pair<engine::PhantomNode, engine::PhantomNode>
    NearestPhantomNodeWithAlternativeFromBigComponent(
        const util::Coordinate /*input_coordinate*/) const override
    {
        return {};
    }

    std::pair<engine::PhantomNode, engine::PhantomNode>
    NearestPhantomNodeWithAlternativeFromBigComponent(const util::Coordinate /*input_coordinate*/,
                                                      const double /*max_distance*/) const override
    {
        return {};
    }

    std::pair<engine::PhantomNode, engine::PhantomNode>
    NearestPhantomNodeWithAlternativeFromBigComponent(const util::Coordinate /*input_coordinate*/,
                                                      const double /*max_distance*/,
                                                      const int /*bearing*/,
                                                      const int /*bearing_range*/) const override
    {
        return {};
    }

    std::pair<engine::PhantomNode, engine::PhantomNode>
    NearestPhantomNodeWithAlternativeFromBigComponent(const util::Coordinate /*input_coordinate*/,
                                                      const int /*bearing*/,
                                                      const int /*bearing_range*/) const override
    {
        return {};
    };

    unsigned GetCheckSum() const override { return 0; }
    bool IsCoreNode(const NodeID /* id */) const override { return false; }
    unsigned GetNameIndexFromEdgeID(const unsigned /* id */) const override { return 0; }
    std::string GetNameForID(const unsigned /* name_id */) const override { return ""; }
    std::string GetRefForID(const unsigned /* name_id */) const override { return ""; }
    std::string GetPronunciationForID(const unsigned /* name_id */) const override { return ""; }
    std::string GetDestinationsForID(const unsigned /* name_id */) const override { return ""; }
    std::size_t GetCoreSize() const override { return 0; }
    std::string GetTimestamp() const override { return ""; }
    bool GetContinueStraightDefault() const override { return true; }
    BearingClassID GetBearingClassID(const NodeID /*id*/) const override { return 0; };
    EntryClassID GetEntryClassID(const EdgeID /*id*/) const override { return 0; }

    bool hasLaneData(const EdgeID /*id*/) const override final { return true; };
    util::guidance::LaneTupelIdPair GetLaneData(const EdgeID /*id*/) const override final
    {
        return {{0, 0}, 0};
    }
    extractor::guidance::TurnLaneDescription
    GetTurnDescription(const LaneDescriptionID /*lane_description_id*/) const override final
    {
        return {};
    }

    util::guidance::BearingClass
    GetBearingClass(const BearingClassID /*bearing_class_id*/) const override
    {
        util::guidance::BearingClass result;
        result.add(0);
        result.add(90);
        result.add(180);
        result.add(270);
        return result;
    }

    util::guidance::EntryClass GetEntryClass(const EntryClassID /*entry_class_id*/) const override
    {
        util::guidance::EntryClass result;
        result.activate(1);
        result.activate(2);
        result.activate(3);
        return result;
    }
};
} // ns test
} // ns osrm

#endif // MOCK_DATAFACADE_HPP
