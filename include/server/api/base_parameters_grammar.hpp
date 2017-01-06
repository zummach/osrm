#ifndef SERVER_API_BASE_PARAMETERS_GRAMMAR_HPP
#define SERVER_API_BASE_PARAMETERS_GRAMMAR_HPP

#include "engine/api/base_parameters.hpp"

#include "engine/bearing.hpp"
#include "engine/hint.hpp"
#include "engine/polyline_compressor.hpp"

#include <boost/optional.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>

#include <limits>
#include <string>

namespace osrm
{
namespace server
{
namespace api
{

namespace
{
namespace ph = boost::phoenix;
namespace qi = boost::spirit::qi;
}

template <typename T, char... Fmt> struct no_trailing_dot_policy : qi::real_policies<T>
{
    template <typename Iterator> static bool parse_dot(Iterator &first, Iterator const &last)
    {
        if (first == last || *first != '.')
            return false;

        static const constexpr char fmt[sizeof...(Fmt)] = {Fmt...};

        if (first + sizeof(fmt) < last && std::equal(fmt, fmt + sizeof(fmt), first + 1u))
            return false;

        ++first;
        return true;
    }

    template <typename Iterator> static bool parse_exp(Iterator &, const Iterator &)
    {
        return false;
    }

    template <typename Iterator, typename Attribute>
    static bool parse_exp_n(Iterator &, const Iterator &, Attribute &)
    {
        return false;
    }

    template <typename Iterator, typename Attribute>
    static bool parse_nan(Iterator &, const Iterator &, Attribute &)
    {
        return false;
    }

    template <typename Iterator, typename Attribute>
    static bool parse_inf(Iterator &, const Iterator &, Attribute &)
    {
        return false;
    }
};

template <typename Iterator, typename Signature>
struct BaseParametersGrammar : boost::spirit::qi::grammar<Iterator, Signature>
{
    using json_policy = no_trailing_dot_policy<double, 'j', 's', 'o', 'n'>;

    BaseParametersGrammar(qi::rule<Iterator, Signature> &root_rule)
        : BaseParametersGrammar::base_type(root_rule)
    {
        const auto add_hint = [](engine::api::BaseParameters &base_parameters,
                                 const boost::optional<std::string> &hint_string) {
            if (hint_string)
            {
                base_parameters.hints.emplace_back(engine::Hint::FromBase64(hint_string.get()));
            }
            else
            {
                base_parameters.hints.emplace_back(boost::none);
            }
        };

        const auto add_bearing =
            [](engine::api::BaseParameters &base_parameters,
               boost::optional<boost::fusion::vector2<short, short>> bearing_range) {
                boost::optional<engine::Bearing> bearing;
                if (bearing_range)
                {
                    bearing = engine::Bearing{boost::fusion::at_c<0>(*bearing_range),
                                              boost::fusion::at_c<1>(*bearing_range)};
                }
                base_parameters.bearings.push_back(std::move(bearing));
            };

        polyline_chars = qi::char_("a-zA-Z0-9_.--[]{}@?|\\%~`^");
        base64_char = qi::char_("a-zA-Z0-9--_=");
        unlimited_rule = qi::lit("unlimited")[qi::_val = std::numeric_limits<double>::infinity()];

        bearing_rule =
            (qi::short_ > ',' > qi::short_)[qi::_val = ph::bind(
                                                [](short bearing, short range) {
                                                    return osrm::engine::Bearing{bearing, range};
                                                },
                                                qi::_1,
                                                qi::_2)];

        location_rule =
            (double_ > qi::lit(',') >
             double_)[qi::_val = ph::bind(
                          [](double lon, double lat) {
                              return util::Coordinate(util::toFixed(util::FloatLongitude{lon}),
                                                      util::toFixed(util::FloatLatitude{lat}));
                          },
                          qi::_1,
                          qi::_2)];

        polyline_rule = qi::as_string[qi::lit("polyline(") > +polyline_chars > ')']
                                     [qi::_val = ph::bind(
                                          [](const std::string &polyline) {
                                              return engine::decodePolyline(polyline);
                                          },
                                          qi::_1)];

        query_rule =
            ((location_rule % ';') |
             polyline_rule)[ph::bind(&engine::api::BaseParameters::coordinates, qi::_r1) = qi::_1];

        radiuses_rule = qi::lit("radiuses=") >
                        (-(qi::double_ | unlimited_rule) %
                         ';')[ph::bind(&engine::api::BaseParameters::radiuses, qi::_r1) = qi::_1];

        hints_rule = qi::lit("hints=") >
                     (-qi::as_string[qi::repeat(engine::ENCODED_HINT_SIZE)[base64_char]])[ph::bind(
                         add_hint, qi::_r1, qi::_1)] %
                         ';';

        bearings_rule =
            qi::lit("bearings=") >
            (-(qi::short_ > ',' > qi::short_))[ph::bind(add_bearing, qi::_r1, qi::_1)] % ';';

        base_rule = radiuses_rule(qi::_r1) | hints_rule(qi::_r1) | bearings_rule(qi::_r1);
    }

  protected:
    qi::rule<Iterator, Signature> base_rule;
    qi::rule<Iterator, Signature> query_rule;

  private:
    qi::rule<Iterator, Signature> bearings_rule;
    qi::rule<Iterator, Signature> radiuses_rule;
    qi::rule<Iterator, Signature> hints_rule;

    qi::rule<Iterator, osrm::engine::Bearing()> bearing_rule;
    qi::rule<Iterator, osrm::util::Coordinate()> location_rule;
    qi::rule<Iterator, std::vector<osrm::util::Coordinate>()> polyline_rule;

    qi::rule<Iterator, unsigned char()> base64_char;
    qi::rule<Iterator, std::string()> polyline_chars;
    qi::rule<Iterator, double()> unlimited_rule;
    qi::real_parser<double, json_policy> double_;
};
}
}
}

#endif
