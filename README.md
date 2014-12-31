#WiimoteGlue
*Userspace driver for Wii remotes and their extensions to simulate standard gamepads*

(Tested only on Arch Linux, 64-bit)

##Motivation

The Linux kernel driver is pretty handy, but the extension controllers like the Nunchuk show up as separate devices. Extra features like the accelerometers or infared sensors also show up as separate devices. Since very little software supports taking input from multiple devices for a single player, using a wiimote for tilt controls or using the wiimote/nunchuk combo is rarely doable. WiimoteGlue acts to combine these into one synthetic device, and adds some extra features to further improve usability.

##Features

* Creates a synthetic virtual gamepad that works in most modern software that expect gamepads.
* Supports extension controllers such as the nunchuk or classic controller.
* Also grabs Wii U pro controllers and allows remapping buttons.
* Configurable button mappings to get that ideal control scheme.
* Dynamic control mappings that change when extensions are inserted or removed.
* Basic processing of accelerometer or infared data.
* Rudimentary support of the Wii Balance Board in addition to a standard controller. Surf your way through your games!
* Read in control mappings from files.
* Uses the Linux gamepad API button defintions rather than ambiguous labels like "A","B","X","Y" for the virtual gamepad.
* Virtual gamepads persist for as long as WiimoteGlue is running, so even software not supporting gamepad hotplugging can be oblivious to Wii remotes connecting/disconnecting.
* Assuming proper file permissions on input devices, does not require super-user privileges.

##Documentation

This README, the help text produced while running WiimoteGlue, and the sample file "maps/readme_sample" are the only documentation at the moment.

##Requirements

* Needs the wiimote kernel driver.
* Built on top of the xwiimote library, a handy wrapper for the kernel driver.
* Uses uinput for creating the virtual gamepads
* Uses udev for finding connected Wii controllers.
* Uses epoll to wait for events to process.
* You need to have read permissions on the various wiimote devices, and write permission to uinput.

You also need to be able to connect your controllers in the first place. This task is done by your bluetooth system, not this software. It has been tested on Bluez 5.26 using bluetoothctl. The wiimote plugin for Bluez 4.96 should be sufficient. Various bluetooth GUIs can work, but some don't handle the wiimote pairing peculiarities well. Using the wiimote sync button can work better than pushing 1+2.

##Future Goals

* Set the player number LEDs on the controllers to match the virtual gamepad numbers.
* Add multi-threading for processing the input events.
* Improve the accelerometer/infared/balance board processing
* Add a "waggle button" that is triggered when the wiimote or nunchuk are shaken.
* Add a mode for the balance board that modulates an axis or axes by walking in place.
* Add commandline arguments for more options
* Add in rumble support.
* Allow buttons to be mapped to axes, and vice versa.
* Improve the control mapping files to be less cumbersome.
* Add a virtual keyboard/mouse for extra possibilities for remapping, like using a wiimote for media player controls.
* Way off: add in a GUI or interface for controlling the driver outside of the the driver's STDIN. System tray icon?
* Support for other extensions like the guitar or drum controllers.
* A means of calibrating the axes?
* A means to reallocate Wii devices to the virtual gamepads.

##Known Issues

* Though each controller can be in a different mode depending on its extension, there is just one mapping for each mode. (e.g. Player 1 with just a wiimote and player 2 with wiimote/nunchuk will have different control mappings. If Player 1 inserts a nunchuk, they will share the wiimote/nunchuk control mapping.)
* Number of virtual gamepads is hard-coded to 4.
* Any wiimotes connected before starting WiimoteGlue will be ignored.
* Currently single-threaded to handle all input events across all controllers. May introduce latency? Most shared data is written only by the command interface; it is only read elsewhere. The real trouble points are adding/deleting entries of the devicelist and keeping track of STDIN commands versus commands from a file.
* Virtual gamepads don't change their axis sensitivities or deadzones when their input sources change. The deadzone ideal for a thumb stick might not be the ideal for a tilt control.
* Code is messy as a personal project. Particularly, i18n was not a concern when writing it. Sorry.
* Uses a udev monitor per wiimote despite xwiimote saying not to do that.
* Assumes uinput is located at /dev/uinput
* Doesn't handle permission issues gracefully.
* Wiimote buttons are still processed when a classic controller is present, despite duplicate buttons. The duplicate button events are mapped the same, and interleaved onto to the synthetic gamepad, but this generally isn't a huge problem.
* Not really designed to handle multiple instances of WiimoteGlue running, mostly due to them grabbing the same wiimotes.
* No current way to change directory used for loading command files; it is just the current directory from when WiimoteGlue was run.
* Only the default classic control mappings technically follow the Linux gamepad standards. The wiimote and wiimote/nunchuk default mappings were chosen to be vaguely useful in most games, rather than following the standard to the letter.


