<!--
SPDX-FileCopyrightText: 2024 Roman Gilg <subdiff@gmail.com>
SPDX-License-Identifier: GPL-2.0-or-later
-->
# Changelog
All notable changes to The Compositor Modules will be documented in this file.
## [0.2.0](https://github.com/winft/como/compare/v0.1.0...v0.2.0) (2024-06-19)


### ⚠ BREAKING CHANGES

* consumer creates screen locker for desktop platform

### Features

* add a placeholder message in the thumbnails grid switcher ([a7902ec](https://github.com/winft/como/commit/a7902ecb3ef0724b9de996d8ac89c4da200e0f5e))
* adds a border around hovered and selected desktop in desktopGrid ([ecdba0d](https://github.com/winft/como/commit/ecdba0d18e524ca76046bc4c5ccfa95592903172))
* drop "<N>" window caption suffix ([0478703](https://github.com/winft/como/commit/0478703e7f86e3febfe5e994e59edc1c20e81326))
* implement new overview layout algorithm ([c31e091](https://github.com/winft/como/commit/c31e0911f04e5d390ef9d1119907827ff322f7e1))
* **input:** remove the terminate server shortcut ([885d504](https://github.com/winft/como/commit/885d504bc4df2ef192e89afce5021c4e9eea6683))
* mark risky KNS content ([84b3a22](https://github.com/winft/como/commit/84b3a22e2d84a4f26ddc4603cfa895b1552fe595))
* **plugin:** provide a way to reserve a screen edge for grid mode ([bedfb80](https://github.com/winft/como/commit/bedfb80ee54d3e4293551e763f8a0afa412b7f15))
* **plugin:** remove middle click to close virtual desktop ([a89f065](https://github.com/winft/como/commit/a89f065a39d2e3cdc3b4f36e847ef0a488196706))
* provide KDE screen locker in desktop module ([4b46eb9](https://github.com/winft/como/commit/4b46eb94de55fe06372eb0920ae137526f805e2e))


### Bug Fixes

* adapt to wlroots pointer enum name change ([5f25519](https://github.com/winft/como/commit/5f255190d74d6b8d2ade74ed572e882b38c72892))
* add screen locked hook to windowing integration ([86df2ac](https://github.com/winft/como/commit/86df2ac3cfa1c08693443fef7a4e7a36b1dc4789))
* add some missing includes ([7586d3c](https://github.com/winft/como/commit/7586d3cd718e08e9610d4fd53ed3667bc602c175))
* assert not dividing by zero ([8c28b86](https://github.com/winft/como/commit/8c28b8677e608dd19da8e12790f827fe9422651b))
* clarify strings for inhibition ([4080777](https://github.com/winft/como/commit/408077738e9b52b0b35988e7422f3a904ad9e47d))
* define decoration spacer button ([3155ada](https://github.com/winft/como/commit/3155adace1ceeb4b34589236f196cad731544f20))
* discard return value ([c944f4f](https://github.com/winft/como/commit/c944f4f11cbe426cfaaa8e2262fd315e3c9a6930))
* do not take ownership of QuickEffect::delegate ([a3a7249](https://github.com/winft/como/commit/a3a72493d31d78f68211b8fd20254abe17efcd5f))
* don't build SPI support if Qt wasn't build with it ([70c9ee6](https://github.com/winft/como/commit/70c9ee66f7adba864162f3744de156836b2bdf8f))
* fix oversights on shortcut handling within Overview/Grid effect ([3f1ae13](https://github.com/winft/como/commit/3f1ae1339f1d9f6064b25b7a75e1ae79584a98da))
* **plugin:** fix autohidden panels blinking when plasmashell launches ([ae65a28](https://github.com/winft/como/commit/ae65a28571e03257046eb979d352a61e691d5d85))
* **plugin:** fix mainscript for declarative effects ([352a8a4](https://github.com/winft/como/commit/352a8a4766fc79209e011a3d08082a46b1a264c7))
* **plugin:** search bar can be clicked without closing effect ([9aa0d25](https://github.com/winft/como/commit/9aa0d25782adb34ab17fae192e8dcf9aef67fede))
* **plugin:** store expo layout without QPointer ([dc2d2c6](https://github.com/winft/como/commit/dc2d2c6ad7375c15da7a9bc9729fcf50940c12a4))
* **plugin:** store internal window handle without QPointer ([bc7cea9](https://github.com/winft/como/commit/bc7cea976a6110ad68945fb24ecf06cdd4ed4652))
* prevent including "show desktop" entry if there are no other windows ([156fc65](https://github.com/winft/como/commit/156fc65fd2ef331c1f69345b619b33f96cd9bd2e))
* store event filters as raw pointers ([ff291bd](https://github.com/winft/como/commit/ff291bd9099015a40c65f2455625ea6dd7d09d94))
* store implict grab without QPointer ([dc1443e](https://github.com/winft/como/commit/dc1443e1638beb8897d03ca32db40e54e599212e))
* store window thumbnail item fields without QPointers ([8d71539](https://github.com/winft/como/commit/8d715390eb6eb73ce467406416d5cc8b6e021aa1))
* use correct enum value for `PointerDevice` ([5ca52a4](https://github.com/winft/como/commit/5ca52a4f1c35238c0980b3e3ee0eac1bbd7cbbda))
* use new wayland enums ([b7a040a](https://github.com/winft/como/commit/b7a040a39d4785c25810ae0d1f90703078d787b7))
* use QKeyCombination::toCombined() ([81834c1](https://github.com/winft/como/commit/81834c12146016fcda69c6d4da305726c2252ce7))


### Refactors

* adapt to Wrapland subsurface change ([92e6581](https://github.com/winft/como/commit/92e65816fc652f173b6f477edff325c6e039d830))
* make NETRootInfo initialization reasonable ([5e5adfb](https://github.com/winft/como/commit/5e5adfb3a3ab7158518fe695c32d281a2ab16d15))
* optimize saving discarded rules to config ([1105f71](https://github.com/winft/como/commit/1105f717a27de307d83b570e37bffc915c1471de))
* remove Q_D macro ([24a43a9](https://github.com/winft/como/commit/24a43a90c694fac8e4f71d80eda7dfcf58e13b8d))
* remove screen locker init signal ([95cfefb](https://github.com/winft/como/commit/95cfefbbc6db5a38a259f074d7acacd226858961))
* replace Qt smart pointers with STL ones ([5828bc2](https://github.com/winft/como/commit/5828bc2231eee9c4084614b0c01a3bead8419ecb))
* round all the things consistently ([07b946f](https://github.com/winft/como/commit/07b946f00b4ac929670bb1f5511b7b4598c93557))
* slight code cleanup ([6470ed1](https://github.com/winft/como/commit/6470ed17a3896af7ff09c09722fd145d944c56d9))
* use smart pointer ([36fa216](https://github.com/winft/como/commit/36fa216d207d6d0518a4ea7cd9f81a9fa761ae33))
* **wl:** create server connections in free function ([504561f](https://github.com/winft/como/commit/504561f7eec7ff7107153520a2fd812b59d31aea))

## [0.1.0](https://github.com/winft/como/compare/cfa93fa9db90b2219f21940be69e323fd7f68355...v0.1.0) (2024-02-27)


### Features

* add script to drop old desktop switching shortcuts ([73a4058](https://github.com/winft/como/commit/73a4058684840d94a08604a05d7dafc26bbca65c))
* assign top-left screen corner to overview by default ([6aecb1d](https://github.com/winft/como/commit/6aecb1df71d91820adbe5f3a813b761ef15e76a1))
* change Shift+Backtab to Shift+Tab for tabbox ([b84f9ce](https://github.com/winft/como/commit/b84f9ce2cfe181737481c2416d0969a2f8d71406))
* change window highlight style in WindowHeapDelegate ([88aba22](https://github.com/winft/como/commit/88aba227f233e148bfb6870013a3d4043dfc713b))
* hide "active mouse screen" option ([4f04b27](https://github.com/winft/como/commit/4f04b271bf907c1bdaa9ecc047fa16e2776571f6))
* implement additional _NET_WM_MOVERESIZE arguments ([c40815e](https://github.com/winft/como/commit/c40815e307014cc2c87c9d135b477e027de76cea))
* make screen edge toggle overview rather than cycle between modes ([fa7fb2a](https://github.com/winft/como/commit/fa7fb2a25c7469cba323eb1027d84682992c1ddc))
* **plugin:** revoke Meta+Tab and Meta+Shift+Tab shortcuts for overview ([06372c7](https://github.com/winft/como/commit/06372c78d4e3c5a29e2ab9916b243a6fe18513cf))
* remove legacy virtual desktop number from the menu ([28671a3](https://github.com/winft/como/commit/28671a3abba034ffeb9098c3876bc66bd5087703))
* **wl:** expose method to allow closing windows on shutdown ([041f28a](https://github.com/winft/como/commit/041f28a00c650feab7dbdc5e14947b7f1dd2e86a))
* **wl:** implement closeable window rule ([5491eea](https://github.com/winft/como/commit/5491eea0352c25fabeccf5306b2ba8644ed85f73))
* **x11:** remove xRenderBlendPicture ([6c13651](https://github.com/winft/como/commit/6c136514a51b1d1e66cfa31a845c3085554f7edb))


### Bug Fixes

* activate on thumbnail click when selected ([9f34e5d](https://github.com/winft/como/commit/9f34e5d8515233fb0da5f7fbab7f69f0bfa2b36e))
* add cursor default shape fallback ([56607d3](https://github.com/winft/como/commit/56607d3a9074c121847e8c23e46c5f5abe6eb0ee))
* allocate an offscreen fbo with correct scale in OffscreenQuickView ([520046c](https://github.com/winft/como/commit/520046c6c440a400bb398fe46da94aca4c8a123b))
* allow switching between modes using shortcuts while already active ([6d8a6f6](https://github.com/winft/como/commit/6d8a6f6e45654c000c328900d0fdc269b0ca5993))
* always use GL_RGBA8 in offscreen quick view ([d723e7a](https://github.com/winft/como/commit/d723e7a54d83c8ee0a6b9bbdb29bc8415d7922e0))
* avoid double delete of QQuickViews ([beb7549](https://github.com/winft/como/commit/beb7549e7214941d85a03e57457c313e41a8e4f4))
* cast to int for comparison with zero ([5515703](https://github.com/winft/como/commit/551570353995feab884127bb511edf5e3935c11e))
* compare numbers without implicit casts ([2b12220](https://github.com/winft/como/commit/2b12220da639eb59f5d60a1653e4a63758f95daf))
* consider Qt::KeypadModifier relevant for global shortcuts ([2880376](https://github.com/winft/como/commit/2880376426b5c35f4904d8609abbde65e58195d1))
* do caps lock is not shift lock ([ca67d51](https://github.com/winft/como/commit/ca67d515c258726b6c708a1ae12d6a94b55adb17))
* do full tabbox reset on window release ([6165dc2](https://github.com/winft/como/commit/6165dc21f5f1e7b095540f92ad5101d4641e5f3d))
* don't scale WindowHeap in overview mode ([4509238](https://github.com/winft/como/commit/4509238d2e76dde07be8e0d5f8c79d886945d588))
* drop kwin-6.0-overview-activities-shortcuts script ([b334bfc](https://github.com/winft/como/commit/b334bfc8835017b61ff28577a0b3936a73683e65))
* export class ([c8197f5](https://github.com/winft/como/commit/c8197f504fb81dbfc1460568928869f7922d235a))
* fix "Drag down to close" label visibility ([30f45f7](https://github.com/winft/como/commit/30f45f729ecb1511895c4046e76bb9dcc841e091))
* fix a warning about incorrect anchor in overview ([6147192](https://github.com/winft/como/commit/6147192e94e9684e8e6a07d87905e5787a68fb61))
* fix glitches in mouseclick ([5056cdd](https://github.com/winft/como/commit/5056cddf0dc6e55ebd2a7db2d2d9f9242253c624))
* fix initialization of QEvent::isAccepted() in cloned events in OffscreenQuickView ([47d0e1c](https://github.com/winft/como/commit/47d0e1c132dea7349f8caa8f3b7b79a99322524a))
* fix sync'ing currentIndex ([4a44ce0](https://github.com/winft/como/commit/4a44ce0bfe2d4c86acea6237e9bacb114d94540b))
* fix zoom push mouse tracking on multi-monitor workspaces ([b712c51](https://github.com/winft/como/commit/b712c511eed421aeda06469c3c4df39ddfc6185f))
* have less concurrent animations ([6bb4143](https://github.com/winft/como/commit/6bb414320eb2fdb57dbb608b680f58e2e3124905))
* make sure window thumbnails and Qt Quick resources  are destroyed properly ([b7c9447](https://github.com/winft/como/commit/b7c9447308c19787c0987c233686fac6336d5b35))
* mark fallthrough ([6105627](https://github.com/winft/como/commit/61056278466fbf633e670f1e7043e02b01c50730))
* match Shift+Backtab against Shift+Tab ([efdc0ab](https://github.com/winft/como/commit/efdc0abe34e5dccad4a5268113240d8edc24b97a))
* only handled input events in on-screen desktops ([aa49b95](https://github.com/winft/como/commit/aa49b95763f60b9e4603a2802227a21c240cb24a))
* only show otherScreenThumbnail if we are actually dragging ([d625b4e](https://github.com/winft/como/commit/d625b4ed933a62a61cbbd03cfb450c63ce623930))
* overwrite the output in OffscreenQuickView::setGeometry() ([c900267](https://github.com/winft/como/commit/c90026710b2228d3f30ab69563e3b9db3152724f))
* **plugin:** always ref window when sliding it offscreen ([3533dd1](https://github.com/winft/como/commit/3533dd1b1d269918eabf0b748e9503e28bf8f769))
* **plugin:** avoid relaying text during overview animation ([072c809](https://github.com/winft/como/commit/072c809f030a839f522d2c2b69d26e43818314a9))
* **plugin:** cache screenshot attributes ([2bcc29e](https://github.com/winft/como/commit/2bcc29e16c813d6a0ba1a57649676f9384b24215))
* **plugin:** cancel animations when screen is locked/unlocked ([137310d](https://github.com/winft/como/commit/137310d32de3407248631c81152e79b004b45cb7))
* **plugin:** compare desktop number not pointer ([4d47a3f](https://github.com/winft/como/commit/4d47a3f8574b524686c5deaede764dd519e8f75b))
* **plugin:** disable acessibility integration on Wayland ([6348991](https://github.com/winft/como/commit/634899130aa0143fb8c96a216973dd1a1da82df1))
* **plugin:** explicitly reset parent on teradown ([503c4cc](https://github.com/winft/como/commit/503c4cc7a05e5df5a85ce72b0bd32a1822ba6629))
* **plugin:** fix previous desktop indicator in desktopchangeosd ([fe9cb05](https://github.com/winft/como/commit/fe9cb05fdd73117271ff23851a870d9c3c4d2cb9))
* **plugin:** handle platform destroyed ([38923c9](https://github.com/winft/como/commit/38923c9327ec9f1c02ee2f17f338f58835fbb832))
* **plugin:** hide "Drag Down to Close" when using a pointing device ([b9d8aae](https://github.com/winft/como/commit/b9d8aaebbcccda806fa2366c2a41f8a8bcd5df65))
* **plugin:** if window is set to "skip switcher", skip it from window heap ([60a791e](https://github.com/winft/como/commit/60a791ebd03991cf8b4fe59f088b9962eddcaabc))
* **plugin:** load milou on demand ([822ac90](https://github.com/winft/como/commit/822ac90df1baaf30c518b1062f9c00bbe10c1605))
* **plugin:** make transition between overview-grid modes longer ([faf4dca](https://github.com/winft/como/commit/faf4dca4fee93ea3cec67563bf398494836b262e))
* **plugin:** make window captions in Overview 2 lines at most ([82c5053](https://github.com/winft/como/commit/82c505331443cc1e47bc5135de97fdf2ae53d9ad))
* **plugin:** remove and create QPA screens uniquely ([3d34fe0](https://github.com/winft/como/commit/3d34fe09f0cd09e1b944b11a24ca49a2a717a9cf))
* **plugin:** retarget fullscreen animation instead of restarting it ([c858465](https://github.com/winft/como/commit/c85846563754ff4c3ee3a742069164938b93c519))
* **plugin:** sse SmoothPixmapTransform when stitching area screenshots ([204f094](https://github.com/winft/como/commit/204f0946769d47a6831d158e4f1c5c6ca759c611))
* **plugin:** use correct type to match ([8c89c52](https://github.com/winft/como/commit/8c89c523c3e41eb9ac7c6a750fd1323d6f26a79d))
* **plugin:** use different names for Qt properties ([a973672](https://github.com/winft/como/commit/a973672c767b3756e53fe53d376fb57bfb925ab2))
* **plugin:** use floating point offscreen texture ([239d2a4](https://github.com/winft/como/commit/239d2a435db6d8685b574868eac6acabfe5479d3))
* **plugin:** use InCubic easing ([650754a](https://github.com/winft/como/commit/650754adb4c4b6e877e34b350adb013152b9db74))
* **plugin:** use InOutCubic easing ([2fa9e1f](https://github.com/winft/como/commit/2fa9e1fedd461b131ed90d1a6712ccf8c3d612fc))
* register touch action to activate Overview instead of toggling it ([223d8b5](https://github.com/winft/como/commit/223d8b506bc7c40f3b4ead067f2d46740cdb332c))
* remove unneeded includes ([da3686c](https://github.com/winft/como/commit/da3686c7e9769699f3daf6e78ada75fe977d336f))
* remove unused lambda captures ([37cf7c8](https://github.com/winft/como/commit/37cf7c8b69d78536946625ce071cad3fabb9cab1))
* remove unused symbols ([69a4889](https://github.com/winft/como/commit/69a48896b4af39d627814e6649b6e012626ef3a1))
* replace QIcon::actualSize ([c99b432](https://github.com/winft/como/commit/c99b4329fb594bf6269facc789d6e89891cfb134))
* replace usage of QVariant::type ([188e4c2](https://github.com/winft/como/commit/188e4c2bb8c093a25e6dd9cef2055653659eedde))
* set componentDisplayName for shortcut migration ([9be29a0](https://github.com/winft/como/commit/9be29a099622fb72175170912125b8a50f0bc2ab))
* silence keyword-macro warning ([3539e47](https://github.com/winft/como/commit/3539e47f6d9ff5e916491114ce8317713947a8b7))
* store composited string as QString ([8d73bd2](https://github.com/winft/como/commit/8d73bd28c53e6644c8801c01e96641cbf3722e2b))
* update kconf_update version ([a85fc9b](https://github.com/winft/como/commit/a85fc9b392e01e7e5678686186c5961998618009))
* use FocusScope as main item of tabbox switcher ([cdfe19a](https://github.com/winft/como/commit/cdfe19a407afe26f360ef474ba17988b55a7aaf8))
* use new event position functions ([b8c0c6a](https://github.com/winft/como/commit/b8c0c6a9e8bc7ed5b8bf40caac4e79feef2ceec3))
* use nullptr ([34dbfab](https://github.com/winft/como/commit/34dbfab6b4953de63e60b524c9f3e698528bdfc0))
* use other QHoverEvent ctor ([4c19bd0](https://github.com/winft/como/commit/4c19bd07963303ded334fd0f5d2897da17265182))
* use other QMouseEvent ctor ([77fd20c](https://github.com/winft/como/commit/77fd20cbbfb15edf03b779b5d7a99e34cdd60da7))
* use QKeyCombination instead of int cast ([81f13a7](https://github.com/winft/como/commit/81f13a7db6f2f86f854a12b682c3773fe2837faf))
* use std::as_const instead of deprecated qAsConst ([510e8cb](https://github.com/winft/como/commit/510e8cba398be886781de919189c7adb9808efd3))
* use std::unique_ptr instead of QScopedPointer ([26025ca](https://github.com/winft/como/commit/26025ca7ba0147748d2cca33708a65a69e76714e))
* **wl:** dispatch mouse events to internal windows via QWindowSystemInterface ([c35ce86](https://github.com/winft/como/commit/c35ce86fafad003f8b109f83b5bb17b94a3ac3f3))
* **wl:** remove unique connection specifier ([e82f695](https://github.com/winft/como/commit/e82f6958123b2d5f67c0b0e060f7c99dd918e3f4))
* **x11:** fix MouseButtonPress events sent to decoration ([63f2f78](https://github.com/winft/como/commit/63f2f78a6f86326e81dc055165c4de3b05e24490))
* **x11:** order initializer list ([628544f](https://github.com/winft/como/commit/628544fb29de4bbd539beee10c24e3dcd89b7834))


### Refactors

* adapt color correct d-bus interface to Plasma ([2919fb1](https://github.com/winft/como/commit/2919fb127f37ce7c31b936e439eb42e60aade444))
* add session manager interface class ([ae4adc5](https://github.com/winft/como/commit/ae4adc50ae93de580980a705d2533af5bb6cf543))
* avoid QtDBus module include ([c3bf5b9](https://github.com/winft/como/commit/c3bf5b9714a71584eebeaf684d53acfc9ef48728))
* get wlroots backend with wl_event_loop ([8c2489b](https://github.com/winft/como/commit/8c2489b3414bb0f70802a921df555eb2881a386b))
* include always by full path ([010e347](https://github.com/winft/como/commit/010e3471a55661708917f444b4d6ecab3d4f48e1))
* **plugin:** don't rely on item type to determine drop behavior ([97b3ef9](https://github.com/winft/como/commit/97b3ef9b2848e51e7dc5c737966be410adf0c525))
* **plugin:** handle platform creation in separate function ([11669b5](https://github.com/winft/como/commit/11669b5a3c98ce02ed98707b73bdfcc44ea23223))
* **plugin:** replace OpacityMask with ShadowedTexture ([9598870](https://github.com/winft/como/commit/9598870fdcb2d43ad7715a7a910613805e7d260c))
* rename toplevel namespace ([c1dd120](https://github.com/winft/como/commit/c1dd120c368068be7e79558961f918dcc300988a))
* use input type alias from base ([7c5993e](https://github.com/winft/como/commit/7c5993e59090241099dda8e1ec867b0eaa76f15a))
* use STL pointers ([f25d981](https://github.com/winft/como/commit/f25d981d90332f6152c0d533009a489297a60152))
* use wlr_output_state API ([b108c58](https://github.com/winft/como/commit/b108c58e86133a4901d71a215ac16eeb47f84f80))
* **wl:** load plugins dynamically ([b348e38](https://github.com/winft/como/commit/b348e381840e7fa014c41bdcf893c5de8195b974))
* **x11:** remove unused key server functions ([83b2c9f](https://github.com/winft/como/commit/83b2c9f100d862df844d2838e26cb4e37e570236))
