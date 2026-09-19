# Fixed artwork

These BMP files are the canonical, editable application/button/game artwork.
They preserve the former parameter-free renderers' existing pixel output.
The Make production graph copies selected source images with leonos-emit; it
never reads the old build directory or runs a Python renderer. Application
selection remains driven by components.toml and the output configuration.

Migration source: the existing generated BMPs were promoted to source assets on
2026-09-19. The former renderers were tools/make_app_icons.py,
tools/make_window_button_icons.py, and tools/make_minesweeper_assets.py.
Font transformations remain data-dependent and are implemented in leonos-font.c.