##FAQ-ish

###What's this about file permissions for the devices?
WiimoteGlue will fail if you don't have the right permissions, and you likely won't have the right permissions unless you do some extra work. Though not recommended for regular use, running WiimoteGlue as a super-user can be a useful way to try it out before you have the permissions sorted out.

You need write access to uinput to create the virtual gamepads.

You need read access to the various event devices created by the kernel driver. Either run WiimoteGlue as root (not recommended) or set up some udev rules to automatically change the permissions. i'd recommed creating some sort of "input" group, changing the group ownership of the devices to be that group, and add your user account to that group (Reminder: you need to open a new shell to update your group permissions after adding yourself to the group!)

    KERNEL=="event*", DRIVERS=="wiimote", GROUP="input", MODE="0660"

seems to be a working udev rule for me.

When rumble support is added, you'll need write access as well. (though only to the core wimote and Wii U pro devices; nunchuks and classic controllers don't have rumble)

When LED changing is added, you'll also need write access to the LED brightness files. These LED devices are handled with by the kernel LED subsystem instead of the input subsystem.

    SUBSYSTEM=="leds", ACTION=="add", DRIVERS=="wiimote", RUN+="/bin/sh -c 'chgrp input /sys%p/brightness'", RUN+="/bin/sh -c 'chmod g+w /sys%p/brightness'"

seems to be a working for me, but there is probably a better way to write this rule.




###North, south, east, west? What are those? My "A" button isn't acting like a "A" button.

See the Linux gamepad documentation. These are the "face buttons" generally pushed by one's right thumb.

https://www.kernel.org/doc/Documentation/input/gamepad.txt

Note that WiimoteGlue uses the event names TR2 and TL2 instead of ZR and ZL.

Nintendo's "A" button isn't placed where the Xbox "A" button is. The default mapping for classic-style controllers matches the Linux gamepad documentation. Notably the usual Nintendo layout, the usual Xbox layout, and the Linux gamepad layout are all different. You'll need to make your own decisions on what mapping is best for you, (Many games today expect Xbox controllers; just because a game says "push A" doesn't mean it knows what button on your controller has an "A" on it.)

For reference:

* Nintendo ABXY is ESNW
* Xbox ABXY is SEWN
* Linux Gamepad ABXY is SENW

(Hence why BTN_A,BTN_B etc. are deprecated event names... but many utilities like evtest still print out BTN_A instead of BTN_SOUTH)

Playstation controllers don't even use labels like A,B, or Y but instead use shapes. One of those shapes is "X," and it doesn't line up with any of the three layouts above.

###How do I connect a wiimote?

That is outside the scope of this. Your bluetooth system handles this. This software assumes your bluetooth stack and kernel wiimote driver are already working and usable.

See https://wiki.archlinux.org/index.php/XWiimote for more information on connecting wiimotes.

Note that this uses xwiimote and the kernel driver, not one of the various wiimote libraries like cwiid that do handle connections, so the info on https://wiki.archlinux.org/index.php/Wiimote is not applicable. To use Wiimoteglue, use XWiimote; do not use cwiid and wminput.

Aside from seeing the device entries get created by the kernel driver, a successful connection can be seen by the Wiimote LEDs switching to having just the left-most one lit. Prior to that, all 4 LEDs will be blinking while the wiimote is in sync mode.

I have had some confusing experience of the wiimote connections sometimes consistently failing then magically working when I try later. I've also seen an unrelated issue when repeatedly and quickly disconnecting and reconnecting a controller. These are once again, outside the scope of WiimoteGlue.

In the former case, the bluetooth connection fails. In the latter issue, the connection succeeds, but the no kernel devices are created. It seemed like the wiimote connected and was on, but all 4 of its LEDs were off since the kernel driver never set them.

###My classic controller d-pad is acting like arrow keys?

