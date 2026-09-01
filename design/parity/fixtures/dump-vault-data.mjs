// Emit the design's demo vault as JSON so build-fixture.ps1 can create the
// deterministic parity database from the same data the reference renders.
// The design file is data; nothing in it is a real credential.
import { DATABASES, GROUPS, ENTRIES, FIELDS } from '../../lib/vault-data.js';

process.stdout.write(JSON.stringify({ DATABASES, GROUPS, ENTRIES, FIELDS }, null, 2));
