# Changelog

## [1.1.0](https://github.com/miguetronic/timelapse-switch/compare/1.0.0...1.1.0) (2026-10-23)


### Features

* a Windows installer, and one Linux download for every system ([80baf19](https://github.com/miguetronic/timelapse-switch/commit/80baf1909a14ae10b4d426d424ddb19e4ab5994c))


### Bug Fixes

* keep the plugin's name out of the file, plain x in chapters ([0626007](https://github.com/miguetronic/timelapse-switch/commit/06260070b593f6e8956ffd794f6d17d8afb38680))

## [1.0.0](https://github.com/miguetronic/timelapse-switch/compare/0.1.0...1.0.0) (2026-10-18)


### Features

* **core:** gates that decide which frames and audio reach the encoder ([64196c4](https://github.com/miguetronic/timelapse-switch/commit/64196c47dbb47e32e4148142f2ce10155333ad36))
* **core:** lock-free ring that hands audio blocks between threads ([5eafe47](https://github.com/miguetronic/timelapse-switch/commit/5eafe47b461b69b8f21441b56eb4dc4f847fd0d1))
* **core:** stage ring for road C, with ADR-012 ([6c78c2c](https://github.com/miguetronic/timelapse-switch/commit/6c78c2c3151b33639c60ec061ea5eb8cf678a6c6))
* **core:** timeline from recording time to final video time ([81a23af](https://github.com/miguetronic/timelapse-switch/commit/81a23af7959e4fc4105382079cc9e8a968c5eee8))
* **core:** what each button and hotkey means, and what the dock shows ([3f4587a](https://github.com/miguetronic/timelapse-switch/commit/3f4587af0d3a8aba907af103e449935ee1dd6d2d))
* **dock-view:** both recordings running in the health line (E302) ([72cdadb](https://github.com/miguetronic/timelapse-switch/commit/72cdadbd0d0cdab49a87cdeb143e087af429f181))
* **dock-view:** the health line, the details and the speed map ([4683f4e](https://github.com/miguetronic/timelapse-switch/commit/4683f4efd1150885979dc539de87e3113f01f75d))
* **dock-view:** what each zone shows while paused ([1e6ce51](https://github.com/miguetronic/timelapse-switch/commit/1e6ce5110a7028e27f41ea88a864d84029006d82))
* **dock:** a menu with Help and About, by Miguetronic ([b6505df](https://github.com/miguetronic/timelapse-switch/commit/b6505df999b9304edfd0d483cdf1f7ec29455da9))
* **dock:** a rail whose first segment takes its own width ([a29f071](https://github.com/miguetronic/timelapse-switch/commit/a29f071212f1f5916206850f236627a98cda55e5))
* **dock:** cards visual direction prototype ([a72e4c5](https://github.com/miguetronic/timelapse-switch/commit/a72e4c5b3cd34f4a3d4688cffe962049a198187d))
* **dock:** cards with the transport and native grafts, first pass ([11620eb](https://github.com/miguetronic/timelapse-switch/commit/11620eb8a7212e5d164d86e153659a399850e860))
* **dock:** Pause and Resume, and the paused state ([c7795ab](https://github.com/miguetronic/timelapse-switch/commit/c7795ab614dc0c91a4023a6459647a94041ffeb9))
* **dock:** tell TSW-E302 in the health line, in both languages ([d5a3103](https://github.com/miguetronic/timelapse-switch/commit/d5a310363dd425e6d85e29052d861bad5fd4670e))
* **dock:** the health line, Details on request and the speed map ([02f0bf9](https://github.com/miguetronic/timelapse-switch/commit/02f0bf96d1c39a73253d08f836643128e0478abf))
* **dock:** the Timelapse Switch dock ([99799d6](https://github.com/miguetronic/timelapse-switch/commit/99799d6acfe8b131d9855d8c11b4d0c492d91722))
* **dock:** while paused, one "Resume in" rail instead of two ([e3fa614](https://github.com/miguetronic/timelapse-switch/commit/e3fa6144a0ee760f4ed8112bf1879f1333da50eb))
* **health:** warn while OBS's own recording runs too (TSW-E302) ([37222d5](https://github.com/miguetronic/timelapse-switch/commit/37222d5585e3ecda34d6e3714185343bfb22fa30))
* pause and resume, in real time and in timelapse ([32b4796](https://github.com/miguetronic/timelapse-switch/commit/32b479618505cd33adbe7afda61c65eeb56712da))
* **recorder:** chapters in Hybrid MP4, and a status for the dock ([60d8129](https://github.com/miguetronic/timelapse-switch/commit/60d81293d5a5eb05c16107b1ad57bffff8835157))
* **recorder:** count stutters in the summary line ([0823edb](https://github.com/miguetronic/timelapse-switch/commit/0823edbb795e9ba84d418d5a6c6e68a5d1ecca11))
* **recorder:** log how full the video feed gets ([b81f1a3](https://github.com/miguetronic/timelapse-switch/commit/b81f1a3280b2de08ec0d915a1dbbb0653f731e5c))
* **recorder:** record at mixed speeds with hotkeys ([a32ba6f](https://github.com/miguetronic/timelapse-switch/commit/a32ba6f0884a3b302ec8b65b1d830e87155513a8))
* **recorder:** road C behind TIMELAPSE_SWITCH_ROAD=C ([8a79f27](https://github.com/miguetronic/timelapse-switch/commit/8a79f2756b523b04e5946d31c03b2babc62f2ae2))
* **recorder:** the plugin chooses road C by itself where supported ([d53b23a](https://github.com/miguetronic/timelapse-switch/commit/d53b23a03fadcb632a57cb66a0c445c25e2741b5))
* session totals, speed map and health, and who made the plugin ([258e865](https://github.com/miguetronic/timelapse-switch/commit/258e865c3cc5461a383e85f98b59844b7e8264be))
* **site:** English website with guide, messages and support ([9e70ae7](https://github.com/miguetronic/timelapse-switch/commit/9e70ae79b988f274ec82bc45d25293e973764ac0))


### Bug Fixes

* **core:** no implicit widening in the fingerprint grid ([7c785b6](https://github.com/miguetronic/timelapse-switch/commit/7c785b60381f16ddde0e436106f70d27f640cbad))
* **core:** the first frame opens the first video slot ([c451226](https://github.com/miguetronic/timelapse-switch/commit/c45122661eceb1b359b451beb6e5414de445dbc2))
* **dock:** a link's focus ring hugs its words ([733d04d](https://github.com/miguetronic/timelapse-switch/commit/733d04d2587d9ca1e8de5102532865d8eb8c3ca5))
* **dock:** cards direction polish after the theme review ([1d500c2](https://github.com/miguetronic/timelapse-switch/commit/1d500c2c57a51e1149db1180dfc6b1000a382057))
* **dock:** keep the focus ring off the edge an armed button outlines ([534643e](https://github.com/miguetronic/timelapse-switch/commit/534643e9a724a29e9a9220b77527da026996abec))
* **dock:** Learn more opens the page of the message shown ([eba4018](https://github.com/miguetronic/timelapse-switch/commit/eba40188a8594f5ae94025691090a808e031f80c))
* **dock:** pass clang-tidy and the P10-8 text rule ([a07d4df](https://github.com/miguetronic/timelapse-switch/commit/a07d4df9ee1746f2d3d829b285c59fea12c9ed05))
* **dock:** scroll instead of squeezing when a theme insets the dock ([10fc6bc](https://github.com/miguetronic/timelapse-switch/commit/10fc6bcf79e73616414f756a13c7e7b5c197c7aa))
* **dock:** stack Pause and Stop only when a word would be cut ([f5f63b6](https://github.com/miguetronic/timelapse-switch/commit/f5f63b6b94d66f892750d91aab8c1db27000e81e))
* **dock:** the status row is never an empty hole ([9cb4332](https://github.com/miguetronic/timelapse-switch/commit/9cb4332c7a1fce250fbad0ef9420f1d54a873546))
* **release:** draft releases, Release builds, every system attached ([b2dd7e2](https://github.com/miguetronic/timelapse-switch/commit/b2dd7e2c1d01d8449c3cd026ac5e14c4c91e128d))
* **road-c:** ask for the copy where OBS does, map two frames later ([4208c01](https://github.com/miguetronic/timelapse-switch/commit/4208c012596e36424210868a62922433037466d1))
* **road-check:** no signed shift or widened product in the list size ([2753dee](https://github.com/miguetronic/timelapse-switch/commit/2753deeb13d400088b5b15ae40078f5cb05e0eeb))
* **tools:** format checks fail on errors and cover tests ([bf33995](https://github.com/miguetronic/timelapse-switch/commit/bf33995f2fc359765c62f9f07d5da56502d77b1a))
* **tools:** write the dash characters as escapes in check.sh ([1a47dff](https://github.com/miguetronic/timelapse-switch/commit/1a47dff229646fd17e3aadc65d01f2c40951c353))


### Performance

* **road-c:** nothing in front of the preview, and no waits ([0fc34ae](https://github.com/miguetronic/timelapse-switch/commit/0fc34ae0f9719cf2b6fd08fdd6b8ee20e1c7c693))


### Miscellaneous

* release 1.0.0 ([3e27f55](https://github.com/miguetronic/timelapse-switch/commit/3e27f550fc3841e43507454d5612a9f949fe985f))

## Changelog

All notable changes to Timelapse Switch, newest first.

This file is generated automatically from the commit messages by release-please at each release (see `docs/releasing.md`); please do not edit it by hand. Versions follow [Semantic Versioning](https://semver.org/). The first release will be 1.0.0.
