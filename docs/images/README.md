# Interface captures

`main-interface.png` is a deterministic render of the final interface at 1180 × 730, with an illustrative ready-installation state and the nickname Steve. The data in this image is sample data; the real window queries the existing services.

To generate the previews without starting downloads, signing in or opening Minecraft:

```powershell
./out/build/x64-debug/EternalLauncher.exe --render-preview out/ui-preview
```

The command uses the same GDI+ renderer as the window and produces 28 captures: main screen, identity, installation and AutoUpdate states, plus four settings screens. The seven new installation captures include querying, downloading, Forge, error, cancelled, cancelling and the compact size. The native field in the previews is drawn; editing, focus, keyboard and DPI behavior must be verified in the real window.

The previews don't represent real server results and don't replace installation/login tests.

`settings.png` is the 1180 × 730 preview with the default values, produced by the same renderer. It shows neither a game session nor the user's saved preferences.

`installation.png` is the download preview at 1180 × 730 with illustrative values.
`installation.origin.txt` records its origin. Additional captures from a visible
native host are in `.impeccable/review/installation/installation-live*.png`; they
show real controls with synthetic data, not a real installation or Minecraft session.
