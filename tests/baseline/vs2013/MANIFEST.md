# VS2013 baseline manifest

## Build

| Key | Value |
|-----|-------|
| toolset | v120_xp |
| cl | 18.00.40629 |
| msvcr120.dll | 12.00.40664.0 |
| configuration | Release |
| commit | 1168dd412416f629ad22302d1a55950e63774a94 |
| tracked tree clean | yes |

## Archive

Archive: `%USERPROFILE%\OpenSWBF2-baseline\vs2013\vs2013-release-1168dd4.zip` SHA-256 `4A7A1D9312A1E328326FB2A2D2E4952354EB5F71AF895CFEFCAD96D451D80684`

| Archived file | SHA-256 |
|---------------|---------|
| `BattlefrontII.exe` | 443ED313E31634C7923E783B6155011D15655DDB803C8ED301EA98249A36EAB3 |
| `LuaCompiler.exe` | 90716E6F4CEA78FA1CD6EBB2A0C312E3C9CCFCA4EEE184C2F004E7123A8A921B |
| `BattlefrontII.pdb` | 9CFCBE8CA6A567FAF63940B7899ADB3461E068B093E61719F9DDE9CD6407818C |
| `BattlefrontII.map` | 21EEC26F2B27912A90EC49009B2E61AF3286129183D6125EB53B4CE304A9225E |
| `cl.command.1.tlog` | E5C5D1A455E62CC37CF6B27DED48D1EDAB50D20B96D74158FB1872AD5CC750F2 |

## Text artifacts

