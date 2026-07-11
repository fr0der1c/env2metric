#include "air_quality_parser.hpp"

#include <cerrno>
#include <cstdlib>

namespace {

constexpr int kMinFieldCount = 7;
constexpr int kMaxFieldCount = 8;

const char* skip_spaces(const char* cursor)
{
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    return cursor;
}

bool is_field_end(char ch)
{
    return ch == ',' || ch == '\0' || ch == '\r' || ch == '\n';
}

ParseAirQualityResult parse_error(const std::string& message)
{
    ParseAirQualityResult result;
    result.error = message;
    return result;
}

bool parse_double_field(const char*& cursor, int index, double& value,
                        std::string& error)
{
    cursor = skip_spaces(cursor);
    if (is_field_end(*cursor)) {
        error = "field " + std::to_string(index) + " is empty";
        return false;
    }

    char* end = nullptr;
    errno = 0;
    value = std::strtod(cursor, &end);
    if (errno != 0 || end == cursor) {
        error = "field " + std::to_string(index) + " is not a number";
        return false;
    }

    cursor = skip_spaces(end);
    if (!is_field_end(*cursor)) {
        error = "field " + std::to_string(index) + " has trailing text";
        return false;
    }

    if (*cursor == ',') {
        ++cursor;
    }
    return true;
}

bool parse_integer_field(const char*& cursor, int index, std::int64_t& value,
                         std::string& error)
{
    cursor = skip_spaces(cursor);
    if (is_field_end(*cursor)) {
        error = "field " + std::to_string(index) + " is empty";
        return false;
    }

    char* end = nullptr;
    errno = 0;
    value = std::strtoll(cursor, &end, 10);
    if (errno != 0 || end == cursor) {
        error = "field " + std::to_string(index) + " is not an integer";
        return false;
    }

    cursor = skip_spaces(end);
    if (*cursor != '\0' && *cursor != '\r' && *cursor != '\n') {
        error = "field " + std::to_string(index) + " has trailing text";
        return false;
    }

    return true;
}

int count_fields(const char* line)
{
    int fields = 1;
    for (const char* cursor = line; *cursor != '\0'; ++cursor) {
        if (*cursor == ',') {
            ++fields;
        }
    }
    return fields;
}

} // namespace

ParseAirQualityResult parse_air_quality_line(const char* line)
{
    const int field_count = count_fields(line);
    if (field_count != kMinFieldCount && field_count != kMaxFieldCount) {
        return parse_error("expected 7 or 8 fields, got " +
                           std::to_string(field_count));
    }

    const char* cursor = line;
    double values[kMaxFieldCount - 1] = {};
    std::string error;

    for (int index = 0; index + 1 < field_count; ++index) {
        if (!parse_double_field(cursor, index + 1, values[index], error)) {
            return parse_error(error);
        }
    }

    std::int64_t sequence = 0;
    if (!parse_integer_field(cursor, field_count, sequence, error)) {
        return parse_error(error);
    }

    ParseAirQualityResult result;
    result.ok = true;
    result.sample.particle_0_3um = values[0];
    result.sample.pm25 = values[1];
    result.sample.hcho = values[2];
    result.sample.co2 = values[3];
    result.sample.temperature = values[4];
    result.sample.humidity = values[5];
    result.sample.has_voc = field_count == kMaxFieldCount;
    result.sample.voc_raw_ppb = result.sample.has_voc ? values[6] : 0.0;
    result.sample.sequence = sequence;
    return result;
}
