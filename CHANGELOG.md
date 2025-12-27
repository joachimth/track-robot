# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial firmware implementation for ESP32-C5
- BTS7960 dual H-bridge motor driver support
- Differential drive mixer with deadzone and expo
- Safety features: emergency stop, failsafe timeout
- Control source arbitration (PS4 > HTTP > Serial)
- Serial control via UART (JSON and CLI modes)
- HTTP API control over Wi-Fi (AP and STA modes)
- Web-based control interface
- PS4 controller support (stub, requires BLE implementation)
- Motor ramping for smooth acceleration
- Configurable PWM frequency (20kHz default)
- Status LED patterns
- GitHub Actions CI/CD for automated builds
- GitHub Actions release workflow
- Web flasher (GitHub Pages) using ESP Web Tools
- Comprehensive documentation

### Changed
- N/A (initial release)

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- N/A

### Security
- N/A

## [1.0.0] - 2025-12-27

### Added
- Initial public release
- Complete firmware for ESP32-C5 tracked robot
- Multiple control interfaces (Serial, HTTP, PS4 framework)
- Safety and failsafe features
- Documentation and wiring guides
- Web flasher for easy firmware installation

---

## Version History Guidelines

When creating a new version, add an entry above with:
- Version number and release date
- Changes categorized into: Added, Changed, Deprecated, Removed, Fixed, Security
- Link to GitHub release

Example:
```markdown
## [1.1.0] - 2025-01-15

### Added
- PS4 controller full BLE implementation
- Battery voltage monitoring
- OTA update support

### Fixed
- Motor ramping overshoot in slow mode
- HTTP API CORS headers for specific origins
```
