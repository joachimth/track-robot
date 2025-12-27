# Release Workflow

## Overview

This project uses GitHub Actions to automatically build firmware and create releases. This ensures:
- Reproducible builds (same code always produces same binary)
- No local toolchain required for users (just flash pre-built binaries)
- Version tracking and changelog
- Web flasher integration (GitHub Pages serves latest firmware)

## Workflow Types

### 1. CI Build (Pull Requests & Main Branch)

**Trigger:** Push to any branch, or pull request

**Actions:**
- Checkout code
- Install ESP-IDF v5.3
- Build firmware for ESP32-C5
- Upload artifacts (binaries) for testing
- Run static analysis (future: linting, unit tests)

**Artifacts produced:**
- `track-robot-firmware.bin` (main app)
- `bootloader.bin` (ESP32 bootloader)
- `partition-table.bin` (partition layout)
- `merged-firmware.bin` (single file for flashing at 0x0)

**Retention:** 90 days

**File:** `.github/workflows/build.yml`

---

### 2. Release Build (Version Tags)

**Trigger:** Push a git tag matching `v*.*.*` (e.g., `v1.0.0`, `v2.1.3`)

**Actions:**
- All CI build steps (above)
- Create GitHub Release
- Attach firmware binaries to release
- Generate manifest.json for web flasher
- Update GitHub Pages with new release

**Artifacts produced (attached to release):**
- `track-robot-vX.Y.Z.bin` (versioned main firmware)
- `bootloader.bin`
- `partition-table.bin`
- `merged-firmware-vX.Y.Z.bin` (single file, all components)
- `manifest.json` (web flasher metadata)
- `release-notes.md` (changelog, auto-generated)

**Retention:** Permanent (until release deleted)

**File:** `.github/workflows/release.yml`

---

## Creating a Release (Step-by-Step)

### Manual Release Process

1. **Update version** in firmware code:
   ```bash
   # Edit firmware/main/main.c
   #define FIRMWARE_VERSION "1.2.3"
   ```

2. **Update CHANGELOG** (optional but recommended):
   ```bash
   echo "## v1.2.3 - $(date +%Y-%m-%d)" >> CHANGELOG.md
   echo "- Added slow mode toggle" >> CHANGELOG.md
   echo "- Fixed motor ramping bug" >> CHANGELOG.md
   ```

3. **Commit changes**:
   ```bash
   git add firmware/main/main.c CHANGELOG.md
   git commit -m "Bump version to v1.2.3"
   ```

4. **Create and push tag**:
   ```bash
   git tag v1.2.3
   git push origin main
   git push origin v1.2.3
   ```

5. **Wait for GitHub Actions** (~5-10 minutes):
   - Go to repository → Actions tab
   - Watch "Release Build" workflow
   - Wait for green checkmark

6. **Verify release**:
   - Go to repository → Releases
   - Confirm `v1.2.3` release exists
   - Download binaries and test (optional)

7. **Update web flasher** (automatic):
   - GitHub Pages site updates within ~2 minutes
   - Visit `https://<username>.github.io/<repo>/`
   - Confirm new version appears in dropdown

---

### Automated Release Process (Future Enhancement)

Use `workflow_dispatch` to trigger release from GitHub UI:

**Steps:**
1. Go to repository → Actions → "Create Release"
2. Click "Run workflow"
3. Enter version number (e.g., `1.2.3`)
4. Enter release notes (optional)
5. Click "Run"

**Workflow will:**
- Automatically bump version in code
- Create git tag
- Build and publish release
- Update GitHub Pages

*Not implemented by default - requires `workflow_dispatch` trigger in release.yml.*

---

## Release Artifacts Explained

### 1. `track-robot-vX.Y.Z.bin`
**Main application firmware**
- Contains compiled code for ESP32-C5
- Flash at offset `0x10000` (default app partition)
- Size: ~800KB - 1.5MB (depending on features enabled)

**When to use:**
- OTA updates (future)
- Flashing app only (keep existing bootloader/partitions)

---

### 2. `bootloader.bin`
**ESP32 first-stage bootloader**
- Runs on power-up, loads app
- Flash at offset `0x0`
- Size: ~30KB

**When to use:**
- Fresh ESP32 (never flashed before)
- Bootloader update needed (rare)

---

### 3. `partition-table.bin`
**Partition layout table**
- Defines flash memory regions (app, NVS, etc.)
- Flash at offset `0x8000` (default)
- Size: ~3KB

**When to use:**
- Fresh ESP32 (never flashed before)
- Changing partition sizes (requires full reflash)

---

### 4. `merged-firmware-vX.Y.Z.bin`
**All-in-one binary (bootloader + partitions + app)**
- Flash at offset `0x0` (includes everything)
- Size: ~1MB - 2MB
- **Recommended for most users**

