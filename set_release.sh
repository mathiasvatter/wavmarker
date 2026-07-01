#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="cmake-build-release"
BIN_NAME="wavmarker"
BIN_PATH="${BUILD_DIR}/${BIN_NAME}"
ASSETS_DIR="${ASSETS_DIR:-}"
RELEASE_TAG="${RELEASE_TAG:-}"
RELEASE_VERSION="${RELEASE_VERSION:-}"

if ! command -v gh >/dev/null 2>&1; then
  echo "Error: gh (GitHub CLI) not found in PATH." >&2
  exit 1
fi

REPO_REMOTE="$(git remote get-url origin)"
REPO="$(echo "$REPO_REMOTE" | sed -E 's#(git@github.com:|https://github.com/)##; s#\.git$##')"
if [ -z "$REPO" ]; then
  echo "Error: Could not determine GitHub repo from origin remote." >&2
  exit 1
fi

TAG=""
if [ -n "$RELEASE_TAG" ]; then
  TAG="$RELEASE_TAG"
elif [ -n "$RELEASE_VERSION" ]; then
  TAG="v${RELEASE_VERSION}"
elif [ -f "CMakeLists.txt" ]; then
  VERSION_FROM_CMAKE="$(sed -nE 's/^project\([^)]*VERSION[[:space:]]+([^[:space:])]+).*/\1/p' CMakeLists.txt | head -n1)"
  if [ -n "$VERSION_FROM_CMAKE" ]; then
    TAG="v${VERSION_FROM_CMAKE}"
  fi
elif GIT_TAG=$(git describe --tags --exact-match 2>/dev/null); then
  TAG="$GIT_TAG"
else
  if [ ! -f "$BIN_PATH" ]; then
    echo "Error: no tag/version provided and ${BIN_PATH} not found for version detection." >&2
    echo "Provide RELEASE_TAG/RELEASE_VERSION or build ${BIN_NAME} first." >&2
    exit 1
  fi
  VERSION="$($BIN_PATH --version | head -n1 | tr -d '\r' | awk '{print $NF}')"
  if [ -z "$VERSION" ]; then
    echo "Error: Could not determine version from ${BIN_PATH} --version" >&2
    exit 1
  fi
  TAG="v${VERSION}"
fi

if [[ "$TAG" != v* ]]; then
  TAG="v${TAG}"
fi

RELEASE_NAME="$TAG"
PRE_RELEASE_FLAG=""
if [[ "$TAG" == *"-"* ]]; then
  PRE_RELEASE_FLAG="--prerelease"
fi

declare -a ASSETS=()
if [ -n "$ASSETS_DIR" ]; then
  if [ ! -d "$ASSETS_DIR" ]; then
    echo "Error: ASSETS_DIR '$ASSETS_DIR' does not exist." >&2
    exit 1
  fi

  MACOS_ARM_SRC="$ASSETS_DIR/${BIN_NAME}-macos-arm64/${BIN_NAME}"
  MACOS_X64_SRC="$ASSETS_DIR/${BIN_NAME}-macos-x86_64/${BIN_NAME}"
  WINDOWS_SRC="$ASSETS_DIR/${BIN_NAME}-windows-x86_64/${BIN_NAME}.exe"

  if [ -f "$MACOS_ARM_SRC" ] && [ -f "$MACOS_X64_SRC" ] && [ -f "$WINDOWS_SRC" ]; then
    TMP_DIR="$(mktemp -d)"
    BUNDLE_DIR_NAME="${BIN_NAME}_${TAG}"
    BUNDLE_ROOT="$TMP_DIR/$BUNDLE_DIR_NAME"

    mkdir -p "$BUNDLE_ROOT/macos_arm64" "$BUNDLE_ROOT/macos_x86_64" "$BUNDLE_ROOT/windows"
    cp "$MACOS_ARM_SRC" "$BUNDLE_ROOT/macos_arm64/${BIN_NAME}"
    cp "$MACOS_X64_SRC" "$BUNDLE_ROOT/macos_x86_64/${BIN_NAME}"
    cp "$WINDOWS_SRC" "$BUNDLE_ROOT/windows/${BIN_NAME}.exe"
    chmod +x "$BUNDLE_ROOT/macos_arm64/${BIN_NAME}" "$BUNDLE_ROOT/macos_x86_64/${BIN_NAME}"

    BUNDLE_ZIP="$PWD/${BUNDLE_DIR_NAME}.zip"
    (cd "$TMP_DIR" && zip -qry "$BUNDLE_ZIP" "$BUNDLE_DIR_NAME")

    ASSETS+=("$BUNDLE_ZIP")
  else
    while IFS= read -r -d '' file; do
      ASSETS+=("$file")
    done < <(find "$ASSETS_DIR" -type f -print0 | sort -z)
  fi
else
  if [ ! -f "$BIN_PATH" ]; then
    echo "Error: ${BIN_NAME} executable not found in ${BUILD_DIR}." >&2
    echo "Build first (or set ASSETS_DIR)." >&2
    exit 1
  fi
  ASSETS+=("$BIN_PATH")
fi

if [ "${#ASSETS[@]}" -eq 0 ]; then
  echo "Error: No assets found to upload." >&2
  exit 1
fi

NOTES_FILE="CHANGELOG.md"
if [ -f "$NOTES_FILE" ] && [ -s "$NOTES_FILE" ]; then
  NOTES="$(cat "$NOTES_FILE")"
else
  NOTES="Release ${TAG}"
fi

if gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; then
  echo "Release '$TAG' already exists on GitHub. Deleting it..."
  gh release delete "$TAG" --repo "$REPO" --yes
fi

if [ -n "$PRE_RELEASE_FLAG" ]; then
  gh release create "$TAG" \
    --repo "$REPO" \
    --title "$RELEASE_NAME" \
    --notes "$NOTES" \
    "${ASSETS[@]}" \
    --prerelease
else
  gh release create "$TAG" \
    --repo "$REPO" \
    --title "$RELEASE_NAME" \
    --notes "$NOTES" \
    "${ASSETS[@]}"
fi

echo "Release created: ${TAG}"
echo "Uploaded assets:"
printf ' - %s\n' "${ASSETS[@]}"