This is a result of the kernel driver's mapping for the classic controller. The DPAD is indeed mapped to the arrow keys, which means the classic controller is picked up as keyboard. Uses "xinput" to disable the classic controller as a keyboard if this poses a problem. "xinput" is also useful if your gamepads are being picked up as mice to move the cursor.

Another fun fact about the classic controller kernel driver: since it doesn't map the left-stick to ABS_X/ABS_Y, the classic controller is not picked up as a gamepad by SDL, which makes it invisible to many modern games. WiimoteGlue's virtual gamepads are picked up by SDL, so even without the gluing/dynamic remapping feature, WiimoteGlue already improves classic controller usability.

(To be fair, I'll point out that the kernel driver for the classic controller predates the Linux gamepad API where the dpad event codes were formalized, and it is thanks to David Herrmann that both the kernel driver and the gamepad API exist.)

[For those curious, SDL requires a device to give out BTN_SOUTH and ABS_X/ABS_Y to be a gamepad. The wiimote alone doesn't have ABS_X, the nunchuk doesn't have BTN_SOUTH, and the classic controller doesn't have ABS_X. The Wii U Pro does meet the SDL gamepad requirements under the kernel driver.]

###Infared data?

The Wii remote has an IR camera that detects infared sources for tracking. This is the "pointer" functionality for the Wii. One can easily purchase USB versions of the Wii sensor bar to get some usable IR sources.

Currently WiimoteGlue will average all visible IR points, and output them as some scaled axes with a fair bit of margins to allow the axes to go "full tilt" before one the points disappear out of view.

###Balance Board support?

Every virtual gamepad can have at most one standard controller (wiimote+extensions or Wii U pro controller) and one balance board allocated to it. When a balance board is connected, it gets assigned to the first virtual gamepad with no board assigned yet.

Currently does a rough average across the four weight sensors to get a center of gravity, and also adds a fair bit of margins.

Most games don't really handle the large shifts of a standing person well, but using it with one's feet while seated in a chair is surprisingly effective.

In the future, I'd like it if the balance board can use walking-in-place balance shifts to modulate an output axis. A standard controller could pick the axis direction, but one's pace would affec the magnitude.

All in all, the balance board functionality will probably always remain a novelty rather than an actually nice control scheme.

###Acceleration Tilt Controls?

The last few accel values are averaged to smooth the signal a little. Different wiimotes can often have different biases and scaling for tilting, but WiimoteGlue does not support changing the axis scalings.

###Rumble?

Not supported. (yet?)

###Keyboard or mouse mappings?

Not supported. (yet?)

###Analog triggers?

Unsupported. However, only the original Classic controller extension for the wiimote has analog triggers. (The Classic controller pro, distinct from the Wii U pro controller, does not have analog triggers.)

Unless you are using the oddly shaped oval classic controller with no hand grips, you aren't going to have analog triggers.

Mapping one of the various input axis to a virtual analog trigger is also unsupported.

###Motion Plus?

Unsupported, but it should be successfully ignored. Xwiimote does expose the motionplus data and even does a little calibration. If someone wants to write an effective way of using the gyroscope data on a virtual gamepad, please do so.


Theoretically the Motion Plus gyroscope data and the Wii remote accelerometerscould be used together to create a fairly stable reading of the Wii remote's orientation of pitch, yaw, and roll in the real world. (The infared sensor could also be used for a nice re-calibration whenever the IR sources come into view again.)

As is, the accelerometers provide a fairly noisy reading of pitch and roll, but yaw is entirely unknown. (The accelerometers can tell you the direction of gravity, when the Wii remote is held still.)

###Wii U Gamepad?

Unsupported. It doesn't use bluetooth, but wifi to communicate. It is unlikely to ever be supported by this software.

An an aside: Check out libDRC for some existing work on using the Gamepad under Linux. (It is rough at the the moment, and held back by the need to expose wifi TSF values to userland, and not all wifi hardware handles TSF sufficiently for the gamepad.)

###Why not use the kernel driver itself, or the exisitng X11 driver xf86-input-xwiimote?

As noted above, the kernel driver makes separate devices for different features/extensions of the wiimote, making them all but unusable in most games. The classic controller has legacy mapping issues leading to it being ignored by SDL. With the kernel driver alone, only the Wii U Pro controller is useful.

The X11 driver is also handy, but it is designed for emulating a keyboard/mouse instead of a gamepad. Though it allows configuring the button mappings, it is not dynamic and cannot be changed while running. These two aspects make it not the most useful for playing games.



