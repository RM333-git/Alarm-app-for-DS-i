# Alarm app for DS(i)
[日本語](./README.ja.md) | English

This is an alarm app for the DSi.
It should work on the DS as well, but this has not been thoroughly tested.
To optimize power consumption for RTC interrupts and sleep mode handling, I’ve implemented a custom modification of `pm.c` from devkitpro/calico.

## From the Developer
I dug out my DSi after a long time and created this alarm app.
It’s mostly for my own use, but I figured I’d release it anyway.
I’d love to exchange information with others who are interested in DSi development or enjoy making things!
I’d also like to connect with people who enjoy niche gadget modifications or developing their own apps.

Feel free to ask questions like “How does this part of the code work?” or suggest improvements via Issues, Pull Requests, comments, replies, [X (formerly Twitter)](https://x.com/miri_harusamee), or [Discord](https://discord.gg/5RyUgWPFeA)!

## How to Use
<img width="258" height="386" alt="screenshot" src="https://github.com/user-attachments/assets/2d8f3ff2-75ab-4415-a2df-0733ae9d285a" />
<img width="258" height="385" alt="screenshot_alarm" src="https://github.com/user-attachments/assets/301539f9-18a7-450b-9d80-05f440762286" />
<img width="258" height="385" alt="screenshot_snooze" src="https://github.com/user-attachments/assets/0ac4121d-881a-40cd-963d-83b50f9e3d82" />

- Press the B button to stop the alarm. If snooze is enabled, the alarm will automatically reset to the next snooze time. To disable snooze, press and hold the A button.
- If you close the device without setting an alarm, a warning tone (alarm) will sound.
- Even if the touch panel is unavailable, you can move the cursor and adjust settings using the D-pad or the ABXY buttons.
- Even if one screen is not displaying, you can switch between the top and bottom screens using the SELECT button.
- The LR buttons are not used.
- By the way, you can also stop the alarm by opening and closing the device.

## Credits & Licenses
This project uses (with some modifications) the following excellent open-source libraries and toolkits.
- [devkitPro / libnds](https://devkitpro.org/) (zlib License)
- [Calico](https://github.com/devkitPro/calico) (ZPL 2.1) - *Note: Some source code (`pm.c`) has been modified and is included.

Please refer to libnds_license.txt and calico_COPYING.txt for details on each license.

## Fonts Used
For time display: Corporate Logo ver2

Other text: MigMix 2P


Translated with DeepL.com (free version)
