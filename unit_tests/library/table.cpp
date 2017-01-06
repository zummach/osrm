#include <boost/test/test_case_template.hpp>
#include <boost/test/unit_test.hpp>

#include "args.hpp"
#include "coordinates.hpp"
#include "fixture.hpp"
#include "waypoint_check.hpp"

#include "osrm/table_parameters.hpp"

#include "osrm/coordinate.hpp"
#include "osrm/engine_config.hpp"
#include "osrm/json_container.hpp"
#include "osrm/osrm.hpp"
#include "osrm/status.hpp"

BOOST_AUTO_TEST_SUITE(table)

BOOST_AUTO_TEST_CASE(test_table_three_coords_one_source_one_dest_matrix)
{
    const auto args = get_args();
    BOOST_REQUIRE_EQUAL(args.size(), 1);

    using namespace osrm;

    auto osrm = getOSRM(args[0]);

    TableParameters params;
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.sources.push_back(0);
    params.destinations.push_back(2);

    json::Object result;

    const auto rc = osrm.Table(params, result);

    BOOST_CHECK(rc == Status::Ok || rc == Status::Error);
    const auto code = result.values.at("code").get<json::String>().value;
    BOOST_CHECK_EQUAL(code, "Ok");

    // check that returned durations error is expected size and proportions
    // this test expects a 1x1 matrix
    const auto &durations_array = result.values.at("durations").get<json::Array>().values;
    BOOST_CHECK_EQUAL(durations_array.size(), params.sources.size());
    for (unsigned int i = 0; i < durations_array.size(); i++)
    {
        const auto durations_matrix = durations_array[i].get<json::Array>().values;
        BOOST_CHECK_EQUAL(durations_matrix.size(),
                          params.sources.size() * params.destinations.size());
    }
    // check destinations array of waypoint objects
    const auto &destinations_array = result.values.at("destinations").get<json::Array>().values;
    BOOST_CHECK_EQUAL(destinations_array.size(), params.destinations.size());
    for (const auto &destination : destinations_array)
    {
        BOOST_CHECK(waypoint_check(destination));
    }
    // check sources array of waypoint objects
    const auto &sources_array = result.values.at("sources").get<json::Array>().values;
    BOOST_CHECK_EQUAL(sources_array.size(), params.sources.size());
    for (const auto &source : sources_array)
    {
        BOOST_CHECK(waypoint_check(source));
    }
}

BOOST_AUTO_TEST_CASE(test_table_three_coords_one_source_matrix)
{
    const auto args = get_args();
    BOOST_REQUIRE_EQUAL(args.size(), 1);

    using namespace osrm;

    auto osrm = getOSRM(args[0]);

    TableParameters params;
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.sources.push_back(0);

    json::Object result;

    const auto rc = osrm.Table(params, result);

    BOOST_CHECK(rc == Status::Ok || rc == Status::Error);
    const auto code = result.values.at("code").get<json::String>().value;
    BOOST_CHECK_EQUAL(code, "Ok");

    // check that returned durations error is expected size and proportions
    // this test expects a 1x3 matrix
    const auto &durations_array = result.values.at("durations").get<json::Array>().values;
    BOOST_CHECK_EQUAL(durations_array.size(), params.sources.size());
    for (unsigned int i = 0; i < durations_array.size(); i++)
    {
        const auto durations_matrix = durations_array[i].get<json::Array>().values;
        BOOST_CHECK_EQUAL(durations_matrix[i].get<json::Number>().value, 0);
        BOOST_CHECK_EQUAL(durations_matrix.size(),
                          params.sources.size() * params.coordinates.size());
    }
    // check destinations array of waypoint objects
    const auto &destinations_array = result.values.at("destinations").get<json::Array>().values;
    BOOST_CHECK_EQUAL(destinations_array.size(), params.coordinates.size());
    for (const auto &destination : destinations_array)
    {
        BOOST_CHECK(waypoint_check(destination));
    }
    // check sources array of waypoint objects
    const auto &sources_array = result.values.at("sources").get<json::Array>().values;
    BOOST_CHECK_EQUAL(sources_array.size(), params.sources.size());
    for (const auto &source : sources_array)
    {
        BOOST_CHECK(waypoint_check(source));
    }
}

BOOST_AUTO_TEST_CASE(test_table_three_coordinates_matrix)
{
    const auto args = get_args();
    BOOST_REQUIRE_EQUAL(args.size(), 1);

    using namespace osrm;

    auto osrm = getOSRM(args[0]);

    TableParameters params;
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());
    params.coordinates.push_back(get_dummy_location());

    json::Object result;

    const auto rc = osrm.Table(params, result);

    BOOST_CHECK(rc == Status::Ok || rc == Status::Error);
    const auto code = result.values.at("code").get<json::String>().value;
    BOOST_CHECK_EQUAL(code, "Ok");

    // check that returned durations error is expected size and proportions
    // this test expects a 3x3 matrix
    const auto &durations_array = result.values.at("durations").get<json::Array>().values;
    BOOST_CHECK_EQUAL(durations_array.size(), params.coordinates.size());
    for (unsigned int i = 0; i < durations_array.size(); i++)
    {
        const auto durations_matrix = durations_array[i].get<json::Array>().values;
        BOOST_CHECK_EQUAL(durations_matrix[i].get<json::Number>().value, 0);
        BOOST_CHECK_EQUAL(durations_matrix.size(), params.coordinates.size());
    }
    const auto &destinations_array = result.values.at("destinations").get<json::Array>().values;
    for (const auto &destination : destinations_array)
    {
        BOOST_CHECK(waypoint_check(destination));
    }
    const auto &sources_array = result.values.at("sources").get<json::Array>().values;
    BOOST_CHECK_EQUAL(sources_array.size(), params.coordinates.size());
    for (const auto &source : sources_array)
    {
        BOOST_CHECK(waypoint_check(source));
    }
}

BOOST_AUTO_TEST_SUITE_END()
