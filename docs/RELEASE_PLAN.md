# First public development release plan

Candidate version: **0.9.0-dev**, GitHub **Pre-release**, never Stable 1.0.
Status: **deferred during automatic-upload infrastructure work**. Minimum
acceptance still needs CDC RX/reconnect and IDE tests. UART and ADC physical
validation are not release blockers.
See the current [acceptance report](HARDWARE_ACCEPTANCE.md) and
[draft release notes](RELEASE_NOTES.md).

## Consolidate the cumulative pull requests

PRs #1 through #9 describe successive cumulative stages against main. Stage 8.5
continues from Stage 8 commit `4fcd665563449ac7ca91b45a105172a193e6c7e5`.
Use one integration of the final reviewed cumulative head, preserving its
existing commit ancestry. Do not squash or cherry-pick each cumulative PR:
that would duplicate earlier stage changes or obscure the shared history.

1. Re-read current main, all stage PR heads and Stage 8.5's final head. Verify
   main is an ancestor of the release head and account for any independent work.
2. Review Stage 8.5 against the Stage 8 base as well as the cumulative diff
   against main. Record all software results and the final hardware evidence.
3. Once the minimum gate passes, integrate the final cumulative PR once,
   preserving commits with a merge commit (or a controlled fast-forward if
   repository policy permits). Revalidate the resulting main tree.
4. Close any older open PRs already fully covered, with links to that integration;
   do not merge them separately. Check actual GitHub state before each action.
5. Tag only the reviewed tested main commit and publish the source artifact
   and checksums as a Pre-release after the audit below.

This document prepares the strategy; it does not itself merge, close PRs,
change repository visibility, create a tag or publish a release.

## Public-readiness audit and artifact

- README must distinguish observed GPIO/DFU/timing/CDC results from software
  tests. UART and ADC remain prominently HARDWARE NOT TESTED.
- Keep MIT LICENSE and third-party notices/provenance. Compiler, device headers,
  dfu-programmer and libusb binaries remain external dependencies.
- Inspect source and reachable Git history for credentials, personal machine
  paths, local USB identities, proprietary binaries and temporary acceptance logs.
  A clean source allowlist alone does not sanitize repository history. If sensitive
  history is found, resolve it before changing repository visibility.
- Run native, DFU, installation/packaging checks and all 14 Arduino plus three
  bare-metal builds against the final candidate. Test the standard 12 MHz/PA23
  profile and preserve the protected application origin 0x80002000.
- Only after the hardware gate passes, create the final archive from the clean
  reviewed source commit. The allowlist excludes build/, .scratch/, local
  configuration and temporary hardware logs. Verify archive checksums and
  repeat installation from that exact extracted artifact in a fresh sketchbook.
- Confirm a second package from unchanged inputs is byte-identical under the
  same runtime. Publish the exact validated bytes and SHA256SUMS, with a
  verified public download URL and an explicit experimental warning.

Boards Manager distribution requires separate portable platform/tool packages,
license review and a tested index; the source archive does not claim that support.
Next planned feature stage after this development release is Stage 9, SPI.
