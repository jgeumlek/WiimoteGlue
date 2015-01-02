#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <libudev.h>
#include <signal.h>

#include <xwiimote.h>

#include "wiimoteglue.h"




void signal_handler(int signum) {
  printf("Signal received, attempting to clean up.\n");
  *KEEP_LOOPING = 0;
  /*Technically could segfault if we receive a signal
   *before the first line of main() where this
   *pointer is set. But in that case,
   *is segfaulting really such a crime?
   */
}

struct commandline_options {
  /*Many of these are wishful thinking at the moment*/
  char* file_to_load;
  int number_of_slots;
  int create_keyboardmouse;
  int check_for_existing_wiimotes;
  int monitor_for_new_wiimotes;
  int ignore_pro;
  char* virt_gamepad_name;
  char* virt_keyboardmouse_name;
} options;


int init_mappings(struct mode_mappings *maps, char* name);
int handle_arguments(struct commandline_options *options, int argc, char *argv[]);

int main(int argc, char *argv[]) {
  struct udev *udev;
  struct wiimoteglue_state state;
  KEEP_LOOPING = &state.keep_looping;
  memset(&state, 0, sizeof(state));
  memset(&options, 0, sizeof(options));
  int monitor_fd;
  int epfd;
  int ret;

  ret = handle_arguments(&options, argc, argv);
  if (ret == 1) {
    return 0; /*arguments just said to print out help or version info.*/
  }
  if (ret < 0) {
    return ret;
  }

  if (options.ignore_pro) {
    printf("Wii U Pro controllers will be ignored.\n");
    state.ignore_pro = 1;
  }


  //Ask how many fake controllers to make... (ASSUME 4 FOR NOW)
  printf("Creating uinput devices (%d gamepads, and a keyboard/mouse combo)...",NUM_SLOTS);
  int i;



  ret = wiimoteglue_uinput_init(NUM_SLOTS, state.slots);

  if (ret) {
    printf("\nError in creating uinput devices, aborting.\nCheck the permissions.\n");
    wiimoteglue_uinput_close(NUM_SLOTS,state.slots);
    return -1;
  } else {
    printf(" okay.\n");
  }

  state.virtual_keyboardmouse_fd = state.slots[0].uinput_fd;

  init_gamepad_mappings(&state.general_maps,"gamepad");

  struct mode_mappings keymouse;
  init_keyboardmouse_mappings(&keymouse,"keyboardmouse");

  set_slot_specific_mappings(&state.slots[0],&keymouse);




  //Start a monitor? (ASSUME YES FOR NOW)
  printf("Starting udev monitor...");
  ret = wiimoteglue_udev_monitor_init(&udev, &state.monitor, &monitor_fd);
  if (ret) {
    printf("\nError in creating udev monitor. No additional controllers will be detected.");
  } else {
    printf(" okay.\n");
  }

  wiimoteglue_epoll_init(&epfd);
  wiimoteglue_epoll_watch_monitor(epfd, monitor_fd, state.monitor);
  wiimoteglue_epoll_watch_stdin(epfd);

  state.epfd = epfd;

  //Check for existing wiimotes? (ASSUME NO FOR NOW)
  //Existing wiimotes mean our virtual slots won't
  //have earlier device nodes, so programs expecting
  //controllers may grab the wiimotes directly
  //rather than our virtual ones.

  printf("Any currently connected wiimotes will be ignored.\n");

  //Start forwarding input events.

  //Process user input.
  state.keep_looping = 1;

  state.dev_count = 0;

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);
  if (options.file_to_load != NULL) {
    wiimoteglue_load_command_file(&state,options.file_to_load);
  }
  printf("WiimoteGlue is running. Enter \"help\" for available commands.\n>>");
  fflush(stdout);
  wiimoteglue_epoll_loop(epfd, &state);



  printf("Shutting down...\n");
  wiimoteglue_uinput_close(NUM_SLOTS, state.slots);


  struct wii_device_list *list_node = state.devlist.next;
  for (; list_node != NULL; ) {
    xwii_iface_unref(list_node->device);
    struct wii_device_list *next = list_node->next;
    if (list_node->id != NULL) {
      free(list_node->id);
    }
    if (list_node->bluetooth_addr != NULL) {
      free(list_node->bluetooth_addr);
    }
    free(list_node);
    list_node = next;
  }

  udev_monitor_unref(state.monitor);
  udev_unref(udev);

  return 0;
}

int handle_arguments(struct commandline_options *options, int argc, char *argv[]) {
 char* invoke = argv[0];
 argc--;
 argv++;
 /*skip the program name*/
 while (argc > 0) {
   if (strcmp("--help",argv[0]) == 0 || strcmp("-h",argv[0]) == 0) {
     printf("WiimoteGlue Version %s\n",WIIMOTEGLUE_VERSION);
     printf("\t%s [arguments]\n\n",invoke);
     printf("Recognized Arguments:\n");
     printf("  -h, --help\t\t\tShow help text\n");
     printf("  -v, --version\t\t\tShow version string\n");
     printf("  -d, --dir <directory>\t\tSet the directory for command files\n");
     printf("  -l, --load-file <file>\tLoad the file at start-up\n");
     printf("      --ignore-pro\t\tIgnore Wii U Pro controllers\n");
     return 1;
   }
   if (strcmp("--version",argv[0]) == 0 || strcmp("-v",argv[0]) == 0) {
     printf("WiimoteGlue Version %s\n",WIIMOTEGLUE_VERSION);
     return 1;
   }
   if (strcmp("--load-file",argv[0]) == 0 || strcmp("-l",argv[0]) == 0) {
     if (argc < 2) {
       printf("Argument \"%s\" requires a filename to load.\n",argv[0]);
       return -1;
     }
     options->file_to_load = argv[1];
     argc--;
     argv++;
   } if (strcmp("--dir",argv[0]) == 0 || strcmp("-d",argv[0]) == 0) {
     if (argc < 2) {
       printf("Argument \"%s\" requires a directory.\n",argv[0]);
       return -1;
     }

     if (chdir(argv[1]) < 0) {
       printf("Could not switch to directory \"%s\"\n",argv[1]);
       perror("chdir");
       return -1;
     }

     argc--;
     argv++;
   } else if (strcmp("--ignore-pro",argv[0]) == 0) {
     options->ignore_pro = 1;
   } else {
     printf("Argument \"%s\" not recognized.\n",argv[0]);
     return -1;
   }

   argc--;
   argv++;
 }
 return 0;
}



