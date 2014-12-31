#ifndef WIIMOTEGLUE_H
#define WIIMOTEGLUE_H

#include <xwiimote.h>
#include <libudev.h>

#define ABS_LIMIT 32767
#define TILT_LIMIT 80
#define TILT_SCALE (ABS_LIMIT/TILT_LIMIT)
#define NUNCHUK_LIMIT 90
#define NUNCHUK_SCALE (ABS_LIMIT/NUNCHUK_LIMIT)
#define CLASSIC_LIMIT 22
#define CLASSIC_SCALE (ABS_LIMIT/CLASSIC_LIMIT)
#define NO_MAP -1

/* Set a limit on file loading to avoid an endless loop.
 * With only so many settings to change, plus a modest
 * amount of comments, this should be sufficient.
 */
#define MAX_LOAD_LINES 500

/* The number of virtual gamepads */
#define NUM_SLOTS 4


struct wii_remote;
struct wii_u_pro;
struct wii_balance;

struct wii_input_sources {
  struct wii_remote *remote;
  struct wii_u_pro *pro;
  struct wii_balance *balance;
};

struct virtual_controller {
  int uinput_fd;
  int slot_number;
  int has_wiimote;
  int has_board;
  struct wii_input_sources sources;
};

struct event_map {

  int button_map[XWII_KEY_NUM];
  //int waggle_button;
  //int nunchuk_waggle_button;
  //int waggle_cooldown;
  int accel_active;
  int accel_map[6][2];
  int stick_map[6][2];
  int balance_map[6][2];
  //int balance_cog;
  int IR_count;
  //int IR_deadzone;
  int IR_map[2][2];
};





struct wii_device_list {
  struct wii_device_list *prev, *next;


  struct xwii_iface *device;
  struct virtual_controller *slot;
  int ifaces;
  struct event_map *map;

  enum DEVICE_TYPE { REMOTE, BALANCE, PRO} type;

  int fd;
};

struct wiimoteglue_state {
  struct udev_monitor *monitor;
  struct virtual_controller slots[NUM_SLOTS]; /*TODO: FIX THIS HARDCODED VALUE.*/
  struct wii_device_list devlist;
  int epfd;
  int keep_looping;
  int load_lines; /*how many lines of loaded files have we processed? */
  struct event_map mode_no_ext;
  struct event_map mode_nunchuk;
  struct event_map mode_classic;
  struct event_map keyboard_mouse; /* unused at the moment */
};

enum axis_entries {
  AXIS_CODE,
  AXIS_SCALE,
};

enum accel_axis {
  WG_ACCELX = 0,
  WG_ACCELY,
  WG_ACCELZ,
  WG_N_ACCELX,
  WG_N_ACCELY,
  WG_N_ACCELZ,
};

enum stick_axis {
  WG_LEFT_X,
  WG_LEFT_Y,
  WG_RIGHT_X,
  WG_RIGHT_Y,
  WG_N_X,
  WG_N_Y,
};

enum IR_axis {
  WG_IR_X,
  WG_IR_Y,
};

enum bal_axis {
  WG_BAL_FL,
  WG_BAL_FR,
  WG_BAL_BL,
  WG_BAL_BR,
  WG_BAL_X,
  WG_BAL_Y
};

int wiimoteglue_uinput_close(int num_slots, struct virtual_controller slots[]);
int wiimoteglue_uinput_init(int num_slots, struct virtual_controller slots[]);

int wiimoteglue_udev_monitor_init(struct udev **udev, struct udev_monitor **monitor, int *mon_fd);
int wiimoteglue_udev_handle_event(struct wiimoteglue_state* state);

int wiimoteglue_epoll_init(int *epfd);
int wiimoteglue_epoll_watch_monitor(int epfd, int mon_fd, void *monitor);
int wiimoteglue_epoll_watch_wiimote(int epfd, struct wii_device_list *device);
int wiimoteglue_epoll_watch_stdin(int epfd);
void wiimoteglue_epoll_loop(int epfd, struct wiimoteglue_state *state);

int wiimoteglue_handle_input(struct wiimoteglue_state *state, int file);
int wiimoteglue_update_wiimote_ifaces(struct wii_device_list *devlist);

int wiimoteglue_handle_wii_event(struct wiimoteglue_state *state, struct wii_device_list *dev);
#endif