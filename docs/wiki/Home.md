# KeePassXC Material

A Windows-only fork of [KeePassXC](https://keepassxc.org) whose interface is being rebuilt in Material Design 3. The project remains unfinished. For the official supported password manager, use [keepassxreboot/keepassxc](https://github.com/keepassxreboot/keepassxc).

## Current verification

The October database lifecycle repair is tracked in [issue #17](https://github.com/Ding-Ding-Projects/keepassxc/issues/17) and [discussion #18](https://github.com/Ding-Ding-Projects/keepassxc/discussions/18). Focused keyless-opening and Save As regressions have passed after their corresponding failures were reproduced. Packaged native creation, complete exit, relaunch and exact readback remain unverified. The [lifecycle record](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/delivery/database-lifecycle-repair.md) separates source, test and native acceptance evidence.

The reviewed menu, shared-motion, vault-sizing and evidence-inventory units are integrated on main at [`8f366c10997a4d0a88ce295ba79e4d22e8cde882`](https://github.com/Ding-Ding-Projects/keepassxc/commit/8f366c10997a4d0a88ce295ba79e4d22e8cde882). The combined offscreen Qt suites recorded **90 passing entries, zero failures, zero skips and no timeout**, with separate red-before-green evidence. Menus receive local search and an inline regular-expression builder while preserving action ownership and callbacks. Shared motion honors reduced-motion preferences, and hidden vault panes restore remembered sizes when first shown. Native rendering, keyboard grabs and the full display/accessibility matrix remain unverified; non-QMenu adapters and remaining universal features are unfinished.

The delivered source repository `main` is
[`f99c814aa8e779cd6b6eca7502c2cbca8b928d3b`](https://github.com/Ding-Ding-Projects/keepassxc/commit/f99c814aa8e779cd6b6eca7502c2cbca8b928d3b).
It includes the startup/motion integration, four promoted choice controls and the
navigation/favicon repair at `1f3ca0526e3657be5308da2763287913d3fd6ac6`, followed
by factual delivery records. The four promoted fields are browser type, group sharing
type, SSH key type and SSH key size. Their three production owners compiled at
`3cb79c5ce840e494b00e6cbcd22cc2fa1d14759f`, and bounded independent review found
no actionable regression. The startup/motion result remains source-bound to
`7cb43f654ef2cc908a76a5a739bddfda2aa29c52`: 167 offscreen QTest pass rows
(127 startup, 7 legacy dim-sum, 33 motion), with no failures, skips or timeout.
Those results do not establish native rendering or packaged lifecycle acceptance.

The navigation source retains **40 immutable articles**. The earlier **94 inventory
checks**, **3 category checks** and exact-source documentation build remain
recorded evidence. [Documentation workflow 37165266322](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/37165266322)
and [main navigation package workflow 37165266179](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/37165266179)
completed successfully at `1f3ca0526e3657be5308da2763287913d3fd6ac6`.
The distinct preservation package run [37164478960](https://github.com/Ding-Ding-Projects/keepassxc/actions/runs/37164478960)
also succeeded and published non-draft [v2.8.32001](https://github.com/Ding-Ding-Projects/keepassxc/releases/tag/v2.8.32001).
All seven downloaded assets for that release matched the published receipts and
source provenance. The local verdict records `packageVerified: true`, expected
`signingStatus: NotSigned`, and **`nativeRuntimeVerified: false`**. The installer
SHA-256 is `257eba22c0c798bf5bb1bafe2340834f08ea456b85346f2d86ad46c83e778757`;
the packaged executable SHA-256 is
`5562a09c3f52435e9fcd4ab3e678df0ba31de36d06e0b797f53c4576117c66b1`.
Downloaded-byte verification is not installed execution acceptance.

The genuine navigation evidence contains **198 original screenshots across 48
tuples**, with **960 contained labels**, no label escape and no body-overflow
state. Scoped visual and interaction checks passed: native background Right/Enter
focus and activation, automatic focus scrolling, DOM overflow selection, and a
fresh favicon HTTP 200 response. Matrix and overflow interactions used DOM clicks;
keyboard input used the hidden-desktop controller. Browser device-scale emulation
is not operating-system display scaling. Representative pixels were inspected;
all images were checked for hash, dimensions and PNG integrity.

The aggregate capture audit remains **failed/incomplete**, exit 1, because exact
owned browser-profile deletion was rejected by automatic approval review. Browser
and server processes, ports and the named desktop were released; retained profiles
remain untouched, with no alternate deletion route or retry. The first invalid
attempt remains invalid. These images have not been promoted into repository
evidence or published, and no KeePassXC native acceptance follows from them.

The separate combined-choice source
`568bb411e571a9b7e08bd0e82e0ea9208ea76267` is preserved on
`codex/choice-final-20261004`. Independent source review is bounded dry.
Its unchanged runtime `23741dbb6bb6ed072483ce74afa5bcf5ec2536e9` passed **57 QTest
rows: 49 behavioral rows plus eight initialization/teardown rows**. The totals are
42 combo, 9 auto-type, 3 motion-catalogue and 3 startup-settings rows; the last two
are selected checks, not complete suites. All processes exited 0 without skips
or timeouts. Exactly one of its 42 immutable article bindings remains stale.
The final manifest action still awaits explicit approval after automatic review
rejected it. The dry review does not release that hold or establish final
bound-documentation, native or packaged acceptance. This candidate is not claimed
integrated into the source main above.

Native compilation, focused update/title-bar/tab/GUI checks, and local unsigned Squirrel package-byte verification have passed at the exact candidates in the [repair verification record](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/delivery/repair-verification-2026-09.md). The updated [website](https://ding-ding-projects.github.io/keepassxc/) is published and displays release-derived download metadata.

Actual drag gestures, an older installed version updating to the new version, and full per-surface feature acceptance remain incomplete. The older inventory's **1 of 172 summary rows** is historical source-level accounting. The current inventory explicitly defines **106 surfaces, 89 feature groups and 998 capabilities**, yielding **9,434 surface/feature groups and 105,788 evidence cells**. The product checker remains incomplete with **0 current cells verified**. These counts describe evidence obligations, not distinct defects or proof that implementation is absent everywhere.

The [finite native smoke and capture sequence](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/delivery/interface-completion.md) requires isolated startup, ten synthetic entries across two groups, atomic save, actual Quit and proven exit, a distinct relaunch, exact local field comparison, and positional reopening through spaces and Cantonese characters. It also covers the changed menus, motion, vault sizing and startup exclusions. Genuine per-click screenshots must bind to the source and executable and pass privacy review before publication in chat, the repository, README and website.

The user-authorized Lowlevel installation is complete at source
`e6e42f2066d539256d6480401d7cef867f2b8dfe`. Its quiet client-owned persistent stdio
route has verified client registration, neutral-working-directory initialization,
58 observed tools and the native C++ backend. Synthetic Qt smoke proved hidden
launch, genuine capture, background close and owned teardown, with unchanged
foreground, cursor and input desktop. This is installation smoke only, and the
synthetic image is not KeePassXC evidence.

Native KeePassXC profile/history acceptance remains pending the owner's scope
decision. Temporary INI filenames do not isolate the independent
`QStandardPaths::AppDataLocation` history root. No personal database was opened,
and no packaged KeePassXC lifecycle has run. Historical September images lack
executable provenance and cannot satisfy current packaged acceptance.

**廣東話摘要：** 導航來源同交付記錄已經合併到 `main`，文件流程同指定導航封裝流程已成功。
`v2.8.32001` 七個下載檔案已驗證位元同來源，預期未簽署，原生執行仍然未驗證。
導航保留 198 張原始截圖、48 個組合及 960 個完整標籤；局部檢查通過，但設定檔刪除被拒絕，
整體驗收仍未完成，保留檔案冇重試刪除。控制器安裝測試唔等於 KeePassXC 驗收。
獨立選項來源覆核冇新問題，57 個 QTest 通過列包括 49 個行為列同 8 個初始化／收尾列，
其中一個文章綁定仍然過時，批准限制未解除。完整產品仍然係 0/105,788 個證據格完成，
9,434 個表面／功能組合未完成；原生隔離範圍仍等候決定。

| Page | Contents |
| --- | --- |
| [Building](./Building.md) | Supported native build and unsigned packaging route |
| [Material Design](./Material-Design.md) | Earlier interface design documentation, subject to current inventory limitations |
| [Passkeys](./Passkeys.md) | Earlier passkey registration and saving documentation |
| [Vault category search](./Vault-Category-Search.md) | Metadata-only entry search, folders, tabs, selected tags and detail labels; current native acceptance is pending |
| [Current verification](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/delivery/repair-verification-2026-09.md) | Exact checks, package hashes and remaining acceptance |
| [Native menu search](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/search/context-menu-search.md) | Local menu filtering, inline regex builder, evaluation bounds and remaining native acceptance |
| [Complete interface acceptance](https://github.com/Ding-Ding-Projects/keepassxc/blob/main/docs/features/delivery/interface-completion.md) | Explicit capability inventory, evidence requirements and finite packaged smoke sequence |

The current lifecycle and complete-interface discussion is [#18](https://github.com/Ding-Ding-Projects/keepassxc/discussions/18), with complete feature delivery in [#12](https://github.com/Ding-Ding-Projects/keepassxc/issues/12). Earlier repair evidence remains in [#16](https://github.com/Ding-Ding-Projects/keepassxc/discussions/16) and compilation history in [#15](https://github.com/Ding-Ding-Projects/keepassxc/issues/15).
