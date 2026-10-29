#include "third_party/doctest/doctest.h"

#include "PebbleFL/Common/PblHashTable.h"
#include "RedEngineFL/Graphics/PC/pcDisplaySettings.h"
#include "RedEngineFL/input/pc/pcXInputPad.h"

TEST_CASE("hash pin ai" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("ai") == 0x4424F79F);
}

TEST_CASE("hash pin autoaim" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("autoaim") == 0x68F63ADB);
}

TEST_CASE("hash pin autonet" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("autonet") == 0xC23937B9);
}

TEST_CASE("hash pin bandwidth" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("bandwidth") == 0x91739772);
}

TEST_CASE("hash pin bootlagger" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("bootlagger") == 0x5D62BEB3);
}

TEST_CASE("hash pin bots" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("bots") == 0xE181E077);
}

TEST_CASE("hash pin connect" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("connect") == 0xAAE0CCF9);
}

TEST_CASE("hash pin difficulty" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("difficulty") == 0x09CFDCF0);
}

TEST_CASE("hash pin email" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("email") == 0x8A8753C7);
}

TEST_CASE("hash pin firstperson" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("firstperson") == 0x6363D774);
}

TEST_CASE("hash pin fixedfunction" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("fixedfunction") == 0xF5DD1FBD);
}

TEST_CASE("hash pin fixedrate" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("fixedrate") == 0x7AC8BB2B);
}

TEST_CASE("hash pin framelock" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("framelock") == 0xE3D8AB51);
}

TEST_CASE("hash pin gamename" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("gamename") == 0x4F0C4904);
}

TEST_CASE("hash pin gameslice" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("gameslice") == 0x98BF5C17);
}

TEST_CASE("hash pin heroes" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("heroes") == 0xA8C4C105);
}

TEST_CASE("hash pin heroruleplayer" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("heroruleplayer") == 0xECD7701E);
}

TEST_CASE("hash pin herorulerespawn" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("herorulerespawn") == 0x3EC8DA95);
}

TEST_CASE("hash pin heroruleteam" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("heroruleteam") == 0xF546DAFE);
}

TEST_CASE("hash pin heroruleunlock" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("heroruleunlock") == 0x202DB217);
}

TEST_CASE("hash pin install" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("install") == 0x8463E1A0);
}

TEST_CASE("hash pin jitter" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("jitter") == 0x02049853);
}

TEST_CASE("hash pin lan" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("lan") == 0x56608010);
}

TEST_CASE("hash pin lang" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("lang") == 0x70EA3B55);
}

TEST_CASE("hash pin latency" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("latency") == 0xF2FB8359);
}

TEST_CASE("hash pin loadspam" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("loadspam") == 0xC53B7844);
}

TEST_CASE("hash pin logfinalscore" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("logfinalscore") == 0x09C3E0FF);
}

TEST_CASE("hash pin loss" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("loss") == 0xE7347B1A);
}

TEST_CASE("hash pin name" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("name") == 0x8D39BDE6);
}

TEST_CASE("hash pin netupdatesize" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("netupdatesize") == 0x1503F968);
}

TEST_CASE("hash pin noai" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("noai") == 0x81BA46B8);
}

TEST_CASE("hash pin noawards" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("noawards") == 0x2E325FB4);
}

TEST_CASE("hash pin nodrag" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nodrag") == 0x718E7AD4);
}

TEST_CASE("hash pin noframelock" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("noframelock") == 0xCD154944);
}

TEST_CASE("hash pin nointro" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nointro") == 0x77E5E744);
}

TEST_CASE("hash pin nojournal" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nojournal") == 0x204F0D0D);
}

TEST_CASE("hash pin nolegal" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nolegal") == 0x3F0ECDEB);
}

TEST_CASE("hash pin nomovies" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nomovies") == 0xDABA84C3);
}

TEST_CASE("hash pin nopoolerrors" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nopoolerrors") == 0xD43981B7);
}

TEST_CASE("hash pin nopregame" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nopregame") == 0xF6976B61);
}

TEST_CASE("hash pin norender" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("norender") == 0x75E8450E);
}

TEST_CASE("hash pin nosound" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nosound") == 0xF4553EF1);
}

TEST_CASE("hash pin nostartupmusic" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nostartupmusic") == 0x31694A02);
}

TEST_CASE("hash pin noteamdamage" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("noteamdamage") == 0xE51023BA);
}