**When to use:**
- Web flasher (easiest method)
- esptool.py single-command flash
- First-time setup

**Example esptool.py command:**
```bash
esptool.py --chip esp32c5 --port /dev/ttyUSB0 write_flash 0x0 merged-firmware-v1.0.0.bin
```

---

### 5. `manifest.json`
**Web flasher metadata**
- Describes firmware binaries and flash offsets
- Used by ESP Web Tools / Web Serial flasher
- Not needed for manual flashing

**Example structure:**
```json
{
  "name": "Track Robot Firmware",
  "version": "1.2.3",
  "builds": [
    {
      "chipFamily": "ESP32-C5",
      "parts": [
        {"path": "bootloader.bin", "offset": 0},
        {"path": "partition-table.bin", "offset": 32768},
        {"path": "track-robot-v1.2.3.bin", "offset": 65536}
      ]
    }
  ]
}
```

---

## Version Numbering Scheme

**Format:** `vMAJOR.MINOR.PATCH` (Semantic Versioning)

**Examples:**
- `v1.0.0` - Initial stable release
- `v1.1.0` - Added new feature (HTTP API improvements)
- `v1.1.1` - Bug fix (motor ramping issue)
- `v2.0.0` - Breaking change (new config format)

**When to bump:**
- **MAJOR** (v2.0.0): Breaking changes (old configs won't work)
- **MINOR** (v1.1.0): New features, backwards-compatible
- **PATCH** (v1.0.1): Bug fixes only

**Pre-release versions** (optional):
- `v1.0.0-beta.1` - Beta testing
- `v1.0.0-rc.1` - Release candidate

**Development builds:**
- Don't tag, just use commit hash: `v1.0.0-dev+abc1234`

---

## GitHub Pages Web Flasher Update

### How It Works

1. **Release workflow** generates `manifest.json` with latest firmware URLs
2. **GitHub Pages** serves static site from `web-flasher/` folder
3. **Web flasher** reads manifest and displays version dropdown
4. **User selects version** and flashes via Web Serial API

### Directory Structure

```
web-flasher/
├── index.html          # Main web flasher UI
├── style.css           # Styling
├── manifest.json       # Firmware metadata (auto-generated)
└── firmware/           # Firmware binaries (auto-uploaded)
    ├── v1.0.0/
    │   ├── merged-firmware-v1.0.0.bin
    │   └── ...
    └── v1.1.0/
        ├── merged-firmware-v1.1.0.bin
        └── ...
```

### Manual Update (if needed)

If automatic update fails, manually upload to GitHub Pages:

1. **Build firmware locally**:
   ```bash
   cd firmware
   idf.py build
   ```

2. **Copy binaries** to web-flasher:
   ```bash
   mkdir -p ../web-flasher/firmware/v1.2.3
   cp build/merged-firmware.bin ../web-flasher/firmware/v1.2.3/
   ```

3. **Update manifest.json**:
   ```json
   {
     "latest_version": "1.2.3",
     "versions": [
       {
         "version": "1.2.3",
         "date": "2025-12-27",
         "url": "firmware/v1.2.3/merged-firmware-v1.2.3.bin"
       }
     ]
   }
   ```

4. **Push to GitHub**:
   ```bash
   git add web-flasher/
   git commit -m "Add v1.2.3 to web flasher"
   git push origin main
   ```

5. **Enable GitHub Pages** (if first time):
   - Go to repository Settings → Pages
   - Source: `main` branch, `/web-flasher` folder
   - Save

---

## CI/CD Configuration

### Required Secrets

None! All workflows use public actions and ESP-IDF.

### Optional Secrets (for future enhancements)

| Secret Name | Purpose | Example Value |
|-------------|---------|---------------|
| `RELEASE_TOKEN` | Create releases with custom permissions | `ghp_xxxxxxxxxxxx` |
| `DISCORD_WEBHOOK` | Notify Discord on release | `https://discord.com/api/webhooks/...` |

**To add secrets:**
1. Go to repository Settings → Secrets and variables → Actions
2. Click "New repository secret"
3. Enter name and value
4. Save

---

## Workflow Files Breakdown

### `.github/workflows/build.yml`

```yaml
name: Build Firmware

on:
  push:
    branches: ['*']
  pull_request:

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.3
          target: esp32c5

      - name: Build firmware
        run: |
          cd firmware
          idf.py build

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: firmware-binaries
          path: firmware/build/*.bin
```

**Customization:**
- Change `esp_idf_version` to track newer ESP-IDF
- Add `idf.py test` step for unit tests
- Add linting steps (cppcheck, clang-tidy)

---

### `.github/workflows/release.yml`

```yaml
name: Release Firmware

on:
  push:
    tags:
      - 'v*.*.*'

jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.3
          target: esp32c5

      - name: Build firmware
        run: |
          cd firmware
          idf.py build
          cd build
          esptool.py --chip esp32c5 merge_bin \
            -o merged-firmware-${GITHUB_REF_NAME}.bin \
            --flash_mode dio --flash_freq 40m --flash_size 16MB \
            0x0 bootloader.bin \
            0x8000 partition-table.bin \
            0x10000 track-robot.bin

      - name: Generate manifest
        run: |
          cat > manifest.json <<EOF
          {
            "name": "Track Robot Firmware",
            "version": "${GITHUB_REF_NAME}",
            "builds": [{
              "chipFamily": "ESP32-C5",
              "parts": [
                {"path": "bootloader.bin", "offset": 0},
                {"path": "partition-table.bin", "offset": 32768},
                {"path": "track-robot-${GITHUB_REF_NAME}.bin", "offset": 65536}
              ]
            }]
          }
          EOF

      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            firmware/build/*.bin
            manifest.json
          generate_release_notes: true

      - name: Update GitHub Pages
        run: |
          # Copy binaries to web-flasher/firmware/
          mkdir -p web-flasher/firmware/${GITHUB_REF_NAME}
          cp firmware/build/*.bin web-flasher/firmware/${GITHUB_REF_NAME}/
          cp manifest.json web-flasher/
          git config user.name "GitHub Actions"
          git config user.email "actions@github.com"
          git add web-flasher/
          git commit -m "Release ${GITHUB_REF_NAME}"
          git push origin main
```

**Customization:**
- Add Slack/Discord notification step
- Generate changelog from commit messages
- Upload to external CDN (e.g., AWS S3)

---

## Troubleshooting

### Build Fails on GitHub Actions

**Error:** `fatal error: 'esp_ps4_ctrl.h' file not found`

**Cause:** Missing ESP-IDF component dependency

**Solution:** Add to `firmware/main/idf_component.yml`:
```yaml
dependencies:
  espressif/esp-ps4: "^1.0.0"
```

---

### Release Not Created

**Error:** `Tag v1.0.0 already exists`

**Cause:** Tag pushed but release workflow didn't run

**Solution:**
1. Delete tag locally and remotely:
   ```bash
   git tag -d v1.0.0
   git push origin :refs/tags/v1.0.0
   ```
2. Fix any workflow errors (check Actions tab)
3. Re-create tag and push

---

### Web Flasher Shows Old Version

**Cause:** GitHub Pages cache not updated

**Solution:**
1. Check `web-flasher/manifest.json` in GitHub (should show new version)
2. Hard refresh browser: Ctrl+Shift+R (Windows/Linux) or Cmd+Shift+R (Mac)
3. Wait 2-5 minutes for Pages to rebuild
4. Check Pages deployment status: Settings → Pages

---

## Best Practices

1. **Always test locally before tagging**:
   ```bash
   idf.py build flash monitor
   # Test all features
   ```

2. **Use descriptive commit messages**:
   ```bash
   git commit -m "Fix motor ramping overshoot in slow mode"
   # NOT: "fix bug"
   ```

3. **Keep releases small and focused**:
   - One feature per release (easier to debug)
   - Bundle multiple bug fixes together

4. **Document breaking changes clearly**:
   ```markdown
   ## v2.0.0 - BREAKING CHANGES
   - Config format changed: Rename `PWM_FREQ` to `MOTOR_PWM_FREQUENCY_HZ`
   - Migration: See docs/migration-v2.md
   ```

5. **Test web flasher after release**:
   - Flash to real hardware from GitHub Pages
   - Verify functionality before announcing

---

## Future Enhancements

### Automated Changelog Generation

Use [conventional commits](https://www.conventionalcommits.org/) and tools like `git-cliff`:

```bash
git cliff --tag v1.2.3 > CHANGELOG.md
```

### Pre-release / Beta Channels

Add separate workflows for beta releases:
- Tag format: `v1.0.0-beta.1`
- Mark as pre-release in GitHub
- Separate manifest for beta users

### OTA (Over-The-Air) Updates

Allow ESP32 to download and install updates from GitHub:
```c
// In main.c
esp_http_client_config_t config = {
    .url = "https://github.com/user/repo/releases/latest/download/firmware.bin",
};
esp_https_ota(&config);
esp_restart();
```

### Signing / Verification

Sign firmware binaries for security:
```bash
espsecure.py sign_data --keyfile private_key.pem firmware.bin
```

Verify on ESP32 before flashing (prevents malicious firmware).

---

## Next Steps

- [Architecture Overview](architecture.md) - Understand firmware structure
- [Wiring Guide](wiring-guide.md) - Hardware setup
- [HTTP API](http-api.md) - Test control endpoints
