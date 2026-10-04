# Searchable combobox choices

## Scope and behavior

The 49 `Material::ComboBox` widgets now open a local searchable choice
surface through the public `QComboBox::showPopup()` and `hidePopup()` overrides.
The source model stays on the combo. A separately owned `QSortFilterProxyModel`
and `QListView` display its rows, respecting the combo's root index and model
column. No labels or data roles are copied into a replacement item model.

Selecting a result returns its original source row. Duplicate labels remain
distinct, disabled or unselectable rows cannot activate, and activating the
current item still emits the native activation signals. Filtering does not
change the current value. Escape or outside dismissal cancels the choice.

Editable controls retain their original line edit, completer, validator,
insertion policy and model ownership. The existing `MotionState` attachment is
unchanged. Model mutations refresh the view. Model replacement, root/column
changes, source destruction, owner hiding and disabling cancel the popup;
activation also validates the source synchronously. The nonvirtual base setter
APIs remain supported, with binding changes observed while the popup is open.

Dismissal listeners may synchronously delete the owner or popup, or change a row's
enabled/selectable flags. The popup clears its search and releases its signal
blocker during hiding, then emits `dismissed` only after Qt finishes processing
visibility. Tracked owner/popup pointers stop subsequent work after deletion.
Selection rechecks binding and both eligibility flags after dismissal, before any
value change or activation signal. A deleted popup can be recreated on reopening.

Dismissal and current-index listeners may also change the selected label or its
editable field. After those listeners finish, selection snapshots `currentText()`
before emitting `activated`, and uses that snapshot for the paired `textActivated`
signal. Both signals therefore describe the same selected text. Changes made by
an `activated` listener do not rewrite that already selected signal value; owner
deletion still prevents the subsequent text signal.

## Search and regular expressions

Plain, case-insensitive search is the default. The adjacent full regex builder
belongs to the same popup and starts with an empty sample. Applying it transfers
the pattern and flags back to this search only. The execution dialect is Qt's
`QRegularExpression` (PCRE2): `i`, `m` and `s` change matching; Unicode properties
are always enabled; `g` does not change the existence-of-a-match decision for a
choice. The field explains these semantics instead of implying JavaScript flags.

Choice filtering allows at most 512 pattern characters, 1,024 source rows and
2,048 characters per display label, with a 120 ms aggregate evaluation budget.
Regex compilation adds PCRE2 match and depth limits (10,000 and 128), and rejects
the shared high-risk pattern shapes. Invalid expressions, resource-limit errors
and oversized input clear the complete result set and show an inline status.
Partial results cannot activate after a limit error. Matching reads display
labels only, never hidden data roles or the editable field's current contents.

## Keyboard and accessibility

The popup initially focuses its local search. Up and Down move the candidate
through enabled results, Return activates its original index, and Escape
cancels. Standard focus traversal reaches the regex controls and result list.
Return inside the builder edits the pattern without activating a choice;
Escape closes the builder first and leaves the choice popup open.
The owner retains the combobox role and exposes expanded/collapsed state plus
an explicit relationship to the popup. The result view exposes Qt's list and
selection semantics. Accessible names and inline result/error descriptions use
the language and voice settings; theme changes update the popup and its rows.

## Privacy and persistence

Queries and choices are not written to configuration, databases, history or logs.
The local search is deliberately absent from the global builder router. Choice
labels are never supplied as regex samples. Dismissal clears the query and
detaches the proxy from the source; user-entered builder samples are cleared.
Only an explicit builder Copy action writes the pattern to the clipboard.

## Explicit exclusions

The integrated source includes the four separately reviewed promotions:
`BrowserSettingsWidget::browserTypeComboBox`, `OpenSSHKeyGenDialog::typeComboBox`,
`OpenSSHKeyGenDialog::bitsComboBox`, and `EditGroupWidgetKeeShare::typeComboBox`.
Their controller compatibility has compile evidence; each still needs native
interaction acceptance with this shared popup.
`WindowSelectComboBox`, completion popups, calendars, and the separate
`Material::Select` implementation are also outside this unit. The exact 45-widget
list plus the four promoted destinations is retained in `design/context-menu-inventory.md`.

## Verification and remaining evidence

`tests/TestMaterialComboBox.cpp` exercises model sharing and ownership, duplicate
labels and roles, source indices, nonzero roots and columns, source mutations,
replacement and destruction, editable contracts, disabled and same-item
activation, local regex behavior, cancellation, lifetime and accessibility.
The tests isolate configuration and set a fresh identity before `QApplication`.

The dismissal regression started at `6f7cbbb5865d5669a1c66772bc23bad94e438cf4`:
both eligibility rows failed because activation still occurred, and four separate
owner/popup deletion cases exited with access violations. A first repair fixed
eligibility but still crashed during Qt's hide-event unwinding. Moving dismissal
outside that unwinding passed all six new cases without changing their assertions.
At `62cb371658e2fedb1751547332db4a36c1a12c36`, the full combo suite reports
36 passes, zero failures, skips or blacklisted cases, exit 0. The executable SHA-256
is `1AC11C7D74871214A56A4758FEF4A765A8AF82C5BEDD93F2AF4FA51E1D2FAD8F`.
These bounded, isolated offscreen results establish the exercised callback paths;
they do not establish native interaction or rendering acceptance.

The activation-text regression at `82466dec0ccbd4dfc3d2a14733ff880dd8178c5c`
reports 2 passes and 6 failures, exit 6: editable and non-editable rows still emit
the old label after dismissal or current-index callbacks change it. The six rows
also cover a later activation callback changing the label again. The unchanged
regression at `80ab41c87ea3d29bed2a2a27589e5da04fb4ed94` reports 8 passes and
zero failures, exit 0; the complete combo suite reports 42 passes and zero
failures, skips or blacklisted cases, exit 0. All previous lifetime, eligibility
and binding assertions remain unchanged. The executable SHA-256 is
`09DC6A84B0F341F6A6DBB56568B99A408718D3321559901E7C34616583A3E1D1`.
These checks used fresh pre-application configuration and identity, the offscreen
Qt platform and a 120-second process bound. They provide no native rendering or
packaged interaction acceptance.

The tests-first source produced 2 passes and 16 failures because the searchable
popup did not exist. Implementation verification is recorded with the exact
candidate and executable hashes in the task receipts. Offscreen Qt checks are
not native packaged acceptance. No new KeePassXC native captures are available.
The hidden-desktop controller passed installation smoke with a synthetic Qt fixture,
but KeePassXC profile/history isolation remains blocked pending the owner's scope
decision. Controller installation evidence does not verify this candidate.
Keyboard and screen-reader operation, theme/language/scale geometry, every
production control and the packaged create/save/quit/reopen flow still require
genuine native interaction and capture evidence before product acceptance.

## Combined integration verification

At `23741dbb6bb6ed072483ce74afa5bcf5ec2536e9`, the shared combo suite reports 42 passes and the
synthetic auto-type selector suite reports 9 passes, both with zero failures, skips
or timeouts and exit 0. Focused shared-copy checks add 3 motion-catalogue passes
and 3 startup-setting localization/persistence passes. The source preserves the
reviewed runtime implementations and earlier causal red evidence. These isolated
offscreen checks do not establish native or packaged acceptance.
