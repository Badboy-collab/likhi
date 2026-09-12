// Unit tests for CloudTranslit_ParseGoogleResponse — pure offline parser.
// Compile/link: g++ test_cloud_parser.cpp ../tsf/src/cloud_translit.cpp -lwinhttp
#include "../../tsf/include/cloud_translit.h"
#include <iostream>
#include <string>
#include <vector>

using namespace bangla_tsf;

static int g_fail = 0;
static int g_total = 0;

static void Check(bool cond, const char* what) {
    g_total++;
    if (!cond) {
        g_fail++;
        std::cout << "  [FAIL] " << what << "\n";
    }
}

int main() {
    std::vector<std::string> out;

    // 1. Real UTF-8 body (Bangla raw).
    {
        std::string body = "[\"SUCCESS\",[[\"amar\",[\"\u0986\u09ae\u09be\u09b0\","
                           "\"\u0986\u09ae\u09b0\u09be\",\"\u0986\u09ae\u09be\u09b0\u09c7\"]]]]";
        Check(CloudTranslit_ParseGoogleResponse(body, out), "utf8 parse ok");
        Check(out.size() == 3, "utf8 count==3");
        Check(out.size() == 3 && out[0] == "\u0986\u09ae\u09be\u09b0", "utf8 c0 == আমার");
    }

    // 2. \uXXXX escapes decode to UTF-8 (কেমন / কেমনো).
    {
        std::string body = "[\"SUCCESS\",[[\"kemon\",[\"\\u0995\\u09c7\\u09ae\\u09a8\","
                           "\"\\u0995\\u09c7\\u09ae\\u09a8\\u09cb\"]]]]";
        out.clear();
        Check(CloudTranslit_ParseGoogleResponse(body, out), "u-escape parse ok");
        Check(out.size() == 2, "u-escape count==2");
        if (out.size() == 2) {
            Check(out[0] == "\u0995\u09c7\u09ae\u09a8", "u-escape c0 == কেমন");
            Check(out[1] == "\u0995\u09c7\u09ae\u09a8\u09cb", "u-escape c1 == কেমনো");
        }
    }

    // 3. single candidate.
    {
        std::string body = "[\"SUCCESS\",[[\"pc\",[\"\u09aa\u09bf\u09b8\u09bf\"]]]]";
        out.clear();
        Check(CloudTranslit_ParseGoogleResponse(body, out), "single parse ok");
        Check(out.size() == 1 && out[0] == "\u09aa\u09bf\u09b8\u09bf", "single == পিসি");
    }

    // 4. escaped quotes/backslashes inside a string.
    {
        std::string body = "[\"SUCCESS\",[[\"x\",[\"a\\\"b\\\\c\"]]]]";
        out.clear();
        Check(CloudTranslit_ParseGoogleResponse(body, out), "escapes parse ok");
        Check(out.size() == 1 && out[0] == "a\"b\\c", "escapes decoded");
    }

    // 5. Failure / malformed shapes → false or empty, never crash.
    {
        out.clear();
        Check(!CloudTranslit_ParseGoogleResponse("[\"FAILURE\",[[\"amar\",[]]]]", out), "FAILURE rejected");
        out.clear();
        Check(!CloudTranslit_ParseGoogleResponse("[\"SUCCESS\",[[\"amar\",[]]]]", out), "empty candidates rejected");
        out.clear();
        Check(!CloudTranslit_ParseGoogleResponse("not json at all", out), "garbage rejected");
        out.clear();
        Check(!CloudTranslit_ParseGoogleResponse("", out), "empty body rejected");
        out.clear();
        Check(!CloudTranslit_ParseGoogleResponse("[\"SUCCESS\",[[\"amar\",[\"\"]]]]", out), "blank candidate rejected");
        out.clear();
        Check(!CloudTranslit_ParseGoogleResponse("[\"SUCCESS\"]", out), "short success rejected");
    }

    // 6. whitespace tolerance.
    {
        std::string body = " [ \"SUCCESS\" , [ [ \"p\" , [ \"\u09aa\" ] ] ] ] ";
        out.clear();
        Check(CloudTranslit_ParseGoogleResponse(body, out), "whitespace tolerated");
        Check(out.size() == 1 && out[0] == "\u09aa", "ws single == প");
    }

    std::cout << "test_cloud_parser: " << (g_total - g_fail) << "/" << g_total << " passed\n";
    if (g_fail == 0) {
        std::cout << "Status: SUCCESS (ALL PASSED)\n";
        return 0;
    }
    std::cout << "Status: FAILURE (" << g_fail << " failed)\n";
    return 1;
}
