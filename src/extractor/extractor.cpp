#include "extractor/extractor.hpp"

#include "extractor/edge_based_edge.hpp"
#include "extractor/extraction_containers.hpp"
#include "extractor/extraction_node.hpp"
#include "extractor/extraction_way.hpp"
#include "extractor/extractor_callbacks.hpp"
#include "extractor/restriction_parser.hpp"
#include "extractor/scripting_environment.hpp"

#include "extractor/raster_source.hpp"
#include "util/graph_loader.hpp"
#include "util/io.hpp"
#include "util/make_unique.hpp"
#include "util/name_table.hpp"
#include "util/range_table.hpp"
#include "util/simple_logger.hpp"
#include "util/timing_util.hpp"

#include "extractor/compressed_edge_container.hpp"
#include "extractor/restriction_map.hpp"
#include "util/static_graph.hpp"
#include "util/static_rtree.hpp"

// Keep debug include to make sure the debug header is in sync with types.
#include "util/debug.hpp"

#include "extractor/tarjan_scc.hpp"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/optional/optional.hpp>

#include <osmium/io/any_input.hpp>

#include <tbb/concurrent_vector.h>
#include <tbb/task_scheduler_init.h>

#include <cstdlib>

#include <algorithm>
#include <atomic>
#include <bitset>
#include <chrono>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric> //partial_sum
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace osrm
{
namespace extractor
{

namespace
{
std::tuple<std::vector<std::uint32_t>, std::vector<guidance::TurnLaneType::Mask>>
transformTurnLaneMapIntoArrays(const guidance::LaneDescriptionMap &turn_lane_map)
{
    // could use some additional capacity? To avoid a copy during processing, though small data so
    // probably not that important.
    //
    // From the map, we construct an adjacency array that allows access from all IDs to the list of
    // associated Turn Lane Masks.
    //
    // turn lane offsets points into the locations of the turn_lane_masks array. We use a standard
    // adjacency array like structure to store the turn lane masks.
    std::vector<std::uint32_t> turn_lane_offsets(turn_lane_map.size() + 2); // empty ID + sentinel
    for (auto entry = turn_lane_map.begin(); entry != turn_lane_map.end(); ++entry)
        turn_lane_offsets[entry->second + 1] = entry->first.size();

    // inplace prefix sum
    std::partial_sum(turn_lane_offsets.begin(), turn_lane_offsets.end(), turn_lane_offsets.begin());

    // allocate the current masks
    std::vector<guidance::TurnLaneType::Mask> turn_lane_masks(turn_lane_offsets.back());
    for (auto entry = turn_lane_map.begin(); entry != turn_lane_map.end(); ++entry)
        std::copy(entry->first.begin(),
                  entry->first.end(),
                  turn_lane_masks.begin() + turn_lane_offsets[entry->second]);
    return std::make_tuple(std::move(turn_lane_offsets), std::move(turn_lane_masks));
}
} // namespace

/**
 * TODO: Refactor this function into smaller functions for better readability.
 *
 * This function is the entry point for the whole extraction process. The goal of the extraction
 * step is to filter and convert the OSM geometry to something more fitting for routing.
 * That includes:
 *  - extracting turn restrictions
 *  - splitting ways into (directional!) edge segments
 *  - checking if nodes are barriers or traffic signal
 *  - discarding all tag information: All relevant type information for nodes/ways
 *    is extracted at this point.
 *
 * The result of this process are the following files:
 *  .names : Names of all streets, stored as long consecutive string with prefix sum based index
 *  .osrm  : Nodes and edges in a intermediate format that easy to digest for osrm-contract
 *  .restrictions : Turn restrictions that are used by osrm-contract to construct the edge-expanded
 * graph
 *
 */
int Extractor::run(ScriptingEnvironment &scripting_environment)
{
    {
        util::LogPolicy::GetInstance().Unmute();
        TIMER_START(extracting);

        const unsigned recommended_num_threads = tbb::task_scheduler_init::default_num_threads();
        const auto number_of_threads =
            std::min(recommended_num_threads, config.requested_num_threads);
        tbb::task_scheduler_init init(number_of_threads);

        util::SimpleLogger().Write() << "Input file: " << config.input_path.filename().string();
        if (!config.profile_path.empty())
        {
            util::SimpleLogger().Write() << "Profile: " << config.profile_path.filename().string();
        }
        util::SimpleLogger().Write() << "Threads: " << number_of_threads;

        ExtractionContainers extraction_containers;
        auto extractor_callbacks = util::make_unique<ExtractorCallbacks>(extraction_containers);

        const osmium::io::File input_file(config.input_path.string());
        osmium::io::Reader reader(input_file);
        const osmium::io::Header header = reader.header();

        unsigned number_of_nodes = 0;
        unsigned number_of_ways = 0;
        unsigned number_of_relations = 0;

        util::SimpleLogger().Write() << "Parsing in progress..";
        TIMER_START(parsing);

        // setup raster sources
        scripting_environment.SetupSources();

        std::string generator = header.get("generator");
        if (generator.empty())
        {
            generator = "unknown tool";
        }
        util::SimpleLogger().Write() << "input file generated by " << generator;

        // write .timestamp data file
        std::string timestamp = header.get("osmosis_replication_timestamp");
        if (timestamp.empty())
        {
            timestamp = "n/a";
        }
        util::SimpleLogger().Write() << "timestamp: " << timestamp;

        boost::filesystem::ofstream timestamp_out(config.timestamp_file_name);
        timestamp_out.write(timestamp.c_str(), timestamp.length());

        // initialize vectors holding parsed objects
        tbb::concurrent_vector<std::pair<std::size_t, ExtractionNode>> resulting_nodes;
        tbb::concurrent_vector<std::pair<std::size_t, ExtractionWay>> resulting_ways;
        tbb::concurrent_vector<boost::optional<InputRestrictionContainer>> resulting_restrictions;

        // setup restriction parser
        const RestrictionParser restriction_parser(scripting_environment);

        while (const osmium::memory::Buffer buffer = reader.read())
        {
            // create a vector of iterators into the buffer
            std::vector<osmium::memory::Buffer::const_iterator> osm_elements;
            for (auto iter = std::begin(buffer), end = std::end(buffer); iter != end; ++iter)
            {
                osm_elements.push_back(iter);
            }

            // clear resulting vectors
            resulting_nodes.clear();
            resulting_ways.clear();
            resulting_restrictions.clear();

            scripting_environment.ProcessElements(osm_elements,
                                                  restriction_parser,
                                                  resulting_nodes,
                                                  resulting_ways,
                                                  resulting_restrictions);

            number_of_nodes += resulting_nodes.size();
            // put parsed objects thru extractor callbacks
            for (const auto &result : resulting_nodes)
            {
                extractor_callbacks->ProcessNode(
                    static_cast<const osmium::Node &>(*(osm_elements[result.first])),
                    result.second);
            }
            number_of_ways += resulting_ways.size();
            for (const auto &result : resulting_ways)
            {
                extractor_callbacks->ProcessWay(
                    static_cast<const osmium::Way &>(*(osm_elements[result.first])), result.second);
            }
            number_of_relations += resulting_restrictions.size();
            for (const auto &result : resulting_restrictions)
            {
                extractor_callbacks->ProcessRestriction(result);
            }
        }
        TIMER_STOP(parsing);
        util::SimpleLogger().Write() << "Parsing finished after " << TIMER_SEC(parsing)
                                     << " seconds";

        util::SimpleLogger().Write() << "Raw input contains " << number_of_nodes << " nodes, "
                                     << number_of_ways << " ways, and " << number_of_relations
                                     << " relations";

        // take control over the turn lane map
        turn_lane_map = extractor_callbacks->moveOutLaneDescriptionMap();

        extractor_callbacks.reset();

        if (extraction_containers.all_edges_list.empty())
        {
            util::SimpleLogger().Write(logWARNING) << "The input data is empty, exiting.";
            return 1;
        }

        extraction_containers.PrepareData(scripting_environment,
                                          config.output_file_name,
                                          config.restriction_file_name,
                                          config.names_file_name);

        WriteProfileProperties(config.profile_properties_output_path,
                               scripting_environment.GetProfileProperties());

        TIMER_STOP(extracting);
        util::SimpleLogger().Write() << "extraction finished after " << TIMER_SEC(extracting)
                                     << "s";
    }

    {
        // Transform the node-based graph that OSM is based on into an edge-based graph
        // that is better for routing.  Every edge becomes a node, and every valid
        // movement (e.g. turn from A->B, and B->A) becomes an edge
        //
        util::SimpleLogger().Write() << "Generating edge-expanded graph representation";

        TIMER_START(expansion);

        std::vector<EdgeBasedNode> edge_based_node_list;
        util::DeallocatingVector<EdgeBasedEdge> edge_based_edge_list;
        std::vector<bool> node_is_startpoint;
        std::vector<EdgeWeight> edge_based_node_weights;
        std::vector<QueryNode> internal_to_external_node_map;
        auto graph_size = BuildEdgeExpandedGraph(scripting_environment,
                                                 internal_to_external_node_map,
                                                 edge_based_node_list,
                                                 node_is_startpoint,
                                                 edge_based_node_weights,
                                                 edge_based_edge_list,
                                                 config.intersection_class_data_output_path);

        auto number_of_node_based_nodes = graph_size.first;
        auto max_edge_id = graph_size.second;

        TIMER_STOP(expansion);

        util::SimpleLogger().Write() << "Saving edge-based node weights to file.";
        TIMER_START(timer_write_node_weights);
        util::serializeVector(config.edge_based_node_weights_output_path, edge_based_node_weights);
        TIMER_STOP(timer_write_node_weights);
        util::SimpleLogger().Write() << "Done writing. (" << TIMER_SEC(timer_write_node_weights)
                                     << ")";

        util::SimpleLogger().Write() << "Computing strictly connected components ...";
        FindComponents(max_edge_id, edge_based_edge_list, edge_based_node_list);

        util::SimpleLogger().Write() << "Building r-tree ...";
        TIMER_START(rtree);
        BuildRTree(std::move(edge_based_node_list),
                   std::move(node_is_startpoint),
                   internal_to_external_node_map);

        TIMER_STOP(rtree);

        util::SimpleLogger().Write() << "Writing node map ...";
        WriteNodeMapping(internal_to_external_node_map);

        WriteEdgeBasedGraph(config.edge_graph_output_path, max_edge_id, edge_based_edge_list);

        util::SimpleLogger().Write()
            << "Expansion  : " << (number_of_node_based_nodes / TIMER_SEC(expansion))
            << " nodes/sec and " << ((max_edge_id + 1) / TIMER_SEC(expansion)) << " edges/sec";
        util::SimpleLogger().Write() << "To prepare the data for routing, run: "
                                     << "./osrm-contract " << config.output_file_name << std::endl;
    }

    return 0;
}

void Extractor::WriteProfileProperties(const std::string &output_path,
                                       const ProfileProperties &properties) const
{
    boost::filesystem::ofstream out_stream(output_path);
    if (!out_stream)
    {
        throw util::exception("Could not open " + output_path + " for writing.");
    }

    out_stream.write(reinterpret_cast<const char *>(&properties), sizeof(properties));
}

void Extractor::FindComponents(unsigned max_edge_id,
                               const util::DeallocatingVector<EdgeBasedEdge> &input_edge_list,
                               std::vector<EdgeBasedNode> &input_nodes) const
{
    struct UncontractedEdgeData
    {
    };
    struct InputEdge
    {
        unsigned source;
        unsigned target;
        UncontractedEdgeData data;

        bool operator<(const InputEdge &rhs) const
        {
            return source < rhs.source || (source == rhs.source && target < rhs.target);
        }

        bool operator==(const InputEdge &rhs) const
        {
            return source == rhs.source && target == rhs.target;
        }
    };
    using UncontractedGraph = util::StaticGraph<UncontractedEdgeData>;
    std::vector<InputEdge> edges;
    edges.reserve(input_edge_list.size() * 2);

    for (const auto &edge : input_edge_list)
    {
        BOOST_ASSERT_MSG(static_cast<unsigned int>(std::max(edge.weight, 1)) > 0,
                         "edge distance < 1");
        BOOST_ASSERT(edge.source <= max_edge_id);
        BOOST_ASSERT(edge.target <= max_edge_id);
        if (edge.forward)
        {
            edges.push_back({edge.source, edge.target, {}});
        }

        if (edge.backward)
        {
            edges.push_back({edge.target, edge.source, {}});
        }
    }

    // connect forward and backward nodes of each edge
    for (const auto &node : input_nodes)
    {
        if (node.reverse_segment_id.enabled)
        {
            BOOST_ASSERT(node.forward_segment_id.id <= max_edge_id);
            BOOST_ASSERT(node.reverse_segment_id.id <= max_edge_id);
            edges.push_back({node.forward_segment_id.id, node.reverse_segment_id.id, {}});
            edges.push_back({node.reverse_segment_id.id, node.forward_segment_id.id, {}});
        }
    }

    tbb::parallel_sort(edges.begin(), edges.end());
    auto new_end = std::unique(edges.begin(), edges.end());
    edges.resize(new_end - edges.begin());

    auto uncontractor_graph = std::make_shared<UncontractedGraph>(max_edge_id + 1, edges);

    TarjanSCC<UncontractedGraph> component_search(
        std::const_pointer_cast<const UncontractedGraph>(uncontractor_graph));
    component_search.Run();

    for (auto &node : input_nodes)
    {
        auto forward_component = component_search.GetComponentID(node.forward_segment_id.id);
        BOOST_ASSERT(!node.reverse_segment_id.enabled ||
                     forward_component ==
                         component_search.GetComponentID(node.reverse_segment_id.id));

        const unsigned component_size = component_search.GetComponentSize(forward_component);
        node.component.is_tiny = component_size < config.small_component_size;
        node.component.id = 1 + forward_component;
    }
}

/**
  \brief Build load restrictions from .restriction file
  */
std::shared_ptr<RestrictionMap> Extractor::LoadRestrictionMap()
{
    boost::filesystem::ifstream input_stream(config.restriction_file_name,
                                             std::ios::in | std::ios::binary);

    std::vector<TurnRestriction> restriction_list;
    util::loadRestrictionsFromFile(input_stream, restriction_list);

    util::SimpleLogger().Write() << " - " << restriction_list.size() << " restrictions.";

    return std::make_shared<RestrictionMap>(restriction_list);
}

/**
  \brief Load node based graph from .osrm file
  */
std::shared_ptr<util::NodeBasedDynamicGraph>
Extractor::LoadNodeBasedGraph(std::unordered_set<NodeID> &barrier_nodes,
                              std::unordered_set<NodeID> &traffic_lights,
                              std::vector<QueryNode> &internal_to_external_node_map)
{
    std::vector<NodeBasedEdge> edge_list;

    boost::filesystem::ifstream input_stream(config.output_file_name,
                                             std::ios::in | std::ios::binary);

    std::vector<NodeID> barrier_list;
    std::vector<NodeID> traffic_light_list;
    NodeID number_of_node_based_nodes = util::loadNodesFromFile(
        input_stream, barrier_list, traffic_light_list, internal_to_external_node_map);

    util::SimpleLogger().Write() << " - " << barrier_list.size() << " bollard nodes, "
                                 << traffic_light_list.size() << " traffic lights";

    // insert into unordered sets for fast lookup
    barrier_nodes.insert(barrier_list.begin(), barrier_list.end());
    traffic_lights.insert(traffic_light_list.begin(), traffic_light_list.end());

    barrier_list.clear();
    barrier_list.shrink_to_fit();
    traffic_light_list.clear();
    traffic_light_list.shrink_to_fit();

    util::loadEdgesFromFile(input_stream, edge_list);

    if (edge_list.empty())
    {
        util::SimpleLogger().Write(logWARNING) << "The input data is empty, exiting.";
        return std::shared_ptr<util::NodeBasedDynamicGraph>();
    }

    return util::NodeBasedDynamicGraphFromEdges(number_of_node_based_nodes, edge_list);
}

/**
 \brief Building an edge-expanded graph from node-based input and turn restrictions
*/
std::pair<std::size_t, EdgeID>
Extractor::BuildEdgeExpandedGraph(ScriptingEnvironment &scripting_environment,
                                  std::vector<QueryNode> &internal_to_external_node_map,
                                  std::vector<EdgeBasedNode> &node_based_edge_list,
                                  std::vector<bool> &node_is_startpoint,
                                  std::vector<EdgeWeight> &edge_based_node_weights,
                                  util::DeallocatingVector<EdgeBasedEdge> &edge_based_edge_list,
                                  const std::string &intersection_class_output_file)
{
    std::unordered_set<NodeID> barrier_nodes;
    std::unordered_set<NodeID> traffic_lights;

    auto restriction_map = LoadRestrictionMap();
    auto node_based_graph =
        LoadNodeBasedGraph(barrier_nodes, traffic_lights, internal_to_external_node_map);

    CompressedEdgeContainer compressed_edge_container;
    GraphCompressor graph_compressor;
    graph_compressor.Compress(barrier_nodes,
                              traffic_lights,
                              *restriction_map,
                              *node_based_graph,
                              compressed_edge_container);

    compressed_edge_container.SerializeInternalVector(config.geometry_output_path);

    util::NameTable name_table(config.names_file_name);

    // could use some additional capacity? To avoid a copy during processing, though small data so
    // probably not that important.
    std::vector<std::uint32_t> turn_lane_offsets;
    std::vector<guidance::TurnLaneType::Mask> turn_lane_masks;
    std::tie(turn_lane_offsets, turn_lane_masks) = transformTurnLaneMapIntoArrays(turn_lane_map);

    EdgeBasedGraphFactory edge_based_graph_factory(
        node_based_graph,
        compressed_edge_container,
        barrier_nodes,
        traffic_lights,
        std::const_pointer_cast<RestrictionMap const>(restriction_map),
        internal_to_external_node_map,
        scripting_environment.GetProfileProperties(),
        name_table,
        turn_lane_offsets,
        turn_lane_masks,
        turn_lane_map);

    edge_based_graph_factory.Run(scripting_environment,
                                 config.edge_output_path,
                                 config.turn_lane_data_file_name,
                                 config.edge_segment_lookup_path,
                                 config.edge_penalty_path,
                                 config.generate_edge_lookup);

    WriteTurnLaneData(config.turn_lane_descriptions_file_name);

    edge_based_graph_factory.GetEdgeBasedEdges(edge_based_edge_list);
    edge_based_graph_factory.GetEdgeBasedNodes(node_based_edge_list);
    edge_based_graph_factory.GetStartPointMarkers(node_is_startpoint);
    edge_based_graph_factory.GetEdgeBasedNodeWeights(edge_based_node_weights);
    auto max_edge_id = edge_based_graph_factory.GetHighestEdgeID();

    const std::size_t number_of_node_based_nodes = node_based_graph->GetNumberOfNodes();

    WriteIntersectionClassificationData(intersection_class_output_file,
                                        edge_based_graph_factory.GetBearingClassIds(),
                                        edge_based_graph_factory.GetBearingClasses(),
                                        edge_based_graph_factory.GetEntryClasses());

    return std::make_pair(number_of_node_based_nodes, max_edge_id);
}

/**
  \brief Writing info on original (node-based) nodes
 */
void Extractor::WriteNodeMapping(const std::vector<QueryNode> &internal_to_external_node_map)
{
    boost::filesystem::ofstream node_stream(config.node_output_path, std::ios::binary);
    const unsigned size_of_mapping = internal_to_external_node_map.size();
    node_stream.write((char *)&size_of_mapping, sizeof(unsigned));
    if (size_of_mapping > 0)
    {
        node_stream.write((char *)internal_to_external_node_map.data(),
                          size_of_mapping * sizeof(QueryNode));
    }
}

/**
    \brief Building rtree-based nearest-neighbor data structure

    Saves tree into '.ramIndex' and leaves into '.fileIndex'.
 */
void Extractor::BuildRTree(std::vector<EdgeBasedNode> node_based_edge_list,
                           std::vector<bool> node_is_startpoint,
                           const std::vector<QueryNode> &internal_to_external_node_map)
{
    util::SimpleLogger().Write() << "constructing r-tree of " << node_based_edge_list.size()
                                 << " edge elements build on-top of "
                                 << internal_to_external_node_map.size() << " coordinates";

    BOOST_ASSERT(node_is_startpoint.size() == node_based_edge_list.size());

    // Filter node based edges based on startpoint
    auto out_iter = node_based_edge_list.begin();
    auto in_iter = node_based_edge_list.begin();
    for (auto index : util::irange<std::size_t>(0UL, node_is_startpoint.size()))
    {
        BOOST_ASSERT(in_iter != node_based_edge_list.end());
        if (node_is_startpoint[index])
        {
            *out_iter = *in_iter;
            out_iter++;
        }
        in_iter++;
    }
    auto new_size = out_iter - node_based_edge_list.begin();
    if (new_size == 0)
    {
        throw util::exception("There are no snappable edges left after processing.  Are you "
                              "setting travel modes correctly in the profile?  Cannot continue.");
    }
    node_based_edge_list.resize(new_size);

    TIMER_START(construction);
    util::StaticRTree<EdgeBasedNode, std::vector<QueryNode>> rtree(node_based_edge_list,
                                                                   config.rtree_nodes_output_path,
                                                                   config.rtree_leafs_output_path,
                                                                   internal_to_external_node_map);

    TIMER_STOP(construction);
    util::SimpleLogger().Write() << "finished r-tree construction in " << TIMER_SEC(construction)
                                 << " seconds";
}

void Extractor::WriteEdgeBasedGraph(
    std::string const &output_file_filename,
    EdgeID const max_edge_id,
    util::DeallocatingVector<EdgeBasedEdge> const &edge_based_edge_list)
{

    std::ofstream file_out_stream;
    file_out_stream.open(output_file_filename.c_str(), std::ios::binary);
    const util::FingerPrint fingerprint = util::FingerPrint::GetValid();
    file_out_stream.write((char *)&fingerprint, sizeof(util::FingerPrint));

    util::SimpleLogger().Write() << "[extractor] Writing edge-based-graph edges       ... "
                                 << std::flush;
    TIMER_START(write_edges);

    std::uint64_t number_of_used_edges = edge_based_edge_list.size();
    file_out_stream.write((char *)&number_of_used_edges, sizeof(number_of_used_edges));
    file_out_stream.write((char *)&max_edge_id, sizeof(max_edge_id));

    for (const auto &edge : edge_based_edge_list)
    {
        file_out_stream.write((char *)&edge, sizeof(EdgeBasedEdge));
    }

    TIMER_STOP(write_edges);
    util::SimpleLogger().Write() << "ok, after " << TIMER_SEC(write_edges) << "s" << std::endl;

    util::SimpleLogger().Write() << "Processed " << number_of_used_edges << " edges";
}

void Extractor::WriteIntersectionClassificationData(
    const std::string &output_file_name,
    const std::vector<BearingClassID> &node_based_intersection_classes,
    const std::vector<util::guidance::BearingClass> &bearing_classes,
    const std::vector<util::guidance::EntryClass> &entry_classes) const
{
    std::ofstream file_out_stream(output_file_name.c_str(), std::ios::binary);
    if (!file_out_stream)
    {
        util::SimpleLogger().Write(logWARNING) << "Failed to open " << output_file_name
                                               << " for writing";
        return;
    }

    util::SimpleLogger().Write() << "Writing Intersection Classification Data";
    TIMER_START(write_edges);
    util::writeFingerprint(file_out_stream);
    util::serializeVector(file_out_stream, node_based_intersection_classes);

    // create range table for vectors:
    std::vector<unsigned> bearing_counts;
    bearing_counts.reserve(bearing_classes.size());
    std::uint64_t total_bearings = 0;
    for (const auto &bearing_class : bearing_classes)
    {
        bearing_counts.push_back(
            static_cast<unsigned>(bearing_class.getAvailableBearings().size()));
        total_bearings += bearing_class.getAvailableBearings().size();
    }

    util::RangeTable<> bearing_class_range_table(bearing_counts);
    file_out_stream << bearing_class_range_table;

    file_out_stream.write(reinterpret_cast<const char *>(&total_bearings), sizeof(total_bearings));
    for (const auto &bearing_class : bearing_classes)
    {
        const auto &bearings = bearing_class.getAvailableBearings();
        file_out_stream.write(reinterpret_cast<const char *>(&bearings[0]),
                              sizeof(bearings[0]) * bearings.size());
    }

    if (!static_cast<bool>(file_out_stream))
    {
        throw util::exception("Failed to write to " + output_file_name + ".");
    }

    util::serializeVector(file_out_stream, entry_classes);
    TIMER_STOP(write_edges);
    util::SimpleLogger().Write() << "ok, after " << TIMER_SEC(write_edges) << "s for "
                                 << node_based_intersection_classes.size() << " Indices into "
                                 << bearing_classes.size() << " bearing classes and "
                                 << entry_classes.size() << " entry classes and " << total_bearings
                                 << " bearing values." << std::endl;
}

void Extractor::WriteTurnLaneData(const std::string &turn_lane_file) const
{
    // Write the turn lane data to file
    std::vector<std::uint32_t> turn_lane_offsets;
    std::vector<guidance::TurnLaneType::Mask> turn_lane_masks;
    std::tie(turn_lane_offsets, turn_lane_masks) = transformTurnLaneMapIntoArrays(turn_lane_map);

    util::SimpleLogger().Write() << "Writing turn lane masks...";
    TIMER_START(turn_lane_timer);

    std::ofstream ofs(turn_lane_file, std::ios::binary);
    if (!ofs)
        throw osrm::util::exception("Failed to open " + turn_lane_file + " for writing.");

    if (!util::serializeVector(ofs, turn_lane_offsets))
    {
        util::SimpleLogger().Write(logWARNING) << "Error while writing.";
        return;
    }

    if (!util::serializeVector(ofs, turn_lane_masks))
    {
        util::SimpleLogger().Write(logWARNING) << "Error while writing.";
        return;
    }

    TIMER_STOP(turn_lane_timer);
    util::SimpleLogger().Write() << "done (" << TIMER_SEC(turn_lane_timer) << ")";
}

} // namespace extractor
} // namespace osrm
