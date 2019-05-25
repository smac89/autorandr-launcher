# autorandr-launcher
Automatically runs autorandr to restore graphical session properties

## Requirements
- [autorandr](https://github.com/phillipberndt/autorandr)

## Installation
- `#> make install`

## Running
- `$> systemctl enable --user --now autorandr_launcher.service`
- `$> journalctl --follow --identifier='autorandr-launcher-service'`
- Profit!

