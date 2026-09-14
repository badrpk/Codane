#!/bin/sh
set -eu
test -x "$(dirname "$0")/../build/codane"
"$(dirname "$0")/../build/codane" validate "$(dirname "$0")/../examples/simple.json"
"$(dirname "$0")/../build/codane" run "$(dirname "$0")/../examples/simple.json" --provider fake >/dev/null
"$(dirname "$0")/../build/codane" version