TEST_CASE("hash pin nowait" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nowait") == 0xBC494AFF);
}

TEST_CASE("hash pin nowaitlate" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nowaitlate") == 0x4BE2EE33);
}

TEST_CASE("hash pin oneworld" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("oneworld") == 0x2264B8ED);
}

TEST_CASE("hash pin password" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("password") == 0x364B5F18);
}

TEST_CASE("hash pin playback" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("playback") == 0xCF8A43EC);
}

TEST_CASE("hash pin playerlimit" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("playerlimit") == 0x3A00D703);
}

TEST_CASE("hash pin poolerrors" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("poolerrors") == 0x351FC388);
}

TEST_CASE("hash pin ppass" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("ppass") == 0xE0EBCA12);
}

TEST_CASE("hash pin profile" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("profile") == 0x4674CAEE);
}

TEST_CASE("hash pin randomize" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("randomize") == 0x331843BC);
}

TEST_CASE("hash pin record" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("record") == 0x593058CC);
}

TEST_CASE("hash pin resolution" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("resolution") == 0x1D215C8F);
}

TEST_CASE("hash pin safemode" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("safemode") == 0x0451542B);
}

TEST_CASE("hash pin showallsessions" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("showallsessions") == 0x4EAE0006);
}

TEST_CASE("hash pin showlegal" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("showlegal") == 0x0481D555);
}

TEST_CASE("hash pin skill" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("skill") == 0xCA15D036);
}

TEST_CASE("hash pin skiplegal" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("skiplegal") == 0xBBB3B003);
}

TEST_CASE("hash pin soundwarning" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("soundwarning") == 0xC16650C8);
}

TEST_CASE("hash pin spaceassaultscorelimit" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("spaceassaultscorelimit") == 0xD5C0B6F5);
}

TEST_CASE("hash pin team" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("team") == 0xA2FD7D0C);
}

TEST_CASE("hash pin throttle" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("throttle") == 0x378F32DD);
}

TEST_CASE("hash pin voicemode" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("voicemode") == 0xDADFA856);
}

TEST_CASE("hash pin win" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("win") == 0x0BF7CB59);
}

TEST_CASE("hash pin borderless" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("borderless") == 0x49DA825A);
    CHECK((unsigned int)PblHash("borderless") == DisplayCfg::kHashBorderless);
}

TEST_CASE("hash pin adapter" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("adapter") == 0xE7CAE794);
    CHECK((unsigned int)PblHash("adapter") == DisplayCfg::kHashAdapter);
}

TEST_CASE("hash pin refresh" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("refresh") == 0xD4F26634);
    CHECK((unsigned int)PblHash("refresh") == DisplayCfg::kHashRefresh);
}

TEST_CASE("hash pin vsync" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("vsync") == 0x06F8F066);
    CHECK((unsigned int)PblHash("vsync") == DisplayCfg::kHashVsync);
}

TEST_CASE("hash pin fps" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("fps") == 0xBDD34AA8);
    CHECK((unsigned int)PblHash("fps") == DisplayCfg::kHashFps);
}

TEST_CASE("hash pin nowaitquery" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nowaitquery") == 0xFF222165);
    CHECK((unsigned int)PblHash("nowaitquery") == DisplayCfg::kHashNoWaitQuery);
}

TEST_CASE("hash pin nodpi" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("nodpi") == 0x5B1931B5);
    CHECK((unsigned int)PblHash("nodpi") == DisplayCfg::kHashNoDpi);
}

TEST_CASE("hash pin swcursor" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("swcursor") == 0x21567C6D);
    CHECK((unsigned int)PblHash("swcursor") == DisplayCfg::kHashSwCursor);
}

TEST_CASE("hash pin displayini" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("displayini") == 0x8CDD8845);
    CHECK((unsigned int)PblHash("displayini") == DisplayCfg::kHashDisplayIni);
}

TEST_CASE("hash pin is case insensitive" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("NoRender") == (unsigned int)PblHash("norender"));
}

TEST_CASE("hash pin noxinput" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("noxinput") == 0x0BFED71A);
    CHECK((unsigned int)PblHash("noxinput") == XInputPad::kHashNoXInput);
}

TEST_CASE("hash pin norumble" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("norumble") == 0x820BCB67);
    CHECK((unsigned int)PblHash("norumble") == XInputPad::kHashNoRumble);
}
