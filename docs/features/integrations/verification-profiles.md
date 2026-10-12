# Isolated verification profiles

The explicit `--verification-profile <absolute-directory>` option provides
process-local state routing for synthetic verification. Normal launches keep their
existing configuration and installed identity.

## Storage and process identity

Use a new or empty directory, or an existing directory carrying the exact profile
marker. Filesystem roots, relative paths, duplicate options and symbolic/reparse
ancestors are rejected. The profile owns separate configuration, data, cache,
runtime and temporary directories, plus its own application and browser IPC names.
Configuration environment overrides and portable configuration files cannot select
ordinary user state while the profile is active.

The profile routes history and application-logo storage, suppresses global browser
registration and extension-installation changes, disables update/download/restart
actions, and does not restore personal recent databases. Database and keyfile
operations are confined to the profile. Copy only synthetic fixtures into it.

This is state isolation, not an operating-system filesystem sandbox. It does not
grant permission to inspect personal vaults, run arbitrary external tools or adopt
unrelated services. An existing verification profile may contain retained synthetic
state and must not be treated as disposable without ownership proof.

## Verification state

Helper and boundary regression tests accompany the implementation. Native build,
proxy transport, actual configuration readback, GUI save/import boundaries and
complete hidden-desktop lifecycle acceptance remain pending. Do not infer a passed
runtime result from a successful source review.

## 廣東話

驗證模式用獨立資料夾儲存設定、快取、歷史同程序通訊身份，只供合成測試資料。
普通啟動嘅資料位置保持不變。呢個功能唔係作業系統沙箱，亦唔授權讀取私人密碼庫。
實際建置、瀏覽器通訊同完整操作流程仍然要逐項驗證。

## Related articles

- [Browser credential access](browser-access.md)
- [Interface completeness](../delivery/interface-completion.md)
