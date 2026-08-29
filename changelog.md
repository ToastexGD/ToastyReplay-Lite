# v1.0.10
- fix the record button not closing endlevellayer
- hide the floating button outside of menu layers
- remove the box behind the macro info text

# v1.0.9
- fix player button not closing endlevellayer when replaying
- hide player button on playback

# v1.0.8
- Fix checkpoint x axis bug

# v1.0.7
- Add a file picker for importing macros

# v1.0.6
- Test build

# v1.0.5
- Fixed Android frame fixing to provide 100% accuracy.

# v1.0.4
- test build

# v1.0.3
- added android and ios support.
- fixed the android 64 bit tick patch skipping the step count conversion, which made every tps above vanilla run too fast.
- fixed the patchless ios tick storage pointing into a read only part of the binary.
- the player scale is applied again after a frame fix on ios.

# v1.0.2
- windows only for now while the mobile ports get sorted out.

# v1.0.1
- fixed an intel mac bug regarding 2p states under the frame fixing system.
- fixed CCMenuItemSpriteExtra clipping with some keybinds.
- fixed slider absolutely fucking up the menu.

# v1.0.0

- Add vanilla input recording and playback with the .ttrl replay format.
- Add a macro list with rename, delete and replay.
- Add a frame stepper with on screen buttons and hold to repeat.
- Add noclip with separate player 1 and player 2 options.
- Add safe mode so completions are not saved while it is on.
- Add a TPS bypass, speedhack and replay specific random seeds.
- Add practice support so recording and playback resume from checkpoints.
- Disable Click Between Frames and Click Between Steps only while recording or playing, then restore them.
- Requires Geode 5.9.0
