# Automatic browser credential access

Paired browsers receive eligible matching entries without a separate desktop
approval dialog. This applies to ordinary website forms and HTTP authentication.
Selecting an account in the extension remains an extension action. Pairing a new
browser, unlocking a database, updating an entry and passkey consent retain their
own workflows.

## Eligibility and persistence

The existing URL, scheme, port, group, pairing-key and best-match rules still
apply. Explicit site or form-host blocks take precedence over historical allow
records. Applicable configured HTTP realms must match. Hidden, recycled and
disallowed expired entries are excluded, and a locked database returns no entries.
Credential lookup never records an approval or changes saved entry permissions.

The obsolete ordinary-access and HTTP-authentication approval settings are removed.
Their stored values are discarded during configuration migration and cannot bring
the dialog back. Existing website block lists remain stored and editable.

## Invalid settings and recovery

Missing permission metadata uses automatic eligibility. Malformed JSON or invalid
permission-field types stop the response instead of silently granting access. A
non-blocking warning explains the entry-settings recovery route. Review and restore
deliberate site blocks before retrying. Diagnostics contain no credential values.

## Verification state

Focused native cases cover repeated ordinary/HTTP requests, unchanged entry
metadata, explicit site/form blocks, conflicting historical records, realms,
expiration, HTTP-only exclusions and malformed JSON. Source review does not prove
that the installed paired-browser transport, lock behavior, focus preservation or
notification rendering has passed. These runtime checks remain pending until
recorded against the built candidate.

## 廣東話

已配對瀏覽器會直接收到符合條件嘅項目，普通網頁表格同 HTTP 認證都唔再彈出
額外批准視窗。網站封鎖、網址配對、有效期限、群組限制同資料庫鎖定仍然有效。
查詢唔會自行儲存批准決定。設定資料損壞時會停止回傳，並用非阻擋提示交代
修復方法；重新嘗試前要檢查原有封鎖設定。實機瀏覽器驗證仍然待完成。

## Related articles

- [Verification profiles](verification-profiles.md)
- [Interface completeness](../delivery/interface-completion.md)
