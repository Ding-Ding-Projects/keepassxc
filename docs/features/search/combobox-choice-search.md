# Searchable combobox choices

## Scope and behavior

The 45 existing `Material::ComboBox` widgets now open a local searchable choice
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

This unit does not change the four plain `QComboBox` widgets:
`BrowserSettingsWidget::browserTypeComboBox`, `OpenSSHKeyGenDialog::typeComboBox`,
`OpenSSHKeyGenDialog::bitsComboBox`, and `EditGroupWidgetKeeShare::typeComboBox`.
`WindowSelectComboBox`, completion popups, calendars, and the separate
`Material::Select` implementation are also outside this unit. The exact 45-widget
list is retained in `design/context-menu-inventory.md`.

## Verification and remaining evidence

`tests/TestMaterialComboBox.cpp` exercises model sharing and ownership, duplicate
labels and roles, source indices, nonzero roots and columns, source mutations,
replacement and destruction, editable contracts, disabled and same-item
activation, local regex behavior, cancellation, lifetime and accessibility.
The tests isolate configuration and set a fresh identity before `QApplication`.

The tests-first source produced 2 passes and 16 failures because the searchable
popup did not exist. Implementation verification is recorded with the exact
candidate and executable hashes in the task receipts. Offscreen Qt checks are
not native packaged acceptance. No new native captures are available: the
required hidden-desktop service connection previously exhausted its bounded
attempts, and service recovery has not yet produced evidence for this candidate.
Keyboard and screen-reader operation, theme/language/scale geometry, every
production control and the packaged create/save/quit/reopen flow still require
genuine native interaction and capture evidence before product acceptance.