| File | SHA-256 | Produced by |
|------|---------|-------------|
| `digests.txt` | A6C3F1C777CDB1F84A3C932A6932EA5721C4B8898E72A49024C606866EC76E3D | `capture-vs2013.ps1 -Collect` |
| `digests-debug.txt` | D285E11C8F109D5C586D948E8F2A653B2EDD89D7367838A74D6EC10BA3B1DB44 | `capture-vs2013.ps1 -DebugDigests` from `run.ps1 -Smoke Twice -SmokeConfiguration Debug` |
| `digests-hog.txt` | A6C3F1C777CDB1F84A3C932A6932EA5721C4B8898E72A49024C606866EC76E3D | `capture-vs2013.ps1 -Collect` |
| `digest-trace-geo1c_con.txt` | A3EA86CB6038FAB58178D3549E67E8EF8675347EE1DFD194233F280A6A0EF6F5 | `capture-vs2013.ps1 -Collect` |
| `digest-trace-hot1g_con.txt` | C22AA71DB3546F113343377DFA6786B14E19D82B9817F510E54EC74CBF67C594 | `capture-vs2013.ps1 -Collect` |
| `digest-trace-nab2g_c.txt` | F7B2B8670597008821CA60FE2CFF9C7F51C95D2FE42E51B7DB8D5C96953FCBDA | `capture-vs2013.ps1 -Collect` |
| `digest-trace-spa3c_ass.txt` | 47EB12CE384D249E5FC947E37DC8C5C13F6F89A95A1DD899B687503434AC8186 | `capture-vs2013.ps1 -Collect` |
| `factory-geo1c_con.txt` | 0C3C380156342E1C492092575630152541B648FF038F8B6285A840C6F73D9C7A | `capture-vs2013.ps1 -Collect` |
| `factory-hot1g_con.txt` | 21739D89813BFA37EC08B53900C0EDE5AD79CA96D8DFDEBAEDE5692127F3E5D6 | `capture-vs2013.ps1 -Collect` |
| `factory-nab2g_c.txt` | 2192336505D7A03D0ABD400927A962AFB6EF09358AB6E572657AC7A4EC0C94E7 | `capture-vs2013.ps1 -Collect` |
| `factory-spa3c_ass.txt` | 0FDB0B17217BBC000279BBCD49AE05681C8ACB199306878FE0AA8E485714CABF | `capture-vs2013.ps1 -Collect` |
| `guard-revert.txt` | D835BD2167183BD4AAF3E7D9FB0EF6A0338594AE9782E2EC0F2EE32C38F2F9A3 | `capture-vs2013.ps1 -Tables` from `guard-revert.ps1` |
| `hardening.txt` | FE2C508ED4156D78D0BD8898C8C45B5F9683C0FFE38CCF7616C52AD86DDCB8AA | `capture-vs2013.ps1 -Tables` from `run.ps1 -Hardening` |
| `initializers.txt` | 31A054E3C448745BC43E508953EF5968CF327D1CE9C6EC56D844BA69992433AD | `capture-vs2013.ps1 -Map` |
| `initializers-di.txt` | 4ACF329F1C421CB1872AE1AB3AB086C944B3DD1C5A13B0F40FF32F63C834E145 | `capture-vs2013.ps1 -Map` |
| `initializers-discarded.txt` | 27B0FB7FD142D05FC90699CD89CF033D54A6308FEB8B94D036C97EB1A8C85B32 | `capture-vs2013.ps1 -Map` |
| `layouts\Aimer@Aimer.txt` | 3AC1D3AD1DB047BCA1D470B48986BAE3D3F1DFD919575FB2B22DAD0E4AFD01EF | `capture-vs2013.ps1 -Layouts` |
| `layouts\Aimer@MuzzleFlash.txt` | 7DC1BABF366DEF6F028D9DC4DD98DC9DC464E580B9DB3690DA63192FA4B4622C | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@AILowLevel.txt` | 739F21C0C2A65B793A5070B68E62166142C5D864F408F73450D93DF1B33BE2F5 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@AmmoCounter.txt` | A71EAFCD25310B599460BF4A1DFBF043FBFCC3D8C40D088F5187595C4C685CE7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@Character.txt` | 739F21C0C2A65B793A5070B68E62166142C5D864F408F73450D93DF1B33BE2F5 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@CombatUtil.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@GameLoop.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@LuaCallbacks_Mission.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@MineHelper.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@NetState.txt` | A71EAFCD25310B599460BF4A1DFBF043FBFCC3D8C40D088F5187595C4C685CE7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@PowerupUtility.txt` | A71EAFCD25310B599460BF4A1DFBF043FBFCC3D8C40D088F5187595C4C685CE7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@RepairHelper.txt` | 739F21C0C2A65B793A5070B68E62166142C5D864F408F73450D93DF1B33BE2F5 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@UnitController.txt` | A71EAFCD25310B599460BF4A1DFBF043FBFCC3D8C40D088F5187595C4C685CE7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@Weapon.txt` | A71EAFCD25310B599460BF4A1DFBF043FBFCC3D8C40D088F5187595C4C685CE7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@WeaponCannon.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@WeaponCatapult.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@WeaponDestruct.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@WeaponDispenser.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@WeaponGrenade.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@WeaponLaser.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@WeaponRepair.txt` | E39D6FCF0A82CE70CE4004B4DDAD174CDFC1E34F82472F3DC8D243177FC1A085 | `capture-vs2013.ps1 -Layouts` |
| `layouts\AmmoCounter@WeaponShield.txt` | A71EAFCD25310B599460BF4A1DFBF043FBFCC3D8C40D088F5187595C4C685CE7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\CommandFlyer@CommandFlyer.txt` | 01C0997DDDF96695C1C0926E9F72FA10E88EA338BC24FB002FA5AC83ECAB7FDC | `capture-vs2013.ps1 -Layouts` |
| `layouts\CommandHover@CommandHover.txt` | 5D04EEB4E9F8281C0578DC15736CCF46D85C57690E6371D544EDAB66330BFD62 | `capture-vs2013.ps1 -Layouts` |
| `layouts\CommandWalker@CommandPost.txt` | D9E918AB555F306B8FF6F36A263B30BB7D4A5EA14F2045B25291E8DF1E7765A0 | `capture-vs2013.ps1 -Layouts` |
| `layouts\CommandWalker@CommandWalker.txt` | 2AAFA6C89798CF6100A905E440A3E02B3C54F441C3056191C00D2ABBFE24B413 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityBuildingAnimatedArmed@EntityBuildingAnimatedArmed.txt` | E5A4A5649D995366F6092E6E3EB4AC22D17FF14CBF89F080B529B7F7ED0B2BB9 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityCarrier@EntityCarrier.txt` | 6128F57EE4FE72C0C49BDAAE6C6E75269E8A78BB74730BC65D16FD7991928A06 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityCarrierRuntimeLayout@EntityCarrier.txt` | 9CA63D1489A1CFB89F49928184781690E2C2D0A659CC7838E3B621892A0423A4 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@AILowLevel.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@Controllable.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@Controller.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@EntityCarrier.txt` | FDC6103D989D983DCC9814171846750ECE00BE7DED9F1BBAED4110DA4D4E91B5 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@HUDElementTarget.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@MountedTurret.txt` | 76FE361F74361F58E05A73EA1909D06645AB8DF71DA2CDDDDF428DD010C8CC56 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@NavigatorSlide.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@PowerupStation.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@Trackable.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@UnitAgent.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@UnitThreatManager.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@UnitWaitSecondaryAgent.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@VisionManager.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyer@Weapon.txt` | B48F4CE796631D3B3FD7D72BF1955601731B900787883E4A98E26416E25285F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityFlyerRuntimeLayout@EntityCarrier.txt` | EDA2F066F222E2279CFAF8706EB435D263F21706C85FE3E39D65F8B0C19182CD | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityGeometry@EntityGeometry.txt` | 53DE5C680D7351B23B8EB0F20CF937F21C15E67BEA7ED04FD237238A4260232D | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityGeometry@FLEffect.txt` | 3AC8179A32387D07EBEE6E3B15970409D69232D3584C3024DB8EDD6A83008205 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityGeometry@PathFinder.txt` | 3AC8179A32387D07EBEE6E3B15970409D69232D3584C3024DB8EDD6A83008205 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityHover@EntityHover.txt` | F7B61DE91E9D06D72D8C6EED0AFF73D90B966DBAF0D81103595FC161E39F39F7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityHover@PowerupStation.txt` | 11C5DEEE285FB3CD5640EBFA7EE7A57D55530541686A0F3C2C70940C9C854D56 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityHover@UnitTrooperAgent.txt` | 11C5DEEE285FB3CD5640EBFA7EE7A57D55530541686A0F3C2C70940C9C854D56 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityWalker@EntityWalker.txt` | 6945595F4EE185F4376F2702E09C1CCB2119E5D8F9E316AD7A43BFDB537FBBC8 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityWalker@PowerupStation.txt` | F23B5F6AFD2D198C566CA9211AB5691B57C47049776259700055A8E0608D04B8 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityWalker@UnitTrooperAgent.txt` | F23B5F6AFD2D198C566CA9211AB5691B57C47049776259700055A8E0608D04B8 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityWater@CollisionManager.txt` | BB0020431CBD0A8BE0F5D0F446B607A3DB7A6BD571AEE5F5261565C3E4013973 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityWater@EntityWater.txt` | 4DD445BE019AB24D0202FAB22C1C8BD2DDBDFAF0CF4C343FA0744A3FB3F60915 | `capture-vs2013.ps1 -Layouts` |
| `layouts\EntityWater@GameLoop.txt` | 4EE73FAD318768E57259B76BAF116EE77227A69D5B8C7186FDED38A5439F0025 | `capture-vs2013.ps1 -Layouts` |
| `layouts\FLInputManager@FLInputManager.txt` | 4849013EF7CF192A2B0D0B38CC1F57332AB805C0F6B577498403694D1E55C2F3 | `capture-vs2013.ps1 -Layouts` |
| `layouts\GameObject@AttachedEffects.txt` | DC16E0519D23058B1CA4E6E64739B264C0C184B5BFB88EF0E671E75881F25360 | `capture-vs2013.ps1 -Layouts` |
| `layouts\GameObject@FLEffect.txt` | 32E739B9715EEBF6C3D642658BA45319CD54ACC3181EBB5D082E3D17481226CC | `capture-vs2013.ps1 -Layouts` |
| `layouts\GameObject@GameObject.txt` | A07253ACFD78DA3A911CDFD7A02717DADBC0717C0522A135D89D4338043FEC99 | `capture-vs2013.ps1 -Layouts` |
| `layouts\GameObject@PathFinder.txt` | D0BB8361032C033C570D50BF50459A50C8BCA29599366464CA697A769B4E5400 | `capture-vs2013.ps1 -Layouts` |
| `layouts\INDEX.txt` | FC74E45C1021EAEC93076A50AB5448DA9C955BEE681079BB56E648C5EB24E1FA | `capture-vs2013.ps1 -Layouts` |
| `layouts\Ordnance@CollisionManager.txt` | 4B689E54C7D0F40D1B2FDA99290D2912CDBB07419D0199570220B8F70F7082FA | `capture-vs2013.ps1 -Layouts` |
| `layouts\Ordnance@Ordnance.txt` | CDF090B3047716461C15C0C65F525FFC246FE218DAC43CD56D4FA659CFE48E1E | `capture-vs2013.ps1 -Layouts` |
| `layouts\PAGameObjectTeam@ProceduralAnimation.txt` | 6976A7A20685AC1DC2CE8808AF327FC2DA7998FA14CB3BCAB25E45D68470CC1F | `capture-vs2013.ps1 -Layouts` |
| `layouts\RedBorderElement@RedBorderElement.txt` | 01CFE062A7E4F673EE5AFC69B053F32447C07EFD8975BD9473DA9A71959B31A7 | `capture-vs2013.ps1 -Layouts` |
| `layouts\Weapon@Aimer.txt` | 02A457DB7544BDE675D954585082204D66D80EE113C579D3D086A586DD0BEF91 | `capture-vs2013.ps1 -Layouts` |
| `layouts\Weapon@GameLoop.txt` | 02A457DB7544BDE675D954585082204D66D80EE113C579D3D086A586DD0BEF91 | `capture-vs2013.ps1 -Layouts` |
| `layouts\Weapon@OrdnanceBeam.txt` | 02A457DB7544BDE675D954585082204D66D80EE113C579D3D086A586DD0BEF91 | `capture-vs2013.ps1 -Layouts` |
| `layouts\Weapon@ScopeDisplay.txt` | 02A457DB7544BDE675D954585082204D66D80EE113C579D3D086A586DD0BEF91 | `capture-vs2013.ps1 -Layouts` |
| `layouts\Weapon@WeaponCannon.txt` | 02A457DB7544BDE675D954585082204D66D80EE113C579D3D086A586DD0BEF91 | `capture-vs2013.ps1 -Layouts` |
| `layouts\WeaponGrenade@WeaponGrenade.txt` | 242040F625AA04F2DF96066EBD91E83422F1E1A8F13EBC07695B2012560C519E | `capture-vs2013.ps1 -Layouts` |
| `layouts\WeaponRepair@WeaponRepair.txt` | 93A9CC64405C67AB033C4E3D7684AD9643CCB017D0CCEE4682A8D84C1B083CA8 | `capture-vs2013.ps1 -Layouts` |
| `smoke-all.txt` | D3A17DD5F652140BC999F40EF43F7E3E0BFEC43CA2B6807DB270EF59E51DBC55 | `capture-vs2013.ps1 -Tables` from `run.ps1 -Smoke All` |
| `smoke-all-digests.txt` | 5D75137AAE338D23E8E441CA0955A9073C1F969F2A00884FDEA6F65158347ACB | `capture-vs2013.ps1 -Tables` from `run.ps1 -Smoke All` |

## Notes

- Phase 20 should compare the `turns`, `rand` and `state` fields of each digest line, not the `%.3f` `time=` and `snd=` text, because CRT printf rounding may differ.
- Release is the reference configuration for every artifact. The binaries are not in the repo; only their SHA-256 sums are.
- `guard-revert.txt` is kept from the 88273c8 capture. Only its DIGEST-01 row depends on the digest value, and `guard-revert.ps1 -Row DIGEST-01` was rerun on this build: GREEN/RED.
- `smoke-all-digests.txt` row `dedicated` is a real-time run, so its `time=` and `rand=` vary between runs.
