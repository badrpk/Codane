#!/bin/sh
set -eu
exec "$(dirname "$0")/../build/codane" run "${1:-$(dirname "$0")/../examples/simple.json}" --provider codex
