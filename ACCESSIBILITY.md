# Accessibility — Eden Encore

Eden Encore is designed to be used with a controller on a television. Accessibility is part of the native launcher, not a claim that every emulated Nintendo Switch game is accessible or that the PS5 system's accessibility services automatically work inside Encore.

This page describes **features present in the current source**, how to use them, and limitations that still need verification. It is not a WCAG certification or a guarantee of successful use with any specific assistive technology.

## Quick start

On the launcher, open **Settings → Accessibility**. Three independent options are available; all are **Off by default**:

| Setting | What it does | Important limitation |
| --- | --- | --- |
| **Larger text** | Enlarges smaller **launcher** labels and body text, with a bounded scale intended for TV readability. | Not a global font-scale control, and does not enlarge in-game text or every artwork label. Long translations may still truncate. |
| **High contrast** | Uses higher-contrast launcher text and stronger, more clearly outlined focus and panel styling. | Contrast ratios and every theme/state have not been comprehensively audited on physical displays. |
| **Reduce motion** | Disables the launcher's decorative drift, zoom and slide movement where those effects use the shared motion setting. | Does not change guest-game animation, refresh rate, loading behavior, gameplay motion or every possible transition. |

Changes are launcher preferences, not per-game accessibility settings. The options can also be located through the launcher's TV-oriented Settings navigation. See [SETTINGS.md](SETTINGS.md#accessibility) for the full settings guide.

## Controller navigation and input

- The native launcher is designed around a **DualSense controller**, with visible focus on actionable items and button prompts for the current navigation context.
- **L1/R1** provide main-tab navigation where supported. Confirm, Back and directional navigation are context-dependent; follow the onscreen prompts rather than assuming every game uses the same semantics.
- Go to **Settings → Controls → Button mapping** to customize the emulated Switch face buttons, shoulder buttons and other mapped actions. Encore provides PlayStation, Switch, Custom PS5 and Custom Switch layouts; a title can have its own mapping.
- Mapping a game's emulated controls is **not** the same as remapping the launcher's entire interface or the PS5 operating system. Some games display Nintendo button graphics that Encore cannot universally replace.
- Controller mapping and saved-profile behavior during gameplay still require title-by-title hardware validation. Do not infer reliable in-game accessibility from a working launcher mapping screen.

For mapping details, see [SETTINGS.md](SETTINGS.md#controls).

## Language and readable information

- The launcher has translated catalogs and font support; **Settings → Language** changes the launcher's selection and the language preference presented to games.
- On first configuration the launcher derives its language choice from the PS5 system language. Installed language catalogs and fallback fonts must be present for the full experience.
- Text labels accompany many icons and controls. The focus treatment and language selection help orientation, but cannot guarantee that every label fits, every translation is complete, or every icon is self-explanatory.
- A game's subtitles, captions, font size, color settings and language availability belong to **that game**, unless Encore explicitly offers a separate feature for them.

See [INSTALL.md](INSTALL.md) for complete package and translation prerequisites.

## Currently unverified or unsupported

The following should **not** be represented as implemented or certified without additional evidence:

- **Screen reader, speech narration and text-to-speech:** no verified end-to-end integration for Encore's custom-drawn launcher.
- **Braille display, voice-only operation and full keyboard-only operation:** not validated for the PS5 native launcher.
- **Full color-blindness presets or contrast-ratio conformance:** high contrast is available, but there is no completed WCAG 2.2 AA/AAA, low-vision or color-vision audit.
- **Captions, audio descriptions and photosensitivity protection for games:** not features provided universally by this emulator.
- **Complete motion elimination:** Reduce motion affects launcher presentation, not the emulated game or all third-party imagery.
- **Accessibility on other PS5 firmware:** the project's primary hardware-tested target is **firmware 13.60**.

Host-side screenshot or source tests only verify specific rendering and source contracts. A successful CI run **does not establish accessibility on a real console or display**.

## How to report an accessibility problem

Use [GitHub Issues](https://github.com/niakw/ProsperoEden-Encore/issues/new/choose) for non-security issues. Please include:

1. The **Encore commit or release**, PS5 firmware, display resolution and whether testing used a real PS5 or a host preview.
2. Where the problem occurs: Home, Library, Settings, an accessibility modal, or in-game input; include the exact menu route.
3. Your **Larger text / High contrast / Reduce motion** settings, UI language and controller layout.
4. The steps to reproduce and the expected outcome. For focus or navigation failures, give the buttons pressed in order.
5. If possible, a redacted screenshot or short video showing clipping, contrast, missing focus or an inaccessible interaction.

Do not upload console keys, account details, saved games, identifying information or copyrighted game content. Report security vulnerabilities **privately** under [SECURITY.md](SECURITY.md), not in an ordinary accessibility issue.

You are welcome to report an accessibility barrier without disclosing any diagnosis or personal medical details.

## Development checklist

Accessibility changes should be tested across **English and a longer translation**, with Larger text, High contrast and Reduce motion both On and Off. Review focus visibility, button mapping, dialog confirmations, text clipping and controller navigation at **1080p as well as other supported TV outputs**.

Keep console tests, host renders and source-only assertions separate in PR descriptions. For general contribution guidance, see [CONTRIBUTING.md](CONTRIBUTING.md). Relevant source: `headless/prosperoeden/pe/ui/settings.cpp`, `theme.hpp` and `launcher.cpp`.

**This document describes verified source capabilities, not a universal accessibility compliance claim.**
