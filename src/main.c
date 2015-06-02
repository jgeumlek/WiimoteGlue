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
  int no_keyboardmouse;
  int check_for_existing_wiimotes;
  int monitor_for_new_wiimotes;
  int ignore_pro;
  int no_set_leds;
  char* virt_gamepad_name;
  char* virt_keyboardmouse_name;
  char* uinput_path;
} options;


int init_mappings(struct mode_mappings *maps, char* name);
int handle_arguments(struct commandline_options *options, int argc, char *argv[]);

int main(int argc, char *argv[]) {
  struct udev *udev = NULL;
  struct wiimoteglue_state state;
  KEEP_LOOPING = &state.keep_looping;
  memset(&state, 0, sizeof(state));
  memset(&options, 0, sizeof(options));
  int monitor_fd;
  int epfd;
  int ret;
  options.number_of_slots = -1; /*initialize so we know it has been set*/
  options.monitor_for_new_wiimotes = 1; /*sensible default values*/
  options.check_for_existing_wiimotes = 1;
  ret = handle_arguments(&options, argc, argv);
  if (ret == 1) {
    return 0; /*arguments just said to print out help or version info.*/
  }
  if (ret < 0) {
    return ret;
  }
  printf("WiimoteGlue Version %s\n\n",WIIMOTEGLUE_VERSION);

  if (!options.no_set_leds)
    state.set_leds = 1;

  if (options.ignore_pro) {
    printf("Wii U Pro controllers will be ignored.\n");
    state.ignore_pro = 1;
  }


  if (options.uinput_path == NULL) {
    printf("Trying to find uinput... ");
    options.uinput_path = try_to_find_uinput();
    if (options.uinput_path == NULL) {
      printf("not found.\n");
      printf("Use --uinput-path to specify its location.\n");
      return -1;
    } else {
      printf("%s\n",options.uinput_path);
    }
  }


  state.num_slots = 4;
  if (options.number_of_slots != -1) {
    state.num_slots = options.number_of_slots;
  }

  /*add one for the keyboardmouse spot*/
  state.slots = calloc((1+state.num_slots),sizeof(struct virtual_controller));

  printf("Creating %d gamepad(s) and a keyboard/mouse...",state.num_slots);
  fflush(stdout);
  int i;



  ret = wiimoteglue_uinput_init(state.num_slots, state.slots,options.uinput_path);

  if (ret) {
    printf("\nError in creating uinput devices, aborting.\nCheck the permissions.\n");
    wiimoteglue_uinput_close(state.num_slots,state.slots);
    return -1;
  } else {
    printf(" okay.\n");
  }

  state.virtual_keyboardmouse_fd = state.slots[0].uinput_fd;

  state.head_map.next = &state.head_map;
  state.head_map.prev = &state.head_map;
  init_gamepad_mappings(&state.head_map.maps,"gamepad");

  struct map_list *keymouse = create_mappings(&state,"keyboardmouse");
  init_keyboardmouse_mappings(&keymouse->maps);
  set_slot_specific_mappings(&state.slots[0],&keymouse->maps);


  /*The device list is cyclic*/
  state.dev_list.next = &state.dev_list;
  state.dev_list.prev = &state.dev_list;


  if (options.monitor_for_new_wiimotes) {
    printf("Starting udev monitor...");
    ret = wiimoteglue_udev_monitor_init(&udev, &state.monitor, &monitor_fd);
    if (ret) {
      printf("\nError in creating udev monitor. No additional controllers will be detected.");
    } else {
      printf(" okay.\n");
    }
  } else {
    printf("No monitor started; no new devices will be found.\n");
  }


  printf("KB name: %s\n",keymouse->maps.name);


  wiimoteglue_epoll_init(&epfd);
  if (options.monitor_for_new_wiimotes)
    wiimoteglue_epoll_watch_monitor(epfd, monitor_fd, state.monitor);

  wiimoteglue_epoll_watch_stdin(&state, epfd);

  state.epfd = epfd;


  //Start forwarding input events.

  //Process user input.
  state.keep_looping = 1;

  state.dev_count = 0;

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);
  if (options.file_to_load != NULL) {
    printf("\n");
    wiimoteglue_load_command_file(&state,options.file_to_load);
    printf("\n");
  }

  if (options.check_for_existing_wiimotes) {
    printf("Looking for already connected devices...\n");
    ret = wiimoteglue_udev_enumerate(&state, &udev);
    if (ret) {
      printf("\nError in udev enumeration. No existing controllers will be found.\n");
    }
  } else {
    printf("Any currently connected wiimotes will be ignored.\n");
  }

  if (options.monitor_for_new_wiimotes)
    printf("\nWiimoteGlue is now running and waiting for Wiimotes.\n");
  printf("Enter \"help\" for available commands.\n>>");
  fflush(stdout);
  wiimoteglue_epoll_loop(epfd, &state);



  printf("Shutting down...\n");


  for (i = 0; i <= state.num_slots; i++)
    change_slot_type(&state,&state.slots[i],SLOT_GAMEPAD);

  struct wii_device_list *list_node = state.dev_list.next;
  while (list_node != &state.dev_list && list_node != NULL) {

    struct wii_device_list *next = list_node->next;

    close_wii_device(&state,list_node->dev);
    forget_wii_device(&state,list_node->dev);

    list_node = next;
  }

  struct map_list *mlist_node;
  mlist_node = state.head_map.next;
  while (mlist_node != NULL && mlist_node != &state.head_map) {
    struct map_list *next = mlist_node->next;
    free(mlist_node->maps.name);
    free(mlist_node);
    mlist_node = next;
  }



  wiimoteglue_uinput_close(state.num_slots, state.slots);

  free(state.slots);


  if (options.monitor_for_new_wiimotes)
    udev_monitor_unref(state.monitor);

  if (udev != NULL)
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
     printf("  -n, --num-pads <number>\tNumber of fake gamepad slots to create\n");
     printf("      --uinput-path\t\tSpecify path to uinput\n");
     printf("      --no-enumerate\t\tDon't enumerate connected devices\n");
     printf("      --no-monitor\t\tDon't listen for new devices.\n");
     printf("      --ignore-pro\t\tIgnore Wii U Pro controllers\n");
     printf("      --no-set-leds\t\tDon't change controller LEDS\n");
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
   } else if (strcmp("--dir",argv[0]) == 0 || strcmp("-d",argv[0]) == 0) {
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
   } else if (strcmp("--uinput-path",argv[0]) == 0) {
     if (argc < 2) {
       printf("Argument \"%s\" requires a path.\n",argv[0]);
       return -1;
     }

     options->uinput_path = argv[1];

     argc--;
     argv++;
   } else if (strcmp("--num-pads",argv[0]) == 0 || strcmp("-n",argv[0]) == 0) {
     if (argc < 2) {
       printf("Argument \"%s\" requires a number.\n",argv[0]);
       return -1;
     }

     int num = argv[1][0] - '0'; /*HACK bad atoi*/


     if (num < 0 || num > 9 || argv[1][0] == '\0' || argv[1][1] != '\0') {
       printf("Number of gamepad slots %d must be in range 0 to 9\n",num);
       return -1;
     }

     options->number_of_slots = num;

     argc--;
     argv++;
   } else if (strcmp("--ignore-pro",argv[0]) == 0) {
     options->ignore_pro = 1;
   } else if (strcmp("--no-set-leds",argv[0]) == 0) {
     options->no_set_leds = 1;
   } else if (strcmp("--no-enumerate",argv[0]) == 0) {
     options->check_for_existing_wiimotes = 0;
   } else if (strcmp("--no-monitor",argv[0]) == 0) {
     options->monitor_for_new_wiimotes = 0;
   } else {
     printf("Argument \"%s\" not recognized.\n",argv[0]);
     return -1;
   }

   argc--;
   argv++;
 }
 return 0;
}



