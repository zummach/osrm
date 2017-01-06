#include "extractor/extractor.hpp"
#include "extractor/extractor_config.hpp"
#include "extractor/scripting_environment_lua.hpp"
#include "util/simple_logger.hpp"
#include "util/version.hpp"

#include <tbb/task_scheduler_init.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>
#include <exception>
#include <new>

using namespace osrm;

enum class return_code : unsigned
{
    ok,
    fail,
    exit
};

return_code parseArguments(int argc, char *argv[], extractor::ExtractorConfig &extractor_config)
{
    // declare a group of options that will be allowed only on command line
    boost::program_options::options_description generic_options("Options");
    generic_options.add_options()("version,v", "Show version")("help,h", "Show this help message");

    // declare a group of options that will be allowed both on command line
    boost::program_options::options_description config_options("Configuration");
    config_options.add_options()(
        "profile,p",
        boost::program_options::value<boost::filesystem::path>(&extractor_config.profile_path)
            ->default_value("profile.lua"),
        "Path to LUA routing profile")(
        "threads,t",
        boost::program_options::value<unsigned int>(&extractor_config.requested_num_threads)
            ->default_value(tbb::task_scheduler_init::default_num_threads()),
        "Number of threads to use")(
        "generate-edge-lookup",
        boost::program_options::value<bool>(&extractor_config.generate_edge_lookup)
            ->implicit_value(true)
            ->default_value(false),
        "Generate a lookup table for internal edge-expanded-edge IDs to OSM node pairs")(
        "small-component-size",
        boost::program_options::value<unsigned int>(&extractor_config.small_component_size)
            ->default_value(1000),
        "Number of nodes required before a strongly-connected-componennt is considered big "
        "(affects nearest neighbor snapping)");

    // hidden options, will be allowed on command line, but will not be
    // shown to the user
    boost::program_options::options_description hidden_options("Hidden options");
    hidden_options.add_options()(
        "input,i",
        boost::program_options::value<boost::filesystem::path>(&extractor_config.input_path),
        "Input file in .osm, .osm.bz2 or .osm.pbf format");

    // positional option
    boost::program_options::positional_options_description positional_options;
    positional_options.add("input", 1);

    // combine above options for parsing
    boost::program_options::options_description cmdline_options;
    cmdline_options.add(generic_options).add(config_options).add(hidden_options);

    const auto *executable = argv[0];
    boost::program_options::options_description visible_options(
        boost::filesystem::path(executable).filename().string() +
        " <input.osm/.osm.bz2/.osm.pbf> [options]");
    visible_options.add(generic_options).add(config_options);

    // parse command line options
    boost::program_options::variables_map option_variables;
    try
    {
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv)
                                          .options(cmdline_options)
                                          .positional(positional_options)
                                          .run(),
                                      option_variables);
    }
    catch (const boost::program_options::error &e)
    {
        util::SimpleLogger().Write(logWARNING) << "[error] " << e.what();
        return return_code::fail;
    }

    if (option_variables.count("version"))
    {
        util::SimpleLogger().Write() << OSRM_VERSION;
        return return_code::exit;
    }

    if (option_variables.count("help"))
    {
        util::SimpleLogger().Write() << visible_options;
        return return_code::exit;
    }

    boost::program_options::notify(option_variables);

    if (!option_variables.count("input"))
    {
        util::SimpleLogger().Write() << visible_options;
        return return_code::exit;
    }

    return return_code::ok;
}

int main(int argc, char *argv[]) try
{
    util::LogPolicy::GetInstance().Unmute();
    extractor::ExtractorConfig extractor_config;

    const auto result = parseArguments(argc, argv, extractor_config);

    if (return_code::fail == result)
    {
        return EXIT_FAILURE;
    }

    if (return_code::exit == result)
    {
        return EXIT_SUCCESS;
    }

    extractor_config.UseDefaultOutputNames();

    if (1 > extractor_config.requested_num_threads)
    {
        util::SimpleLogger().Write(logWARNING) << "Number of threads must be 1 or larger";
        return EXIT_FAILURE;
    }

    if (!boost::filesystem::is_regular_file(extractor_config.input_path))
    {
        util::SimpleLogger().Write(logWARNING)
            << "Input file " << extractor_config.input_path.string() << " not found!";
        return EXIT_FAILURE;
    }

    if (!boost::filesystem::is_regular_file(extractor_config.profile_path))
    {
        util::SimpleLogger().Write(logWARNING)
            << "Profile " << extractor_config.profile_path.string() << " not found!";
        return EXIT_FAILURE;
    }

    // setup scripting environment
    extractor::LuaScriptingEnvironment scripting_environment(
        extractor_config.profile_path.string().c_str());
    return extractor::Extractor(extractor_config).run(scripting_environment);
}
catch (const std::bad_alloc &e)
{
    util::SimpleLogger().Write(logWARNING) << "[exception] " << e.what();
    util::SimpleLogger().Write(logWARNING)
        << "Please provide more memory or consider using a larger swapfile";
    return EXIT_FAILURE;
}
