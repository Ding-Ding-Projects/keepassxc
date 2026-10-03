# Browser, sharing and SSH choice controls

Feature id: `choice-control-promotion` · Category: Interface

## Behaviour and configuration

Four existing choice fields use `Material::ComboBox`, the application's styled
`QComboBox` subclass: browser type in browser settings, sharing type in group
settings, and key type plus key size in the SSH key generator. Their object names,
form properties, signal connections and controller logic are unchanged.

Browser settings retain the Firefox and Chromium item data and persisted selection.
Sharing retains its mode data and the controller's dependent enabled states. The
SSH generator retains Ed25519, RSA and ECDSA, their existing size lists, and the RSA
3072-bit and ECDSA 256-bit defaults. This promotion does not add editability or alter
validators, key generation, database access, or persistence formats.

## Verification and remaining acceptance

The affected compiled owners are the `browser`, `keeshare` and `sshagent` CMake
targets. Building them verifies that Qt's generated forms instantiate the promoted
classes and compile with the real controllers. Existing browser protocol and SSH
cryptography tests do not exercise these four choice fields, so they are not
evidence for this conversion.

The promotion establishes class adoption only. This combined source now includes
the separately reviewed searchable combo-box implementation; combined offscreen
verification must bind the frozen integration candidate. Native acceptance of the
four real controllers remains separate from those synthetic checks.
Native rendering, keyboard interaction, accessibility and the language, theme and
display-scale matrix remain pending. The hidden-desktop controller is installed
and has passed installation smoke with a synthetic Qt fixture. KeePassXC profile
and history isolation remains blocked pending the owner's scope decision. No
KeePassXC interaction or current screenshot is claimed from that fixture.

## Failure modes and security considerations

Each form declares the subclass and its `gui/material/MaterialControls.h` include.
A missing declaration or incompatible controller call is a build failure. The
promotion adds no network operation, credential collection or logging. Future
interaction checks must use isolated configuration and synthetic data; they must
not open personal databases or generate keys for an existing user identity.
