#WiimoteGlue
*Userspace driver for Wii remotes and their extensions to simulate standard gamepads*

(Tested only on Arch Linux, 64-bit)

##Building

    make

If you get undefined KEY_* errors, try

    make keycodefix
    make

This will generate new key_codes.c that should use only KEY_* constants that you have defined.

It requires udev, uinput, and xwiimote.

https://github.com/dvdhrm/xwiimote

(also available in the AUR https://aur.archlinux.org/packages/xwiimote/ )

##Running

    ./wiimoteglue

WiimoteGlue will start up and wait for Wii remotes to show up. WiimoteGlue will read commands to from its standard input.

    ./wiimoteglue --help

to see the (very few) command line arguments.


You'll need certain file permissions on the uinput and wiimote event devices.

You might need to use --uinput-path to tell WiimoteGlue where the uinput device is on your system. (You might even need to modprobe uinput)



##Motivation

The Linux kernel driver for wiimotes is pretty handy, but the extension controllers like the Nunchuk show up as separate devices. Not just that, extra features like the accelerometers or infared sensors also show up as separate devices. Not many games support taking input from multiple devices for a single player, so using a wiimote for tilt controls or using the wiimote/nunchuk combo is often not possible. WiimoteGlue acts to combine these into a synthetic gamepad, and adds some extra features to further improve usability.

##Features

* Creates synthetic virtual gamepads that work in most modern software that expect gamepads.
* Supports extension controllers such as the nunchuk or classic controller.
* Configurable at run-time button mappings to get that ideal control scheme.
* Also grabs Wii U pro controllers and allows remapping buttons.
* Dynamic control mappings that change when extensions are inserted or removed.
* Controller LEDs are changed to match the virtual gamepad slot they are in.
* Basic processing of accelerometer or infared data.
* Basic support of the Wii Balance Board in addition to a standard controller. Surf your way through your games!
* Read in control mappings from files. Set up a file per game, and you can switch between them easily.
* Uses the Linux gamepad API button defintions rather than ambiguous labels like "A","B","X","Y"  or "Button 0" for the virtual gamepad.
* Virtual gamepads persist for as long as WiimoteGlue is running, so even software not supporting gamepad hotplugging can be oblivious to Wii remotes connecting/disconnecting.
* Also creates a virtual keyboard/mouse device that can be mapped.
* Assuming proper file permissions on input devices, this does not require super-user privileges.

##Example of Why You Might Use WiimoteGlue

You decide to play \<popular kart racing game\> with just a sideways Wii remote, tilting the controller to steer for some silly fun.

After veering off the track repeatedly, it is time to get serious. You plug in the Nunchuk extension controller, and instantly the controls change. Rather than flimsy tilt-based steering, you now have the reliable Nunchuk's control stick handling your steering, and the buttons on the Wii remote have changed as well to match your new grip.

After a while, a friend comes by. "Oh hey, it's \<popular kart racing game\>! Can I play? Wait. What's with that funky controller? I don't trust my left and right hands to be separated by a cord, and there's no way that has enough buttons."

Before they turn away, you manage to swap the Nunchuk out for a Wii Classic Controller. Like magic, the controller works as expected. No need to fiddle with control mappings every time an extension changes, and no need to change the game options to use the newly connected Classic Controller. Your friend, eased by the comforting vague familiarity of the Classic Controller, enjoys their race.

After they leave, you remove the Classic Controller, change a few mappings in WiimoteGlue, connect a Wii Balance Board, and decide to try steering with your feet.

##Documentation

This README, the help text produced while running WiimoteGlue, and the sample file "maps/demo_readme" are the only documentation at the moment.

This README deals mostly with what WiimoteGlue is, and what it does.

The help text in the program gives the specifics of the available commands and their expected formats.

The demo_readme file offers a quick explanation/demonstration on how to map buttons, but does not demonstrate all commands, features.

(The media_control mapping file offers a demo of the keyboard/mouse mapping.)

##Requirements

* Needs the wiimote kernel driver, hid-wiimote.
* Built on top of the xwiimote library, a handy wrapper for the kernel driver.
* Uses uinput for creating the virtual gamepads
* Uses udev for finding connected Wii controllers.
* Uses epoll to wait for events to process.
* You need to have read permissions on the various wiimote devices, and write permission to uinput.

You also need to be able to connect your controllers in the first place. This task is done by your bluetooth system, not this software. It has been tested on Bluez 5.26 using bluetoothctl. The wiimote plugin for Bluez 4.96 should be sufficient. Various bluetooth GUIs can work, but some don't handle the wiimote pairing peculiarities well. Using the wiimote sync button can work better than pushing 1+2.

See https://wiki.archlinux.org/index.php/XWiimote for more info on connecting wiimotes.

##Future Goals


* Add multi-threading for processing the input events.
* Improve the accelerometer/infared/balance board processing
* Add a "waggle button" that is triggered when the wiimote or nunchuk are shaken.
* Add a mode for the balance board that modulates an axis (or axes?) by walking in place.
* Add more commandline arguments for more options
* Add in rumble support.
* Allow buttons to be mapped to axes, and vice versa.
* Improve the control mapping files to be less cumbersome.
* Way off: add in a GUI or interface for controlling the driver outside of the the driver's STDIN. System tray icon?
* A means of calibrating the axes?
* A reasonable way to let separate controllers have separate control mappings while not making extra work when the same mapping is wanted on all.
* Clean and document the code in general.
* A way to store device aliases based on their unique bluetooth addresses.

##Known Issues

* Though each controller can be in a different mode depending on its extension, there are just two mappings for each mode: gamepad or keyboard.
* Sometimes extensions aren't detected, especially when already inserted when a wiimote connects. Unplugging them and re-inserting generally fixes this.
* Though the Wii U Pro supports changing the button mappings and axis mappings, it does not allow inverting the axes.
* Since the Wii U Pro is already detected by SDL, WiimoteGlue leads to "duplicate" controllers.
* Any wiimotes connected before starting WiimoteGlue will be ignored.
* Keyboard/mouse emulation is not perfect. Expect changes in the interface.
* Currently single-threaded, handling all input events across all controllers. May introduce latency?
* Virtual gamepads don't change their axis sensitivities or deadzones when their input sources change. The deadzone ideal for a thumb stick might not be the ideal for a tilt control.
* Code is messy as a personal project. Particularly, i18n was not a concern when writing it. Sorry.
* Uses a udev monitor per wiimote despite xwiimote saying not to do that.
* Wiimote buttons are still processed when a classic controller is present, despite duplicate buttons. The duplicate button events are mapped the same, and interleaved onto to the synthetic gamepad, but this generally isn't a huge problem.
* Not really designed to handle multiple instances of WiimoteGlue running, mostly due to them grabbing the same wiimotes.
* Virtual output devices aren't "cleared" when their input sources are removed. If you remap a button while it is held down (or an uncentered axis), the old mapping will be "frozen" to whatever input it had last.



##Troubleshooting FAQ-ish Section

###What's this about file permissions for the devices?
WiimoteGlue will fail if you don't have the right permissions, and you likely won't have the right permissions unless you do some extra work. Though not recommended for regular use, running WiimoteGlue as a super-user can be a useful way to try it out before you have the permissions sorted out.

You need write access to uinput to create the virtual gamepads. WiimoteGlue assumes uinput is located at /dev/uinput which might not be true on other distros. If you know where uinput is on your system, change the location at the top of uinput.c

You need read access to the various event devices created by the kernel driver. Either run WiimoteGlue as root (not recommended) or set up some udev rules to automatically change the permissions. I'd recommed creating some sort of "input" group, changing the group ownership of the devices to be that group, and add your user account to that group (Reminder: you need to open a new shell to update your group permissions after adding yourself to the group!)

    KERNEL=="event*", DRIVERS=="wiimote", GROUP="<groupname>", MODE="0660"

(where \<groupname\> is the name of some user group you've added yourself to.0

When rumble support is added, you'll need write access as well. (though only to the core wiimote and Wii U pro devices; nunchuks and classic controllers don't have rumble)

When LED changing is added, you'll also need write access to the LED brightness files. These LED devices are handled with by the kernel LED subsystem instead of the input subsystem.

    SUBSYSTEM=="leds", ACTION=="add", DRIVERS=="wiimote", RUN+="/bin/sh -c 'chgrp <groupname> /sys%p/brightness'", RUN+="/bin/sh -c 'chmod g+w /sys%p/brightness'"

seems to be working for me, but there is probably a better way to write this udev rule.


###North, south, east, west? What are those? My "A" button isn't acting like an "A" button.

See the Linux gamepad documentation. These are the "face buttons" generally pushed by one's right thumb.

https://www.kernel.org/doc/Documentation/input/gamepad.txt

Note that WiimoteGlue uses the event names TR2 and TL2 instead of ZR and ZL.

Nintendo's "A" button isn't placed where the Xbox "A" button is. WiimoteGlue's default mapping for classic-style controllers matches the Linux gamepad documentation. Be aware the usual Nintendo layout, the usual Xbox layout, and the Linux gamepad layout for the face buttons are all different. You'll need to make your own decisions on what mapping is best for you, (Many games today expect Xbox controllers; just because a game says "push A" doesn't mean it knows what button on your controller has an "A" on it.)

For reference:

* Nintendo ABXY is ESNW
* Xbox ABXY is SEWN
* Linux Gamepad ABXY is SENW

(Hence why BTN_A,BTN_B etc. are deprecated event names... but many utilities like evtest still print out BTN_A instead of BTN_SOUTH)

Playstation controllers don't even use labels like A,B, or Y but instead use shapes. One of those shapes is "X," and it doesn't line up with any of the three layouts above. Aren't game controllers fun?


###When I mapped my control stick to the mouse, centering the stick centers the cursor. this doesn't seem very useful?

This is the intended functionality of WiimoteGlue. It outputs the absolute positions from the stick as absolute positions on the virtual mouse.

"Steering" the cursor with the stick would likely be more useful, but would need to be handled in a way fundamentally different from how WiimoteGlue now works. In WiimoteGlue, if the stick doesn't move, no events are generated at all. To steer the cursor, we'd need to consistently and regularly generate events even when the stick is held still in a certain direction.

###This fake mouse pointer is all wonky. Is it working right?

The fake mouse should be auto-detected by evdev in "absolute" mode, but it might have been set to "relative" which is the general mode used by mice.

    xinput --set-mode "WiimoteGlue Virtual Keyboard and Mouse" ABSOLUTE

will fix this, and this setting does not persist after closing WiimoteGlue.

Also note that the infared and accelerometer readings aren't smoothed at all, so using them for controlling the mouse cursor will be noisy.


###I mapped buttons to the keyboard/mouse, but they aren't doing anything?

First, note that you need to specify you wish to change the keyboard mapping instead of the gamepad one.

    map keyboardmouse <mode> <wiimote input event> <virtual output event>

where "\<mode\>" is one of the extension modes: wiimote, nunchuk, or classic.

"keyboardmouse" names a mapping, and a mapping has all three modes. When no mapping name is given, the "map" command assumes you meant the "gamepad" mapping.

Secondly, any keyboard or mouse events sent to a virtual gamepad are ignored. You'll need to do one of the following:

    assign <device name> keyboardmouse

or

    slot 1 keyboardmouse


The first takes a particular device (like a wiimote) and assigns it to the keyboard. The second one takes virtual gamepad slot #1 and switches it to point to the virtual keyboard.

To undo this,

    assign <device name> 1

or

    slot 1 gamepad

depending on what you changed above.

Use

    list

to see all device names, and the "1" can be replaced by "2","3", or "4" to affect the other slots instead.

Note that each mode still has exactly one control mapping, regardless of whether the devices are mapped to a fake keyboard or fake gamepad. Since the fake keyboard ignores gamepad events and gamepads ignore keyboard events, one device can't simultaneously send keyboard/mouse and gamepad events.

Changing the slot rather the device to point to a keyboard is nice since it means future devices connected to the slot will automatically point to the keyboard.

###How do I connect a wiimote?

That is outside the scope of WiimoteGlue. Your bluetooth system handles this. This software assumes your bluetooth stack and kernel wiimote driver are already working and usable.

See https://wiki.archlinux.org/index.php/XWiimote for more information on connecting wiimotes.

Note that this uses xwiimote and the kernel driver, not one of the various wiimote libraries like cwiid that do handle connections, so the info on https://wiki.archlinux.org/index.php/Wiimote is not applicable. To use Wiimoteglue, use XWiimote; do not use cwiid and wminput.

Aside from seeing the device entries created by the kernel driver, a successful connection can be verified by the Wiimote LEDs switching to having just the left-most one lit. Prior to that, all 4 LEDs will be blinking while the wiimote is in sync mode.

I have had some confusing experience of the wiimote connections sometimes consistently failing then magically working when I try later. I've also seen an unrelated issue when repeatedly and quickly disconnecting and reconnecting a controller. These are once again, outside the scope of WiimoteGlue. Some of this might be my bluetooth hardware being flakey.

In the former case, the bluetooth connection fails. In the latter case, the connection succeeds, but the no kernel devices are created. It seemed like the wiimote was connected and was on, but all 4 of its LEDs were off since the kernel driver never set them.

###My classic controller d-pad directions are acting like arrow keys?

This is a result of the kernel driver's mapping for the classic controller. The DPAD is indeed mapped to the arrow keys, and the classic controller is picked up as keyboard. Use "xinput" to disable the classic controller as a keyboard if this poses a problem. "xinput" is also useful if your joysticks are being picked up as mice to move the cursor.

    xinput --disable "Nintendo Wii Remote Classic Controller"

Another fun fact about the classic controller kernel driver: since it doesn't map the left-stick to ABS_X/ABS_Y, the classic controller is not picked up as a gamepad by SDL, which makes it invisible to many modern games. Further, the  Y-axes were inverted. WiimoteGlue's virtual gamepads are picked up by SDL, so even without the gluing/dynamic remapping feature, WiimoteGlue already improves classic controller usability.

(To be fair, I'll point out that the kernel driver for the classic controller predates the Linux gamepad API where the dpad event codes were formalized, and it is thanks to David Herrmann that both the kernel driver and the gamepad API exist, along with xwiimote. Thanks for all the work!)

[For those curious, SDL requires a device to give out BTN_SOUTH and ABS_X/ABS_Y to be detected. The wiimote alone doesn't have ABS_X, the nunchuk doesn't have BTN_SOUTH, and the classic controller doesn't have ABS_X. The Wii U Pro does meet the SDL gamepad requirements under the kernel driver.]

###That duplicated Wii U Pro controller issue is annoying. What can I do?

There are a few work arounds of various ugliness:

1. Assign the pro controller to the "none" slot in WiimoteGlue. WiimoteGlue will ignore it, and you can use the original Wii U Pro device.
2. Delete the original pro controller device after WiimoteGlue has opened it, but before you start the game. WiimoteGlue will hold onto its already opened interface, but the game won't.

If Option #1 sounds better to you, the "--ignore-pro" command line argument for WiimoteGlue is likely what you seek.

One potential work-around would be to have the Wii U Pro event device grabbed with EVIOCGRAB to get it's events exclusively, but I don't think xwiimote has a convenient way to do this. Since Pro controllers don't have extensions to track, it might be worth handling them manually just to do this. (We'd just have to track the single event device + the four LED devices for full functionality.)

##Features of WiimoteGlue FAQ-ish section

###Infared data?

The Wii remote has an IR camera that detects infared sources for tracking. This is the "pointer" functionality for the Wii. One can easily purchase USB versions of the Wii sensor bar to get some usable IR sources, or make your own from IR LEDs or even flames if you are bold enough.

Currently WiimoteGlue will take only the left-most detected IR source. This means mutlipe IR sources (such as the two sides of a sensor bar) are not used to improve the reading, and may in fact lead to strange results.

Adding in an extra big deadzone specifically when handling IR data might be useful, if one wants to use it for panning first-person cameras.

Adding in support for multiple IR sources can improve the data, and allow for some extra virtual axes like rotation or scale (which would translate to distance from the IR sources)

###Balance Board support?

Every virtual gamepad slot can have at most one standard controller (wiimote+extensions or Wii U pro controller) and one balance board allocated to it. When a balance board is connected, it gets assigned to the first virtual gamepad with no board assigned yet.

Currently does a rough average across the four weight sensors to get a center of gravity, and also adds a fair bit of margin to allow it to go full tilt.

Note that the balance board *always* uses the "wiimote" mode mappings, even when extensions are connected.

    map wiimote bal_x left_x

will map *all* balance boards to left_x, regardless of the the presence or absence of nunchuks or other extensions on the controller assigned to the same slot as the balance board.

    map nunchuk bal_x left_x

should have no effect.

Most games don't really handle the large shifts of a standing person well, but using it with one's feet while seated in a chair is surprisingly effective.

In the future, I'd like it if the balance board can use the shifts from walking in place to modulate an output axis. A standard controller could pick the axis direction, but one's pace would affect the magnitude. It might be fun, but probably not very "immersive" compared to other foot-based input devices.

All in all, the balance board functionality will probably always remain a novelty rather than an actually nice control scheme. Why not challenge some friends to some balance-board steering time trials for a laugh?

In the future, WiimoteGlue might also export the four sensors themselves for mapping. One might be able to find uses, like trying to use the balance board as a set of pedals.

###Acceleration Tilt Controls?

The accelerometer values are scaled a little to make them usable as a rough pitch/roll reading, but are otherwise sent directly. These values are very noisy; some extra processing to smooth them might be useful.

Different wiimotes can often have different biases and scaling for their tilting, but WiimoteGlue does not support changing the axis calibrations.

###Rumble?

Not supported. (yet?)

###Keyboard and mouse mappings?

Still in rough early stages, but the basic functionality is there.

For the full list of recognized key names, look at include/uapi/linux/input.h from the Linux kernel source for all the constants. Most things of the form "KEY_<something>" are recognized if you make it all lowercase.

Or you can look at the ugly huge key name lookup function in commandline.c present in WiimoteGlue.

###Analog triggers?

Unsupported. However, only the original Classic controller extension for the wiimote has analog triggers. (The Classic controller pro, distinct from the Wii U pro controller, does not have analog triggers.)

Unless you are using the oddly shaped oval classic controller with no hand grips, you aren't going to have analog triggers anyways.

Mapping one of the various input axes to a virtual analog trigger is also unsupported, but this might change.

###Buttons to sticks? Sticks to buttons?

Not supported.

Currently buttons can only be mapped to buttons, and axes can only be mapped to axes.

###What are the default mappings anyways?

I don't have this information in a handy format. Hopefully the "init_*_mapping()" functions in control_mappings.c will be sufficiently readable for the curious.

###Motion Plus?

Unsupported, but it should be successfully ignored when present. Xwiimote does expose the motionplus data and even does a little calibration. If someone wants to write an effective way of using the gyroscope data on a virtual gamepad, please do so.

Theoretically the Motion Plus gyroscope data and the Wii remote accelerometerscould be used together to create a fairly stable reading of the Wii remote's orientation of pitch, yaw, and roll in the real world. (The infared sensor could also be used for a nice re-calibration whenever the IR sources come into view again.)

As is, the accelerometers provide a fairly noisy reading of pitch and roll, but yaw is entirely unknown. (The accelerometers can tell you the direction of gravity, when the Wii remote is held still.)

###Wiimote Guitar or Drum Controller?

Unsupported, but reasonably easy to add. Xwiimote exposes these. I don't own either of these, so I didn't bother writing code I can't test.

###Wii U Gamepad? [that thing with the screen on it]

Unsupported. It doesn't use bluetooth, but wifi to communicate. It is unlikely to ever be supported by this software.

An an aside: Check out libDRC for some existing work on using the Wii U Gamepad under Linux. (It is rough at the the moment, and held back by the need to expose wifi TSF values to userspace, and not all wifi hardware handles TSF sufficiently for the gamepad.) As far I'm aware, once it is working, libDRC can handle the buttons on the Wii U Gamepad easily; the main challenge is streaming images to the screen in a way that the Gamepad will accept.


###Why not use the kernel driver itself, or the exisitng X11 driver xf86-input-xwiimote?

As noted above, the kernel driver makes separate devices for different features/extensions of the wiimote, making them all but unusable in most games. The classic controller has legacy mapping issues leading to it being ignored by SDL. With the kernel driver alone, only the Wii U Pro controller is useful.

The X11 driver is also handy, but it is designed for emulating a keyboard/mouse instead of a gamepad. Though it allows configuring the button mappings, it is not dynamic and cannot be changed while running. These two aspects make it not the most useful for playing games.


###Why all this hassle over switching between the fake gamepads and the fake keyboard?

One could create a virtual device that has all the functionality of a gamepad, keyboard, and mouse all-in-one. However, both mice and gamepads use ABS_X and ABS_Y. If it was one device, We'd need to tell X to ignore or pay attention to ABS_X/Y depending on whether we are in gamepad mode or mouse mode, since most people don't want their gamepads controlling the cursor.

By separating them, the gamepads are automagically picked up as gamepads, the fake keyboard/mouse is picked up as a keyboard/mouse, and not much extra work is needed.

The evdev autodetection is pretty nice; let's let it do the work, even if we need to jump through a couple hoops to give it the right nudges.

Theoretically WiimoteGlue could keep track of virtual device outputs on a per-button basis rather than per-device, but that seems like a lot of extra complexity for little gain. If you have a valid use case for a single wiimote having both gamepad buttons and keyboard keys on it, it'll be considered.


###Why not have WiimoteGlue talk to X directly to emulate the mouse when needed?

* I don't want to deal with X code.
* With Wayland potentially replacing X in the future, that sort of direct interaction doesn't seem wise. The Linux input system used by WiimoteGlue will likely be relatively stable.

###Why does WiimoteGlue capture Wii U Pro controllers?

At the moment, this functionality doesn't give much advantage over just using the Wii U Pro controller directly. It does let the buttons be remapped, which can be nice for switching around the face buttons.

Since WiimoteGlue handles LEDs based on slots, it makes a handy indicator on the pro controller.

If one desires the balance board combo control schemes, grabbing the Wii U Pro controllers makes them usable for that.

Overall, it seemed like a reasonably easy thing to add with little harm, and in the future it will allow some better consistency for playing with a mix of pro controllers and wiimotes.



