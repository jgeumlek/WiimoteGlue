#ifndef WIIMOTEGLUE_H
#define WIIMOTEGLUE_H

#include <xwiimote.h>
#include <libudev.h>

#define WIIMOTEGLUE_VERSION "1.02.00"


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

/* Used throughout as a max length for various identifiers,
 * like device IDs or mapping names
 */
#define WG_MAX_NAME_SIZE 32




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

struct mode_mappings {
  char* name;
  int reference_count;
  /*I want devices to be able to share
   *mappings, but I also want to be
   *able to free up unused ones.
   */
  struct event_map mode_no_ext;
  struct event_map mode_nunchuk;
  struct event_map mode_classic;
};

struct virtual_controller;
struct wii_device_list;

struct wii_device {

  struct xwii_iface *xwii;
  struct virtual_controller *slot;
  int fd;

  int ifaces;
  struct event_map *map;
  struct mode_mappings* dev_specific_mappings;

  char* id;
  char* bluetooth_addr;
  
  struct udev_device* udev;

  enum DEVICE_TYPE { REMOTE, BALANCE, PRO, UNKNOWN} type;

  /*At any time, a device should be in at most
   *two lists: the main list of all devices,
   *and the list of devices for a certain slot.
   */
  struct wii_device_list *main_list, *slot_list;

  bool original_leds[4];
  /*Let's be nice and leave the LEDs
   *how we found them.
   */
};

/*Mmm... Linked lists.
 *Ease of insertion and deletion is nice.
 *Users are unlikely to use more than a handful
 *of controllers at once, and all lookups/iterations
 *over this list are done in non-time-critical
 *situations.
 */
struct wii_device_list {
  struct wii_device_list *prev, *next;

  struct wii_device *dev;
};

struct map_list {
  struct map_list *prev, *next;

  struct mode_mappings maps;
};

struct virtual_controller {
  int uinput_fd;
  int keyboardmouse_fd;
  int gamepad_fd;
  int slot_number;
  char* slot_name;
  int has_wiimote;
  int has_board;
  enum SLOT_TYPE {SLOT_KEYBOARDMOUSE,SLOT_GAMEPAD} type;
  struct mode_mappings* slot_specific_mappings;

  struct wii_device_list dev_list;
};


struct wiimoteglue_state {
  struct udev_monitor *monitor;
  struct virtual_controller* slots;
  int num_slots;

  int virtual_keyboardmouse_fd; /*Handy enough to keep around*/

  int epfd;
  int keep_looping;
  int load_lines; /*how many lines of loaded files have we processed? */
  int dev_count; /*simple counter for making identifiers*/
  int ignore_pro; /*ignore Wii U Pro Controllers?*/
  int set_leds; /*Should we try changing controlle LEDs?*/

  struct wii_device_list dev_list;
  struct map_list head_map;
};

int * KEEP_LOOPING; //Sprinkle around some checks to let signals interrupt.

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

char* try_to_find_uinput();
int wiimoteglue_uinput_close(int num_slots, struct virtual_controller slots[]);
int wiimoteglue_uinput_init(int num_slots, struct virtual_controller slots[], char* uinput_path);

int wiimoteglue_udev_monitor_init(struct udev **udev, struct udev_monitor **monitor, int *mon_fd);
int wiimoteglue_udev_handle_event(struct wiimoteglue_state* state);

int wiimoteglue_epoll_init(int *epfd);
int wiimoteglue_epoll_watch_monitor(int epfd, int mon_fd, void *monitor);
int wiimoteglue_epoll_watch_wiimote(int epfd, struct wii_device *device);
int wiimoteglue_epoll_watch_stdin(struct wiimoteglue_state* state, int epfd);
void wiimoteglue_epoll_loop(int epfd, struct wiimoteglue_state *state);

int wiimoteglue_load_command_file(struct wiimoteglue_state *state, char *filename);
int wiimoteglue_handle_input(struct wiimoteglue_state *state, int file);

int wiimoteglue_update_wiimote_ifaces(struct wii_device *dev);
int wiimoteglue_handle_wii_event(struct wiimoteglue_state *state, struct wii_device *dev);

struct virtual_controller* find_open_slot(struct wiimoteglue_state *state, int dev_type);
struct virtual_controller* lookup_slot(struct wiimoteglue_state* state, char* name);

int wiimoteglue_compute_all_device_maps(struct wiimoteglue_state* state, struct wii_device_list *devlist);
int compute_device_map(struct wiimoteglue_state* state, struct wii_device *devlist);
struct mode_mappings* lookup_mappings(struct wiimoteglue_state* state, char* map_name);
struct map_list* create_mappings(struct wiimoteglue_state *state, char *name);

int * get_input_key(char *key_name, int button_map[]);
int * get_input_axis(char *axis_name, struct event_map *map);
int get_output_key(char *key_name);
int get_output_axis(char *axis_name);

#endif
