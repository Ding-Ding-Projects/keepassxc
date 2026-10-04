# Searchable auto-type window choices

The entry editor's window-title association control uses the shared Material choice popup. It remains an editable `WindowSelectComboBox`: users can type a custom association, choose a current title, or retain the custom text stored in the first row. Typing does not insert another item into its model.

Opening the popup refreshes the available titles through the existing auto-type title provider. The first row retains the current editor text and its existing item data; refreshed title rows replace the previous title list. The public `refreshWindowList()` entry point remains available to the entry editor. A read-only editor does not open the popup or request another title list.

The popup provides local literal search and an adjacent full regex builder. Regex mode is opt-in and uses the shared Qt regular-expression contract, `i`, `m`, and `s` matching flags, inline validation, and bounded matching. Unicode properties are always enabled; `g` does not change whether a choice contains a match. Search filters a separate view without replacing the source model, editing the association, or changing the original row index used by activation. Escape cancels selection. Choosing a result updates the existing editor through the usual combo signals.

The control preserves its line editor, validator, completer, item data, and expanding-width/fixed-height size policy. Its width hints continue to come from the line editor, while height includes the Material field minimum. Motion and editable accessibility come from the shared combo implementation.

## Privacy and verification

Window titles can contain sensitive information. The popup keeps titles in the existing local model and never copies them into regex-builder sample text, persisted search state, or logs. Search syntax changes only popup filtering; it does not rewrite the stored auto-type association or change the auto-type matching engine. Opening the builder does not invoke auto-type.

`TestWindowSelectComboBox` uses a private per-instance title supplier with synthetic strings. The public constructor always supplies the existing production provider; there is no configuration, environment, or public setter that overrides it. Tests isolate configuration and application identity before constructing `QApplication`, run offscreen, and never instantiate the live auto-type platform or enumerate real window titles.

Focused regressions cover custom text and refresh, original-index activation with duplicate labels and distinct item data, read-only behavior, refresh on reopening, cancellation, regex-builder synchronization and empty samples, invalid and oversized patterns, insertion policy, sizing, editor accessibility, and popup teardown. Compilation includes the real `autotype` target and its containing entry editor through `keepassxc_gui`.

Native entry-editor interaction, assistive-technology behavior, and the language/theme/scale/minimum-size capture matrix remain unverified. Synthetic offscreen tests are not native acceptance or evidence of a shipped release.
## Combined integration verification

At `23741dbb6bb6ed072483ce74afa5bcf5ec2536e9`, the shared combo suite reports 42 passes and the
synthetic auto-type selector suite reports 9 passes, both with zero failures, skips
or timeouts and exit 0. Focused shared-copy checks add 3 motion-catalogue passes
and 3 startup-setting localization/persistence passes. The source preserves the
reviewed runtime implementations and earlier causal red evidence. These isolated
offscreen checks do not establish native or packaged acceptance.
