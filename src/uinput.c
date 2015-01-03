#include <linux/uinput.h>
#include <linux/input.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "wiimoteglue.h"

/*Handles opening uinput and creating virtual gamepads.
 *Change the location /dev/uinput below if you need to.
 */

char* try_to_find_uinput() {
  static char* paths[] = {
    "/dev/uinput",
    "/dev/input/uinput",
    "/dev/misc/uinput"
  };
  const int num_paths = 3;
  int i;

  for (i = 0; i < num_paths; i++) {
    if (access(paths[i],F_OK) == 0) {
      return paths[i];
    }
  }

  return NULL;

}


int wiimoteglue_uinput_init(int num_slots, struct virtual_controller slots[], char* uinput_path) {
  int uinput_fd = 0;
  int i;
  int keyboardmouse_fd = open_uinput_keyboardmouse_fd(uinput_path);
  if (keyboardmouse_fd < 0) {
    return -1;
  }

  slots[0].uinput_fd = keyboardmouse_fd;
  slots[0].keyboardmouse_fd = slots[0].uinput_fd;
  slots[0].gamepad_fd = slots[0].uinput_fd;
  slots[0].has_wiimote = 0;
  slots[0].has_board = 0;
  slots[0].slot_number = 0;
  slots[0].dev_list.next = &slots[i].dev_list;
  slots[0].dev_list.prev = &slots[i].dev_list;


  for (i = 1; i <= num_slots; i++) {
    uinput_fd = open_uinput_gamepad_fd(uinput_path);
    if (uinput_fd < 0) {
      return -1;
    }
    slots[i].uinput_fd = uinput_fd;
    slots[i].keyboardmouse_fd = keyboardmouse_fd;
    slots[i].gamepad_fd = uinput_fd;
    slots[i].slot_number = i;
    slots[i].has_wiimote = 0;
    slots[i].has_board = 0;
    slots[i].dev_list.next = &slots[i].dev_list;
    slots[i].dev_list.prev = &slots[i].dev_list;
  }


  return 0;

}

int wiimoteglue_uinput_close(int num_slots, struct virtual_controller slots[]) {
  int i;
  /*Remember, there are num_slots+1 devices, because of the fake keyboard/mouse */
  for (i = 0; i <= num_slots; i++) {
    if (ioctl(slots[i].uinput_fd, UI_DEV_DESTROY) < 0) {
      //printf("Error destroying uinput device.\n");
    }
    close(slots[i].uinput_fd);
  }

  return 0;
}


int open_uinput_gamepad_fd(char* uinput_path) {
  /* as far as I know, you can't change the reported event types
   * after creating the virtual device.
   * So we just create a gamepad with all of them, even if unmapped.
   */
  static int abs[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY};
  static int key[] = { BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST, BTN_SELECT, BTN_MODE, BTN_START, BTN_TL, BTN_TL2, BTN_TR, BTN_TR2, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT, BTN_DPAD_UP};
  struct uinput_user_dev uidev;
  int fd;
  int i;
  //TODO: Read from uinput for rumble events?
  fd = open(uinput_path, O_WRONLY | O_NONBLOCK);
  if (fd < 0) {
    perror("open uinput");
    return -1;
  }
  memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "WiimoteGlue Virtual Gamepad");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1;
  uidev.id.product = 0x1;
  uidev.id.version = 1;

  ioctl(fd, UI_SET_EVBIT, EV_ABS);
  for (i = 0; i < 4; i++) {
    ioctl(fd, UI_SET_ABSBIT, abs[i]);
    uidev.absmin[abs[i]] = -32768;
    uidev.absmax[abs[i]] = 32768;
    uidev.absflat[abs[i]] = 4096;
  }

  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  for (i = 0; i < 15; i++) {
    ioctl(fd, UI_SET_KEYBIT, key[i]);
  }

  write(fd, &uidev, sizeof(uidev));
  if (ioctl(fd, UI_DEV_CREATE) < 0)
    perror("uinput device creation");
  return fd;
}

int open_uinput_keyboardmouse_fd(char* uinput_path) {

  static int abs[] = { ABS_X, ABS_Y};
  static int key[] = { BTN_LEFT, BTN_MIDDLE, BTN_RIGHT,BTN_TOUCH,BTN_TOOL_PEN};
  /* BTN_TOOL_PEN seems to successfully hint to evdev that
   * we are going to be outputting absolute positions,
   * not relative motions.
   */
  struct uinput_user_dev uidev;
  int fd;
  int i;

  //TODO: Read from uinput for rumble events?
  fd = open(uinput_path, O_WRONLY | O_NONBLOCK);
  if (fd < 0) {
    perror("\nopen uinput");
    return -1;
  }
  memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "WiimoteGlue Virtual Keyboard and Mouse");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1;
  uidev.id.product = 0x1;
  uidev.id.version = 1;

  ioctl(fd, UI_SET_EVBIT, EV_ABS);
  for (i = 0; i < 2; i++) {
    ioctl(fd, UI_SET_ABSBIT, abs[i]);
    uidev.absmin[abs[i]] = -32768;
    uidev.absmax[abs[i]] = 32768;
    uidev.absflat[abs[i]] = 4096;
  }



  /*Just set all possible keys that come before BTN_MISC
   * This should cover all reasonable keyboard keys.*/
  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  for (i = 2; i < BTN_MISC; i++) {
    ioctl(fd, UI_SET_KEYBIT, i);
  }
  for (i = KEY_OK; i < KEY_MAX; i++) {
    ioctl(fd, UI_SET_KEYBIT, i);
  }


  /*Set standard mouse buttons*/
  for (i = 0; i < 5; i++) {
    ioctl(fd, UI_SET_KEYBIT, key[i]);
  }

  write(fd, &uidev, sizeof(uidev));
  if (ioctl(fd, UI_DEV_CREATE) < 0)
    perror("uinput device creation");
  return fd;
}