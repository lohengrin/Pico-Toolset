#include <cstdio>
#include <cstring>
#include <string>

#include "pico/stdlib.h"
#include "pico_toolset/json_reader.h"

int main() {
    stdio_init_all();
    sleep_ms(2000);

    const char* doc = R"({"now":1760000000000,"ac":[{"flight":"AFR123 ","alt_baro":35000},{"flight":null,"alt_baro":null}]})";

    pico_toolset::JsonReader r(doc, std::strlen(doc));
    std::string key;
    if (r.beginObject()) {
        while (r.nextObjectMember(key)) {
            if (key == "ac" && r.beginArray()) {
                while (r.nextArrayElement()) {
                    if (!r.beginObject()) break;
                    std::string k, flight;
                    double alt = 0;
                    while (r.nextObjectMember(k)) {
                        if (k == "flight") r.parseString(flight);
                        else if (k == "alt_baro") r.parseNumber(alt);
                        else r.skipValue();
                    }
                    std::printf("flight='%s' alt=%.0f\n", flight.c_str(), alt);
                    flight.clear();
                }
            } else {
                r.skipValue();
            }
        }
    }
    std::printf("valid=%d\n", r.valid() ? 1 : 0);
    for (;;) tight_loop_contents();
}
