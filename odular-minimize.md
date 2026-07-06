## 4. Optional: lazy loading

Only open the secondary document once the first page of the primary has
rendered, e.g. by hooking `DualDocumentController::loadLinked()` off
`Document`'s page-ready signal rather than calling it immediately after
`openDocument()` returns.

### 5.5 Dropping translations (English-only interface) without touching source

`i18n()` / `i18nc()` calls stay in the code as-is — ripping them out of every
call site across the tree would violate the minimize-changes goal, and isn't
necessary: KI18n's `i18n()` already falls back to the untranslated source
string (which is English) whenever no translation catalog is loaded. So the
practical way to get an English-only interface without editing any `.cpp`
file is to **stop building/installing the translation catalogs**, which is
two lines in the same overlay-gated block in the root `CMakeLists.txt`:

```cmake
if(NOT BUILD_ODULAR_OVERLAY)
    ki18n_install(po)
    if(KF6DocTools_FOUND)
        kdoctools_install(po)
    endif()
endif()
```

`find_package(KF6 ... COMPONENTS ... I18n ...)` still has to stay — the
`I18n` component is a required, non-optional entry in okular's own
`find_package(KF6 ...)` call, and `i18n()` itself is a compile-time macro
from that library, not something layered on top. Removing the component
would break the build; skipping catalog installation just means there's
nothing for it to translate *to* at runtime.

### 5.6 Script: strip unneeded `.po` catalogs

The `po/` directory ships ~1,069 `.po` files across 83 locale subdirectories.
If odular never installs them (5.5), they cost nothing at build time, but
if you also want them gone from the working tree/repo entirely:

```bash
#!/usr/bin/env bash
# odular/scripts/strip-translations.sh
# Deletes every po/<locale> directory, keeping only the source (English)
# strings baked into the .cpp/.ui/.rc files. Safe to re-run; it's a no-op
# once already stripped.
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
PO_DIR="${REPO_ROOT}/po"

# Locales to keep, if any (space-separated, e.g. KEEP_LOCALES="en_GB").
KEEP_LOCALES="${KEEP_LOCALES:-}"

if [[ ! -d "${PO_DIR}" ]]; then
    echo "No po/ directory found at ${PO_DIR}, nothing to do."
    exit 0
fi

shopt -s nullglob
removed=0
for locale_dir in "${PO_DIR}"/*/; do
    locale="$(basename "${locale_dir}")"
    keep=false
    for k in ${KEEP_LOCALES}; do
        [[ "${locale}" == "${k}" ]] && keep=true
    done
    if [[ "${keep}" == false ]]; then
        rm -rf -- "${locale_dir}"
        removed=$((removed + 1))
    fi
done

echo "Removed ${removed} locale director$( [[ ${removed} -eq 1 ]] && echo y || echo ies ) from po/."
[[ -n "${KEEP_LOCALES}" ]] && echo "Kept: ${KEEP_LOCALES}"
```

Usage:

```bash
chmod +x odular/scripts/strip-translations.sh
./odular/scripts/strip-translations.sh           # deletes all 83 locale dirs
KEEP_LOCALES="en_GB" ./odular/scripts/strip-translations.sh   # keep one
```

Since no locale directory contains "en" (US English is the untranslated
source string, not a `.po` file), the default run with no `KEEP_LOCALES`
already leaves the interface English-only — there's nothing to "keep,"
only locales to remove.
