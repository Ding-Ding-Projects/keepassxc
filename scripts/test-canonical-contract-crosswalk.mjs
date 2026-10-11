import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

// Independent required identities, deliberately not discovered from product code.
const required = `language funny-emoji personal-vocabulary school-mode narration scheduled-settings display-name adhd-modes dim-sum dim-sum-photo-source material-design workflow-navigation motion appearance-editor color-picker logo-customization overlay-panels collapse-filters accessibility-sizing functional-ui tabs regex-search command-palette context-menus menu-shortcuts rich-controls guided-forms settings-explanations blank-editors super-confirmation element-locks support-tickets unlock-ladder authenticator qr-pairing honest-monetization local-history exports bulk-actions changelog offline-docs rendered-provider-text notifications file-converter minecraft-server-manager ollama-suite external-editor download-handoff forge-publishing progress-recovery completeness-parity front-provenance product-evidence screen-recording design-reference-parity design-folder layout-matrix public-screenshot-gallery landing-site mobile-site installer-download-button feature-articles vendored-fonts shared-link-embed homepage-link vocabulary-unlock-boundary status-hub discord-status-bridge panic-webhooks tidbyt-displays build-entrypoints fresh-build-run-command dependency-fetcher bundled-dependencies squirrel-installer self-signing auto-updates app-icon release-workflow release-timing release-line-counts release-dim-sum-photo dim-sum-code-names ci-bootstrap runner-selection encrypted-public-builder tabbed-readme human-time-estimate sanitized-instruction-copy agents-md-vocabulary-block vocabulary-hash-lock roadmap-checklist feature-docs postman-collections wiki-website-sync handoff-record closeout-prompt discussion-records project-board operational-skill readme-prompt-banner shared-instruction-single-file-editions project-profile roblox-model-catalogue roblox-visual-realism`.split(' ');
const document = JSON.parse(readFileSync(new URL('../docs/features/canonical-contract-crosswalk.json', import.meta.url), 'utf8'));
function validate(value) {
    assert.equal(value.schemaVersion, 1);
    assert.equal(value.canonicalContractCount, required.length);
    assert.deepEqual(value.rows.map(row => row.canonicalId).sort(), [...required].sort());
    for (const row of value.rows) {
        assert(Array.isArray(row.scope) && row.scope.length);
        assert(row.scopeReason.length > 20);
        assert(Array.isArray(row.productFamilies));
        assert(['incomplete', 'not-applicable'].includes(row.evidenceState));
    }
}
validate(document);
for (const id of required) {
    const omitted = structuredClone(document);
    omitted.rows = omitted.rows.filter(row => row.canonicalId !== id);
    assert.throws(() => validate(omitted), `Omitted contract must fail: ${id}`);
}
const duplicate = structuredClone(document);
duplicate.rows.push(duplicate.rows[0]);
assert.throws(() => validate(duplicate));
console.log(`PASS canonical crosswalk: ${required.length} explicit identities, ${required.length} omission negatives, duplicate rejection. Product acceptance remains incomplete.`);
