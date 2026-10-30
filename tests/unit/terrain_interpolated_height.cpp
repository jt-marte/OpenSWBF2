#include "third_party/doctest/doctest.h"

#include <string>

#include "testutil.h"

TEST_CASE("terrain interpolated height rounds each step to float" * doctest::test_suite("regression"))
{
    std::string sText;
    REQUIRE(ReadRepoText("src\\RedEngineFL\\Application\\RedTerrain.cpp", sText));
    const std::string sFixed = "const float fH = _mm_cvtss_f32(\n"
                               "        _mm_mul_ss(_mm_sub_ss(_mm_set_ss(GetHeight(iX + 1, iZ + 1)), _mm_set_ss(fH0)), "
                               "_mm_set_ss(fFracZ)));";
    const size_t uiAt = sText.find(sFixed);
    CHECK(uiAt != std::string::npos);
    CHECK(sText.find(sFixed, uiAt + 1) == std::string::npos);
    CHECK(sText.find("(GetHeight(iX + 1, iZ + 1) - fH0) * fFracZ") == std::string::npos);
    CHECK(sText.find("#include <xmmintrin.h>") != std::string::npos);
}
