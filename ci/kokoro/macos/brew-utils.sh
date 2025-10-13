#!/usr/bin/env bash
#
# Reusable utilities for interacting with Homebrew on macOS CI.

# This script is intended to be sourced, not executed.
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  exit 1
fi

# Load dependencies for logging functions
source module ci/lib/io.sh

# A wrapper for Homebrew commands that automatically falls back to the
# github.com mirror if the default ghcr.io mirror fails.
function brew_with_fallback() {
  # First, attempt the command with default settings and retries.
  if brew "$@"; then
    return 0
  else
    local exit_code=$?
    io::log_yellow "Initial brew command failed (exit code ${exit_code})."
    io::log_yellow "Retrying with the github.com fallback mirror."

    # Set the variable ONLY for the duration of this single brew command.
    # The parent shell's environment is NOT modified.
    HOMEBREW_BOTTLE_DOMAIN="https://github.com/Homebrew/homebrew-bottles/releases/download" \
      brew "$@"

    return $?
  fi
}
