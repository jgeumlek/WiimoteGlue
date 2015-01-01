#include <stdio.h>
#include <string.h>
#include "wiimoteglue.h"

#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>

/* Handles user input from STDIN and command files. */

char * getwholeline(int file) {
  char *buffer = malloc(50);
  char *ptr = buffer;
  size_t totallength = 50;
  int spaceleft = totallength;
  char nextchar[2];
  int ret;

  if(buffer == NULL)
    return NULL;

  for(;;) {
    ret = read(file,nextchar,1);
    if(ret <= 0)
      break;

    if(--spaceleft == 0) {
      spaceleft = totallength;
      char *newbuff = realloc(buffer, totallength *= 2);

      if(newbuff == NULL) {
	free(buffer);
	return NULL;
      }
      ptr = newbuff + (ptr - buffer);
      buffer = newbuff;
    }
    if((*ptr++ = nextchar[0]) == '\n')
      break;
  }

  if (ptr == buffer) {
    /* nothing was read! Something wrong, EOF? */
    free(buffer);
    return NULL;
  }

  *ptr = '\0';
  return buffer;
}

#define NUM_WORDS 5
void process_command(struct wiimoteglue_state *state, char *args[]);
void update_mapping(struct wiimoteglue_state *state, char *mode, char *in, char *out, char *opt);
void toggle_setting(struct wiimoteglue_state *state, int active, char *mode, char *setting, char *opt);
int load_command_file(struct wiimoteglue_state *state, char *filename);
int change_slot(struct wiimoteglue_state *state, char *slotname, char *setting);
int list_devices(struct wii_device_list *devlist, char *option);
struct wii_device_list* lookup_device(struct wii_device_list *devlist, char *name);
int * get_input_key(char *key_name, int button_map[]);
int * get_input_axis(char *axis_name, struct event_map *map);
int get_output_key(char *key_name);
int get_output_axis(char *key_name);

int wiimoteglue_handle_input(struct wiimoteglue_state *state, int file) {
  char *line = getwholeline(file);
  if (line != NULL) {
    char *lineptr = line;
    char *args[NUM_WORDS];
    char **argptr;

    for (argptr = args; (*argptr = strsep(&lineptr, " \t\n")) != NULL;) {
      if (**argptr != '\0') {
	if (++argptr >= &args[NUM_WORDS]) {
          break;
	}
      }
    }


    process_command(state,args);


    free(line);
    return 0;
  }
  return -1;
}

void process_command(struct wiimoteglue_state *state, char *args[]) {
  if (state->load_lines == MAX_LOAD_LINES) {
      printf("Maximum number of lines for command files exceeded.\n");
      printf("Either you have an endless loop in files loading files,\n");
      printf("or your command files are too big. There are only so many buttons to map...\n");
      state->load_lines++;
      return;
  }
  state->load_lines++;

  if (args[0] == NULL) {
    return;
  }
  if (args[0][0] == '#') {
    return;
  }

  if (strcmp(args[0],"quit") == 0) {
    state->keep_looping = 0;
    return;
  }
  if (strcmp(args[0],"help") == 0) {
    printf("The following commands are recognized:\n");
    printf("\thelp - show this message\n");
    printf("\tmap - change a button/axis mapping in one of the modes\n");
    printf("\tenable/disable - control extra controller features\n");
    printf("\tlist - list all open devices\n");
    printf("\tassign - assign a device to a virtual slot\n");
    printf("\tload - opens a file and runs the commands inside\n");
    printf("\tquit - close down WiimoteGlue\n");
    printf("\tmodes - show recognized keywords for controller modes\n");
    printf("\tevents - show recognized keywords for input/output events\n");
    printf("\tfeatures - show recognized controller features to enable/disable\n");
    printf("\nCommands expecting arguments will show their usage formats.\n");
    return;
  }
  if (strcmp(args[0],"modes") == 0) {
    printf("A separate input mapping is maintained for each of the following modes.\n");
    printf("The mode used changes automatically when extensions are inserted/removed.\n");
    printf("\t\"wiimote\" - used when the wiimote has no extension. (Motion Plus is ignored.)\n");
    printf("\t\"nunchuk\" - used when a nunchuk is present.\n");
    printf("\t\"classic\" - used when a classic controller is present, or for a Wii U pro controller\n");

    return;
  }
  if (strcmp(args[0],"events") == 0) {
    printf("The recognized names for input buttons are:\n");
    printf("up, down, left, right, a, b, c, x, y, z, plus, minus, home, 1, 2, l, r, zl, zr, thumbl, thumbr\n\n");

    printf("The recognized names for input axes are:\n");
    printf("\taccel_x - wiimote acceleration x (tilt forward/back)\n");
    printf("\taccel_y - wiimote acceleration y (tilt left/right)\n");
    printf("\taccel_z - wiimote acceleration z\n");
    printf("\tir_x,ir_y - Infared \"pointer\" axes.\n");
    printf("\tn_accel_x - nunchuk acceleration x (tilt forward/back)\n");
    printf("\tn_accel_y - nunchuk acceleration y (tilt left/right)\n");
    printf("\tn_accel_z - nunchuk acceleration z\n");
    printf("\tn_x, n_y - nunchuck control stick axes\n");
    printf("\tleft_x, left_y - classic/pro left stick axes\n");
    printf("\tright_x, right_y - classic/pro right stick axes\n");
    /*Not implemented at the moment.
     * printf("\tbal_fl,bal_fr,bal_bl,bal_br - balance board front/back left/right axes\n");
     */
    printf("\tbal_x,bal_y - balance board center-of-gravity axes.\n");
    printf("\n");

    printf("The recognized names for the synthetic gamepad output buttons are:\n");
    printf("up, down, left, right, north, south, east, west, start, select, mode, tl, tr, tl2, tr2, thumbl, thumbr, none\n");
    printf("(See the Linux gamepad API for these definitions. North/south/etc. refer to the action face buttons.)\n\n");

    printf("When mapped to a keyboard/mouse, these button mappings are available:\n");
    printf("\tleft_click, right_click, middle_click, key_a, key_leftshift, etc.\n");
    printf("(look up uapi/linux/input.h for all key names, just make them lowercase)\n\n");

    printf("The recognized names for the output axes are:\n");
    printf("\tleft_x, left_y - left stick axes\n");
    printf("\tright_x, right_y - right stick axes\n");
    printf("\tnone - an ignored axis\n");

    printf("\tmouse_x, mouse_y - aliases for left_x, left_y\n");

    printf("Add \"invert\" at the end of an axis mapping to invert it.\n");


    return;
  }
  if (strcmp(args[0],"features") == 0) {
    printf("The recognized extra features are:\n");
    printf("\taccel - process and output acceleration axis mappings\n");
    printf("\tir - process the wiimotes infared pointer axes\n");
    printf("(currently no extra options for these features are recognized)\n");
    return;
  }
  if (strcmp(args[0],"map") == 0) {
    update_mapping(state,args[1],args[2],args[3],args[4]);
    return;
  }
  if (strcmp(args[0],"enable") == 0) {
    toggle_setting(state,1,args[1],args[2],args[3]);
    return;
  }
  if (strcmp(args[0],"disable") == 0) {
    toggle_setting(state,0,args[1],args[2],args[3]);
    return;
  }
  if (strcmp(args[0],"load") == 0) {
    load_command_file(state,args[1]);
    return;
  }
  if (strcmp(args[0],"slot") == 0) {
    change_slot(state,args[1],args[2]);
    return;
  }
  if (strcmp(args[0],"assign") == 0) {
     assign_device(state,args[1],args[2]);
     return;
  }
  if (strcmp(args[0],"list") == 0) {
    list_devices(&state->devlist,args[1]);
    return;
  }


  printf("Command not recognized.\n");
}

void update_mapping(struct wiimoteglue_state *state, char *mode, char *in, char *out, char *opt) {
  struct event_map *mapping = NULL;

  if (mode == NULL || in == NULL || out == NULL) {
    printf("Invalid command format.\n");
    printf("usage: map <mode> <wii input> <gamepad output> [invert]\n");
    return;
  }

  if (strcmp(mode,"wiimote") == 0) {
    mapping = &state->mode_no_ext;
  } else if (strcmp(mode,"nunchuk") == 0) {
    mapping = &state->mode_nunchuk;
  } else if (strcmp(mode,"classic") == 0) {
    mapping = &state->mode_classic;
  }

  if (mapping == NULL) {
    printf("Controller mode \"%s\" not recognized.\n(Valid modes are \"wiimote\",\"nunchuk\", and \"classic\")\n",mode);
    return;
  }

  int *button = get_input_key(in,mapping->button_map);
  if (button != NULL) {
      int new_key = get_output_key(out);
      if (new_key == -2) {
	printf("Output button \"%s\" not recognized. See \"events\" for valid values.\n",out);
	return;
      }

      *button = new_key;
      return;
  }

  int *axis = get_input_axis(in,mapping);

  if (axis != NULL) {
    int new_axis = get_output_axis(out);
    if (new_axis == -2) {
      printf("Output axis \"%s\" not recognized. See \"events\" for valid values.\n",out);
      return;
    }

    axis[0] = new_axis;

    if (opt != NULL && strcmp(opt,"invert") == 0) {
      axis[1] = -abs(axis[1]);
    } else {
      axis[1] = abs(axis[1]);
    }
    return;
  }


  printf("Input event \"%s\" not recognized. See \"events\" for valid values.\n",in);
  printf("usage: map <mode> <wii input> <gamepad output> [invert]\n");
  return;


}

void toggle_setting(struct wiimoteglue_state *state, int active, char *mode, char *setting, char *opt) {
  struct event_map *mapping = NULL;

  if (mode == NULL || setting == NULL) {
    printf("Invalid command format.\n");
    printf("usage: <enable|disable> <mode> <feature> [option]\n");
    return;
  }

  if (strcmp(mode,"wiimote") == 0) {
    mapping = &state->mode_no_ext;
  } else if (strcmp(mode,"nunchuk") == 0) {
    mapping = &state->mode_nunchuk;
  } else if (strcmp(mode,"classic") == 0) {
    mapping = &state->mode_classic;
  }

  if (strcmp(setting, "accel") == 0) {
    mapping->accel_active = active;
    wiimoteglue_update_wiimote_ifaces(&state->devlist);
    return;
  }

  if (strcmp(setting, "ir") == 0) {
    int ir_count = 1;

    if (opt != NULL && strcmp(opt,"multiple") == 0) ir_count = 2;

    if (!active) ir_count = 0;

    /*currently multiple IR sources, or a single one are treated identically*/
    mapping->IR_count = ir_count;
    wiimoteglue_update_wiimote_ifaces(&state->devlist);
    return;
  }

  printf("Feature \"%s\" not recognized.\n",setting);
  printf("usage: <enable|disable> <mode> <feature> [option]\n");
}




int load_command_file(struct wiimoteglue_state *state, char *filename) {
  if (filename == NULL) {
    printf("usage: load <filename>\n");
    return 0;
  }
  int fd = open(filename,O_RDONLY);
  if (fd < 0) {
    printf("Failed to open file \'%s\'\n",filename);
    perror("Open");
    return -1;
  }
  printf("Reading commands from file \'%s\'\n",filename);
  int ret = 0;
  while (ret >= 0) {
    if (state->load_lines > MAX_LOAD_LINES) {
      /*exceeded number of lines read, start backing out.*/
      close(fd);
      return -2;
    }
    ret = wiimoteglue_handle_input(state, fd);
  }

  close(fd);
  return 0;

}

int change_slot(struct wiimoteglue_state *state, char *slotname, char *setting) {
  if (slotname == NULL || setting == NULL) {
    printf("usage: slot <slotnumber> <gamepad|keyboardmouse>\n");
  }

  int slotnumber = slotname[0] - '0'; /*HACK: single-digit atoi*/


  if (slotnumber < 1 || slotnumber > NUM_SLOTS) {
    printf("\'%s\' is not a valid slot number.\n",slotname);
  }

  if (strcmp(setting, "gamepad") == 0) {
    printf("Setting slot %d to be a virtual gamepad.\n",slotnumber);
    state->slots[slotnumber].uinput_fd = state->slots[slotnumber].gamepad_fd;
    return 0;
  }

  if (strcmp(setting, "keyboardmouse") == 0) {
    printf("Setting slot %d to be a virtual keyboard/mouse combo.\n",slotnumber);
    state->slots[slotnumber].uinput_fd = state->virtual_keyboardmouse_fd;
    return 0;
  }

  printf("\'%s\' was not a recongnized setting.\n",setting);
  printf("(try \"gamepad\" or \"keyboardmouse\"\n");

  return -1;
}

int assign_device(struct wiimoteglue_state *state, char *devname, char *slotname) {
  if (devname == NULL || slotname == NULL) {
    printf("usage: assign <device name|device address> <slot number|\"keyboardmouse\"|\"none\">\n");
    return 0;
  }
  struct wii_device_list* device = lookup_device(&state->devlist,devname);
  if (device == NULL) {
    printf("\'%s\' did not match a device id or address.\n",devname);
    printf("Use \"list\" to see devices.\n");
    return -1;
  }
  struct virtual_controller* slot = NULL;
  int slotnumber = slotname[0] - '0'; /*HACK: single-digit atoi*/
  if (slotnumber >= 1 && slotnumber <= NUM_SLOTS) {
    slot = &state->slots[slotnumber];
  }
  if (strcmp(slotname,"keyboardmouse") == 0) {
    slot = &state->slots[0];
  }
  if (slot == NULL && strcmp(slotname,"none") != 0) {
    printf("\'%s\' was not a valid slot.\n",slotname);
    printf("Valid choices are keyboardmouse, ");
    int i;
    for (i = 1; i <= NUM_SLOTS; i++) {
      printf("%d, ",i);
    }
    printf("none\n");
    return -1;
  }

  if (device->type == REMOTE || device->type == PRO) {
    if (device->slot != NULL) {
    device->slot->has_wiimote--;
    }
    if (slot != NULL) {
      slot->has_wiimote++;
      if (slot->has_wiimote > 1)
	printf("Mutiple controllers assigned to a slot!\nSafety not guaranteed.\n");
    }
  }
  if (device->type == BALANCE) {
    if (device->slot != NULL) {
    device->slot->has_board--;
    }
    if (slot != NULL) {
      slot->has_board++;
      if (slot->has_board > 1)
	printf("Mutiple boards assigned to a slot!\nSafety not guaranteed.\n");
    }
  }
  device->slot = slot;

  return 0;

}




int list_devices(struct wii_device_list *devlist, char *option) {
  struct wii_device_list* dev = devlist;
  static char* wiimote = "Wii Remote";
  static char* pro = "Wii U Pro Controller";
  static char* board = "Balance Board";
  static char* unknown = "Unknown Device Type?";
  char* type = unknown;

  if (dev == NULL) {
    return -1;
  }
  if (dev->device != NULL) {
    if (dev->type == REMOTE)
      type = wiimote;
    if (dev->type == PRO)
      type = pro;
    if (dev->type == BALANCE)
      type = board;

    printf("- %s (%s)\n",dev->id,dev->bluetooth_addr);
    printf("\t%s\n",type);

    if (dev->slot != NULL) {
      if (dev->slot->slot_number != 0) {
	printf("\tAssigned to slot %d\n",dev->slot->slot_number);
      } else {
	printf("\tAssigned to virtual keyboard/mouse\n");
      }
    } else {
      printf("\tNot assigned to any slot");
    }

  }
  if (dev->next != NULL) {
    list_devices(dev->next,option);
  }
  return 0;
}

struct wii_device_list * lookup_device(struct wii_device_list *devlist, char *name) {

  if (devlist == NULL) {
    return NULL;
  }
  if (devlist->device != NULL) {
    if (strncmp(name,devlist->id,32) == 0)
      return devlist;
    if (strncmp(name,devlist->bluetooth_addr,18) == 0)
      return devlist;
  }

  return lookup_device(devlist->next,name);
}

int * get_input_key(char *key_name, int button_map[]) {
  if (key_name == NULL) return NULL;
  if (strcmp(key_name,"up") == 0) return &button_map[XWII_KEY_UP];
  if (strcmp(key_name,"down") == 0) return &button_map[XWII_KEY_DOWN];
  if (strcmp(key_name,"left") == 0) return &button_map[XWII_KEY_LEFT];
  if (strcmp(key_name,"right") == 0) return &button_map[XWII_KEY_RIGHT];
  if (strcmp(key_name,"a") == 0) return &button_map[XWII_KEY_A];
  if (strcmp(key_name,"b") == 0) return &button_map[XWII_KEY_B];
  if (strcmp(key_name,"c") == 0) return &button_map[XWII_KEY_C];
  if (strcmp(key_name,"x") == 0) return &button_map[XWII_KEY_X];
  if (strcmp(key_name,"y") == 0) return &button_map[XWII_KEY_Y];
  if (strcmp(key_name,"z") == 0) return &button_map[XWII_KEY_Z];
  if (strcmp(key_name,"plus") == 0) return &button_map[XWII_KEY_PLUS];
  if (strcmp(key_name,"minus") == 0) return &button_map[XWII_KEY_MINUS];
  if (strcmp(key_name,"home") == 0) return &button_map[XWII_KEY_HOME];
  if (strcmp(key_name,"1") == 0) return &button_map[XWII_KEY_ONE];
  if (strcmp(key_name,"2") == 0) return &button_map[XWII_KEY_TWO];
  if (strcmp(key_name,"l") == 0) return &button_map[XWII_KEY_TL];
  if (strcmp(key_name,"r") == 0) return &button_map[XWII_KEY_TR];
  if (strcmp(key_name,"zl") == 0) return &button_map[XWII_KEY_ZL];
  if (strcmp(key_name,"zr") == 0) return &button_map[XWII_KEY_ZR];
  if (strcmp(key_name,"thumbl") == 0) return &button_map[XWII_KEY_THUMBL];
  if (strcmp(key_name,"thumbr") == 0) return &button_map[XWII_KEY_THUMBR];
  return NULL;
}

int * get_input_axis(char *axis_name, struct event_map *map) {
  if (axis_name == NULL) return NULL;
  if (strcmp(axis_name,"accelx") == 0) return map->accel_map[0];
  if (strcmp(axis_name,"accely") == 0) return map->accel_map[1];
  if (strcmp(axis_name,"accelz") == 0) return map->accel_map[2];
  if (strcmp(axis_name,"n_accelx") == 0) return map->accel_map[3];
  if (strcmp(axis_name,"n_accely") == 0) return map->accel_map[4];
  if (strcmp(axis_name,"n_accelz") == 0) return map->accel_map[5];
  if (strcmp(axis_name,"left_x") == 0) return map->stick_map[0];
  if (strcmp(axis_name,"left_y") == 0) return map->stick_map[1];
  if (strcmp(axis_name,"right_x") == 0) return map->stick_map[2];
  if (strcmp(axis_name,"right_y") == 0) return map->stick_map[3];
  if (strcmp(axis_name,"n_x") == 0) return map->stick_map[4];
  if (strcmp(axis_name,"n_y") == 0) return map->stick_map[5];
  if (strcmp(axis_name,"ir_x") == 0) return map->IR_map[0];
  if (strcmp(axis_name,"ir_y") == 0) return map->IR_map[1];
  if (strcmp(axis_name,"bal_fl") == 0) return map->balance_map[0];
  if (strcmp(axis_name,"bal_fr") == 0) return map->balance_map[1];
  if (strcmp(axis_name,"bal_bl") == 0) return map->balance_map[2];
  if (strcmp(axis_name,"bal_br") == 0) return map->balance_map[3];
  if (strcmp(axis_name,"bal_x") == 0) return map->balance_map[4];
  if (strcmp(axis_name,"bal_y") == 0) return map->balance_map[5];

  return NULL;
}

int get_output_key(char *key_name) {
  if (key_name == NULL) return -2;
  if (strcmp(key_name,"up") == 0) return BTN_DPAD_UP;
  if (strcmp(key_name,"down") == 0) return BTN_DPAD_DOWN;
  if (strcmp(key_name,"left") == 0) return BTN_DPAD_LEFT;
  if (strcmp(key_name,"right") == 0) return BTN_DPAD_RIGHT;
  if (strcmp(key_name,"north") == 0) return BTN_NORTH;
  if (strcmp(key_name,"south") == 0) return BTN_SOUTH;
  if (strcmp(key_name,"east") == 0) return BTN_EAST;
  if (strcmp(key_name,"west") == 0) return BTN_WEST;
  if (strcmp(key_name,"start") == 0) return BTN_START;
  if (strcmp(key_name,"select") == 0) return BTN_SELECT;
  if (strcmp(key_name,"mode") == 0) return BTN_MODE;
  if (strcmp(key_name,"tr") == 0) return BTN_TR;
  if (strcmp(key_name,"tl") == 0) return BTN_TL;
  if (strcmp(key_name,"tr2") == 0) return BTN_TR2;
  if (strcmp(key_name,"tl2") == 0) return BTN_TL2;
  if (strcmp(key_name,"thumbr") == 0) return BTN_THUMBR;
  if (strcmp(key_name,"thumbl") == 0) return BTN_THUMBL;
  if (strcmp(key_name,"thumbl") == 0) return BTN_THUMBL;
  if (strcmp(key_name,"none") == 0) return NO_MAP;
  if (strcmp(key_name,"left_click") == 0) return BTN_LEFT;
  if (strcmp(key_name,"right_click") == 0) return BTN_RIGHT;
  if (strcmp(key_name,"middle_click") == 0) return BTN_MIDDLE;
  if (strcmp(key_name,"key_esc") == 0) return KEY_ESC;
  if (strcmp(key_name,"key_1") == 0) return KEY_1;
  if (strcmp(key_name,"key_2") == 0) return KEY_2;
  if (strcmp(key_name,"key_3") == 0) return KEY_3;
  if (strcmp(key_name,"key_4") == 0) return KEY_4;
  if (strcmp(key_name,"key_5") == 0) return KEY_5;
  if (strcmp(key_name,"key_6") == 0) return KEY_6;
  if (strcmp(key_name,"key_7") == 0) return KEY_7;
  if (strcmp(key_name,"key_8") == 0) return KEY_8;
  if (strcmp(key_name,"key_9") == 0) return KEY_9;
  if (strcmp(key_name,"key_0") == 0) return KEY_0;
  if (strcmp(key_name,"key_minus") == 0) return KEY_MINUS;
  if (strcmp(key_name,"key_equal") == 0) return KEY_EQUAL;
  if (strcmp(key_name,"key_backspace") == 0) return KEY_BACKSPACE;
  if (strcmp(key_name,"key_tab") == 0) return KEY_TAB;
  if (strcmp(key_name,"key_q") == 0) return KEY_Q;
  if (strcmp(key_name,"key_w") == 0) return KEY_W;
  if (strcmp(key_name,"key_e") == 0) return KEY_E;
  if (strcmp(key_name,"key_r") == 0) return KEY_R;
  if (strcmp(key_name,"key_t") == 0) return KEY_T;
  if (strcmp(key_name,"key_y") == 0) return KEY_Y;
  if (strcmp(key_name,"key_u") == 0) return KEY_U;
  if (strcmp(key_name,"key_i") == 0) return KEY_I;
  if (strcmp(key_name,"key_o") == 0) return KEY_O;
  if (strcmp(key_name,"key_p") == 0) return KEY_P;
  if (strcmp(key_name,"key_leftbrace") == 0) return KEY_LEFTBRACE;
  if (strcmp(key_name,"key_rightbrace") == 0) return KEY_RIGHTBRACE;
  if (strcmp(key_name,"key_enter") == 0) return KEY_ENTER;
  if (strcmp(key_name,"key_leftctrl") == 0) return KEY_LEFTCTRL;
  if (strcmp(key_name,"key_a") == 0) return KEY_A;
  if (strcmp(key_name,"key_s") == 0) return KEY_S;
  if (strcmp(key_name,"key_d") == 0) return KEY_D;
  if (strcmp(key_name,"key_f") == 0) return KEY_F;
  if (strcmp(key_name,"key_g") == 0) return KEY_G;
  if (strcmp(key_name,"key_h") == 0) return KEY_H;
  if (strcmp(key_name,"key_j") == 0) return KEY_J;
  if (strcmp(key_name,"key_k") == 0) return KEY_K;
  if (strcmp(key_name,"key_l") == 0) return KEY_L;
  if (strcmp(key_name,"key_semicolon") == 0) return KEY_SEMICOLON;
  if (strcmp(key_name,"key_apostrophe") == 0) return KEY_APOSTROPHE;
  if (strcmp(key_name,"key_grave") == 0) return KEY_GRAVE;
  if (strcmp(key_name,"key_leftshift") == 0) return KEY_LEFTSHIFT;
  if (strcmp(key_name,"key_backslash") == 0) return KEY_BACKSLASH;
  if (strcmp(key_name,"key_z") == 0) return KEY_Z;
  if (strcmp(key_name,"key_x") == 0) return KEY_X;
  if (strcmp(key_name,"key_c") == 0) return KEY_C;
  if (strcmp(key_name,"key_v") == 0) return KEY_V;
  if (strcmp(key_name,"key_b") == 0) return KEY_B;
  if (strcmp(key_name,"key_n") == 0) return KEY_N;
  if (strcmp(key_name,"key_m") == 0) return KEY_M;
  if (strcmp(key_name,"key_comma") == 0) return KEY_COMMA;
  if (strcmp(key_name,"key_dot") == 0) return KEY_DOT;
  if (strcmp(key_name,"key_slash") == 0) return KEY_SLASH;
  if (strcmp(key_name,"key_rightshift") == 0) return KEY_RIGHTSHIFT;
  if (strcmp(key_name,"key_kpasterisk") == 0) return KEY_KPASTERISK;
  if (strcmp(key_name,"key_leftalt") == 0) return KEY_LEFTALT;
  if (strcmp(key_name,"key_space") == 0) return KEY_SPACE;
  if (strcmp(key_name,"key_capslock") == 0) return KEY_CAPSLOCK;
  if (strcmp(key_name,"key_f1") == 0) return KEY_F1;
  if (strcmp(key_name,"key_f2") == 0) return KEY_F2;
  if (strcmp(key_name,"key_f3") == 0) return KEY_F3;
  if (strcmp(key_name,"key_f4") == 0) return KEY_F4;
  if (strcmp(key_name,"key_f5") == 0) return KEY_F5;
  if (strcmp(key_name,"key_f6") == 0) return KEY_F6;
  if (strcmp(key_name,"key_f7") == 0) return KEY_F7;
  if (strcmp(key_name,"key_f8") == 0) return KEY_F8;
  if (strcmp(key_name,"key_f9") == 0) return KEY_F9;
  if (strcmp(key_name,"key_f10") == 0) return KEY_F10;
  if (strcmp(key_name,"key_numlock") == 0) return KEY_NUMLOCK;
  if (strcmp(key_name,"key_scrolllock") == 0) return KEY_SCROLLLOCK;
  if (strcmp(key_name,"key_kp7") == 0) return KEY_KP7;
  if (strcmp(key_name,"key_kp8") == 0) return KEY_KP8;
  if (strcmp(key_name,"key_kp9") == 0) return KEY_KP9;
  if (strcmp(key_name,"key_kpminus") == 0) return KEY_KPMINUS;
  if (strcmp(key_name,"key_kp4") == 0) return KEY_KP4;
  if (strcmp(key_name,"key_kp5") == 0) return KEY_KP5;
  if (strcmp(key_name,"key_kp6") == 0) return KEY_KP6;
  if (strcmp(key_name,"key_kpplus") == 0) return KEY_KPPLUS;
  if (strcmp(key_name,"key_kp1") == 0) return KEY_KP1;
  if (strcmp(key_name,"key_kp2") == 0) return KEY_KP2;
  if (strcmp(key_name,"key_kp3") == 0) return KEY_KP3;
  if (strcmp(key_name,"key_kp0") == 0) return KEY_KP0;
  if (strcmp(key_name,"key_kpdot") == 0) return KEY_KPDOT;
  if (strcmp(key_name,"key_zenkakuhankaku") == 0) return KEY_ZENKAKUHANKAKU;
  if (strcmp(key_name,"key_102nd") == 0) return KEY_102ND;
  if (strcmp(key_name,"key_f11") == 0) return KEY_F11;
  if (strcmp(key_name,"key_f12") == 0) return KEY_F12;
  if (strcmp(key_name,"key_ro") == 0) return KEY_RO;
  if (strcmp(key_name,"key_katakana") == 0) return KEY_KATAKANA;
  if (strcmp(key_name,"key_hiragana") == 0) return KEY_HIRAGANA;
  if (strcmp(key_name,"key_henkan") == 0) return KEY_HENKAN;
  if (strcmp(key_name,"key_katakanahiragana") == 0) return KEY_KATAKANAHIRAGANA;
  if (strcmp(key_name,"key_muhenkan") == 0) return KEY_MUHENKAN;
  if (strcmp(key_name,"key_kpjpcomma") == 0) return KEY_KPJPCOMMA;
  if (strcmp(key_name,"key_kpenter") == 0) return KEY_KPENTER;
  if (strcmp(key_name,"key_rightctrl") == 0) return KEY_RIGHTCTRL;
  if (strcmp(key_name,"key_kpslash") == 0) return KEY_KPSLASH;
  if (strcmp(key_name,"key_sysrq") == 0) return KEY_SYSRQ;
  if (strcmp(key_name,"key_rightalt") == 0) return KEY_RIGHTALT;
  if (strcmp(key_name,"key_linefeed") == 0) return KEY_LINEFEED;
  if (strcmp(key_name,"key_home") == 0) return KEY_HOME;
  if (strcmp(key_name,"key_up") == 0) return KEY_UP;
  if (strcmp(key_name,"key_pageup") == 0) return KEY_PAGEUP;
  if (strcmp(key_name,"key_left") == 0) return KEY_LEFT;
  if (strcmp(key_name,"key_right") == 0) return KEY_RIGHT;
  if (strcmp(key_name,"key_end") == 0) return KEY_END;
  if (strcmp(key_name,"key_down") == 0) return KEY_DOWN;
  if (strcmp(key_name,"key_pagedown") == 0) return KEY_PAGEDOWN;
  if (strcmp(key_name,"key_insert") == 0) return KEY_INSERT;
  if (strcmp(key_name,"key_delete") == 0) return KEY_DELETE;
  if (strcmp(key_name,"key_macro") == 0) return KEY_MACRO;
  if (strcmp(key_name,"key_mute") == 0) return KEY_MUTE;
  if (strcmp(key_name,"key_volumedown") == 0) return KEY_VOLUMEDOWN;
  if (strcmp(key_name,"key_volumeup") == 0) return KEY_VOLUMEUP;
  if (strcmp(key_name,"key_power") == 0) return KEY_POWER;
  if (strcmp(key_name,"key_kpequal") == 0) return KEY_KPEQUAL;
  if (strcmp(key_name,"key_kpplusminus") == 0) return KEY_KPPLUSMINUS;
  if (strcmp(key_name,"key_pause") == 0) return KEY_PAUSE;
  if (strcmp(key_name,"key_scale") == 0) return KEY_SCALE;
  if (strcmp(key_name,"key_kpcomma") == 0) return KEY_KPCOMMA;
  if (strcmp(key_name,"key_hangeul") == 0) return KEY_HANGEUL;
  if (strcmp(key_name,"key_hanguel") == 0) return KEY_HANGUEL;
  if (strcmp(key_name,"key_hangeul") == 0) return KEY_HANGEUL;
  if (strcmp(key_name,"key_hanja") == 0) return KEY_HANJA;
  if (strcmp(key_name,"key_yen") == 0) return KEY_YEN;
  if (strcmp(key_name,"key_leftmeta") == 0) return KEY_LEFTMETA;
  if (strcmp(key_name,"key_rightmeta") == 0) return KEY_RIGHTMETA;
  if (strcmp(key_name,"key_compose") == 0) return KEY_COMPOSE;
  if (strcmp(key_name,"key_stop") == 0) return KEY_STOP;
  if (strcmp(key_name,"key_again") == 0) return KEY_AGAIN;
  if (strcmp(key_name,"key_props") == 0) return KEY_PROPS;
  if (strcmp(key_name,"key_undo") == 0) return KEY_UNDO;
  if (strcmp(key_name,"key_front") == 0) return KEY_FRONT;
  if (strcmp(key_name,"key_copy") == 0) return KEY_COPY;
  if (strcmp(key_name,"key_open") == 0) return KEY_OPEN;
  if (strcmp(key_name,"key_paste") == 0) return KEY_PASTE;
  if (strcmp(key_name,"key_find") == 0) return KEY_FIND;
  if (strcmp(key_name,"key_cut") == 0) return KEY_CUT;
  if (strcmp(key_name,"key_help") == 0) return KEY_HELP;
  if (strcmp(key_name,"key_menu") == 0) return KEY_MENU;
  if (strcmp(key_name,"key_calc") == 0) return KEY_CALC;
  if (strcmp(key_name,"key_setup") == 0) return KEY_SETUP;
  if (strcmp(key_name,"key_sleep") == 0) return KEY_SLEEP;
  if (strcmp(key_name,"key_wakeup") == 0) return KEY_WAKEUP;
  if (strcmp(key_name,"key_file") == 0) return KEY_FILE;
  if (strcmp(key_name,"key_sendfile") == 0) return KEY_SENDFILE;
  if (strcmp(key_name,"key_deletefile") == 0) return KEY_DELETEFILE;
  if (strcmp(key_name,"key_xfer") == 0) return KEY_XFER;
  if (strcmp(key_name,"key_prog1") == 0) return KEY_PROG1;
  if (strcmp(key_name,"key_prog2") == 0) return KEY_PROG2;
  if (strcmp(key_name,"key_www") == 0) return KEY_WWW;
  if (strcmp(key_name,"key_msdos") == 0) return KEY_MSDOS;
  if (strcmp(key_name,"key_coffee") == 0) return KEY_COFFEE;
  if (strcmp(key_name,"key_screenlock") == 0) return KEY_SCREENLOCK;
  if (strcmp(key_name,"key_coffee") == 0) return KEY_COFFEE;
  if (strcmp(key_name,"key_direction") == 0) return KEY_DIRECTION;
  if (strcmp(key_name,"key_cyclewindows") == 0) return KEY_CYCLEWINDOWS;
  if (strcmp(key_name,"key_mail") == 0) return KEY_MAIL;
  if (strcmp(key_name,"key_bookmarks") == 0) return KEY_BOOKMARKS;
  if (strcmp(key_name,"key_computer") == 0) return KEY_COMPUTER;
  if (strcmp(key_name,"key_back") == 0) return KEY_BACK;
  if (strcmp(key_name,"key_forward") == 0) return KEY_FORWARD;
  if (strcmp(key_name,"key_closecd") == 0) return KEY_CLOSECD;
  if (strcmp(key_name,"key_ejectcd") == 0) return KEY_EJECTCD;
  if (strcmp(key_name,"key_ejectclosecd") == 0) return KEY_EJECTCLOSECD;
  if (strcmp(key_name,"key_nextsong") == 0) return KEY_NEXTSONG;
  if (strcmp(key_name,"key_playpause") == 0) return KEY_PLAYPAUSE;
  if (strcmp(key_name,"key_previoussong") == 0) return KEY_PREVIOUSSONG;
  if (strcmp(key_name,"key_stopcd") == 0) return KEY_STOPCD;
  if (strcmp(key_name,"key_record") == 0) return KEY_RECORD;
  if (strcmp(key_name,"key_rewind") == 0) return KEY_REWIND;
  if (strcmp(key_name,"key_phone") == 0) return KEY_PHONE;
  if (strcmp(key_name,"key_iso") == 0) return KEY_ISO;
  if (strcmp(key_name,"key_config") == 0) return KEY_CONFIG;
  if (strcmp(key_name,"key_homepage") == 0) return KEY_HOMEPAGE;
  if (strcmp(key_name,"key_refresh") == 0) return KEY_REFRESH;
  if (strcmp(key_name,"key_exit") == 0) return KEY_EXIT;
  if (strcmp(key_name,"key_move") == 0) return KEY_MOVE;
  if (strcmp(key_name,"key_edit") == 0) return KEY_EDIT;
  if (strcmp(key_name,"key_scrollup") == 0) return KEY_SCROLLUP;
  if (strcmp(key_name,"key_scrolldown") == 0) return KEY_SCROLLDOWN;
  if (strcmp(key_name,"key_kpleftparen") == 0) return KEY_KPLEFTPAREN;
  if (strcmp(key_name,"key_kprightparen") == 0) return KEY_KPRIGHTPAREN;
  if (strcmp(key_name,"key_new") == 0) return KEY_NEW;
  if (strcmp(key_name,"key_redo") == 0) return KEY_REDO;
  if (strcmp(key_name,"key_f13") == 0) return KEY_F13;
  if (strcmp(key_name,"key_f14") == 0) return KEY_F14;
  if (strcmp(key_name,"key_f15") == 0) return KEY_F15;
  if (strcmp(key_name,"key_f16") == 0) return KEY_F16;
  if (strcmp(key_name,"key_f17") == 0) return KEY_F17;
  if (strcmp(key_name,"key_f18") == 0) return KEY_F18;
  if (strcmp(key_name,"key_f19") == 0) return KEY_F19;
  if (strcmp(key_name,"key_f20") == 0) return KEY_F20;
  if (strcmp(key_name,"key_f21") == 0) return KEY_F21;
  if (strcmp(key_name,"key_f22") == 0) return KEY_F22;
  if (strcmp(key_name,"key_f23") == 0) return KEY_F23;
  if (strcmp(key_name,"key_f24") == 0) return KEY_F24;
  if (strcmp(key_name,"key_playcd") == 0) return KEY_PLAYCD;
  if (strcmp(key_name,"key_pausecd") == 0) return KEY_PAUSECD;
  if (strcmp(key_name,"key_prog3") == 0) return KEY_PROG3;
  if (strcmp(key_name,"key_prog4") == 0) return KEY_PROG4;
  if (strcmp(key_name,"key_dashboard") == 0) return KEY_DASHBOARD;
  if (strcmp(key_name,"key_suspend") == 0) return KEY_SUSPEND;
  if (strcmp(key_name,"key_close") == 0) return KEY_CLOSE;
  if (strcmp(key_name,"key_play") == 0) return KEY_PLAY;
  if (strcmp(key_name,"key_fastforward") == 0) return KEY_FASTFORWARD;
  if (strcmp(key_name,"key_bassboost") == 0) return KEY_BASSBOOST;
  if (strcmp(key_name,"key_print") == 0) return KEY_PRINT;
  if (strcmp(key_name,"key_hp") == 0) return KEY_HP;
  if (strcmp(key_name,"key_camera") == 0) return KEY_CAMERA;
  if (strcmp(key_name,"key_sound") == 0) return KEY_SOUND;
  if (strcmp(key_name,"key_question") == 0) return KEY_QUESTION;
  if (strcmp(key_name,"key_email") == 0) return KEY_EMAIL;
  if (strcmp(key_name,"key_chat") == 0) return KEY_CHAT;
  if (strcmp(key_name,"key_search") == 0) return KEY_SEARCH;
  if (strcmp(key_name,"key_connect") == 0) return KEY_CONNECT;
  if (strcmp(key_name,"key_finance") == 0) return KEY_FINANCE;
  if (strcmp(key_name,"key_sport") == 0) return KEY_SPORT;
  if (strcmp(key_name,"key_shop") == 0) return KEY_SHOP;
  if (strcmp(key_name,"key_alterase") == 0) return KEY_ALTERASE;
  if (strcmp(key_name,"key_cancel") == 0) return KEY_CANCEL;
  if (strcmp(key_name,"key_brightnessdown") == 0) return KEY_BRIGHTNESSDOWN;
  if (strcmp(key_name,"key_brightnessup") == 0) return KEY_BRIGHTNESSUP;
  if (strcmp(key_name,"key_media") == 0) return KEY_MEDIA;
  if (strcmp(key_name,"key_switchvideomode") == 0) return KEY_SWITCHVIDEOMODE;
  if (strcmp(key_name,"key_kbdillumtoggle") == 0) return KEY_KBDILLUMTOGGLE;
  if (strcmp(key_name,"key_kbdillumdown") == 0) return KEY_KBDILLUMDOWN;
  if (strcmp(key_name,"key_kbdillumup") == 0) return KEY_KBDILLUMUP;
  if (strcmp(key_name,"key_send") == 0) return KEY_SEND;
  if (strcmp(key_name,"key_reply") == 0) return KEY_REPLY;
  if (strcmp(key_name,"key_forwardmail") == 0) return KEY_FORWARDMAIL;
  if (strcmp(key_name,"key_save") == 0) return KEY_SAVE;
  if (strcmp(key_name,"key_documents") == 0) return KEY_DOCUMENTS;
  if (strcmp(key_name,"key_battery") == 0) return KEY_BATTERY;
  if (strcmp(key_name,"key_bluetooth") == 0) return KEY_BLUETOOTH;
  if (strcmp(key_name,"key_wlan") == 0) return KEY_WLAN;
  if (strcmp(key_name,"key_uwb") == 0) return KEY_UWB;
  if (strcmp(key_name,"key_unknown") == 0) return KEY_UNKNOWN;
  if (strcmp(key_name,"key_video_next") == 0) return KEY_VIDEO_NEXT;
  if (strcmp(key_name,"key_video_prev") == 0) return KEY_VIDEO_PREV;
  if (strcmp(key_name,"key_brightness_cycle") == 0) return KEY_BRIGHTNESS_CYCLE;
  if (strcmp(key_name,"key_brightness_auto") == 0) return KEY_BRIGHTNESS_AUTO;
  if (strcmp(key_name,"key_brightness_zero") == 0) return KEY_BRIGHTNESS_ZERO;
  if (strcmp(key_name,"key_brightness_auto") == 0) return KEY_BRIGHTNESS_AUTO;
  if (strcmp(key_name,"key_display_off") == 0) return KEY_DISPLAY_OFF;
  if (strcmp(key_name,"key_wwan") == 0) return KEY_WWAN;
  if (strcmp(key_name,"key_wimax") == 0) return KEY_WIMAX;
  if (strcmp(key_name,"key_wwan") == 0) return KEY_WWAN;
  if (strcmp(key_name,"key_rfkill") == 0) return KEY_RFKILL;
  if (strcmp(key_name,"key_micmute") == 0) return KEY_MICMUTE;
  if (strcmp(key_name,"key_ok") == 0) return KEY_OK;
  if (strcmp(key_name,"key_select") == 0) return KEY_SELECT;
  if (strcmp(key_name,"key_goto") == 0) return KEY_GOTO;
  if (strcmp(key_name,"key_clear") == 0) return KEY_CLEAR;
  if (strcmp(key_name,"key_power2") == 0) return KEY_POWER2;
  if (strcmp(key_name,"key_option") == 0) return KEY_OPTION;
  if (strcmp(key_name,"key_info") == 0) return KEY_INFO;
  if (strcmp(key_name,"key_time") == 0) return KEY_TIME;
  if (strcmp(key_name,"key_vendor") == 0) return KEY_VENDOR;
  if (strcmp(key_name,"key_archive") == 0) return KEY_ARCHIVE;
  if (strcmp(key_name,"key_program") == 0) return KEY_PROGRAM;
  if (strcmp(key_name,"key_channel") == 0) return KEY_CHANNEL;
  if (strcmp(key_name,"key_favorites") == 0) return KEY_FAVORITES;
  if (strcmp(key_name,"key_epg") == 0) return KEY_EPG;
  if (strcmp(key_name,"key_pvr") == 0) return KEY_PVR;
  if (strcmp(key_name,"key_mhp") == 0) return KEY_MHP;
  if (strcmp(key_name,"key_language") == 0) return KEY_LANGUAGE;
  if (strcmp(key_name,"key_title") == 0) return KEY_TITLE;
  if (strcmp(key_name,"key_subtitle") == 0) return KEY_SUBTITLE;
  if (strcmp(key_name,"key_angle") == 0) return KEY_ANGLE;
  if (strcmp(key_name,"key_zoom") == 0) return KEY_ZOOM;
  if (strcmp(key_name,"key_mode") == 0) return KEY_MODE;
  if (strcmp(key_name,"key_keyboard") == 0) return KEY_KEYBOARD;
  if (strcmp(key_name,"key_screen") == 0) return KEY_SCREEN;
  if (strcmp(key_name,"key_pc") == 0) return KEY_PC;
  if (strcmp(key_name,"key_tv") == 0) return KEY_TV;
  if (strcmp(key_name,"key_tv2") == 0) return KEY_TV2;
  if (strcmp(key_name,"key_vcr") == 0) return KEY_VCR;
  if (strcmp(key_name,"key_vcr2") == 0) return KEY_VCR2;
  if (strcmp(key_name,"key_sat") == 0) return KEY_SAT;
  if (strcmp(key_name,"key_sat2") == 0) return KEY_SAT2;
  if (strcmp(key_name,"key_cd") == 0) return KEY_CD;
  if (strcmp(key_name,"key_tape") == 0) return KEY_TAPE;
  if (strcmp(key_name,"key_radio") == 0) return KEY_RADIO;
  if (strcmp(key_name,"key_tuner") == 0) return KEY_TUNER;
  if (strcmp(key_name,"key_player") == 0) return KEY_PLAYER;
  if (strcmp(key_name,"key_text") == 0) return KEY_TEXT;
  if (strcmp(key_name,"key_dvd") == 0) return KEY_DVD;
  if (strcmp(key_name,"key_aux") == 0) return KEY_AUX;
  if (strcmp(key_name,"key_mp3") == 0) return KEY_MP3;
  if (strcmp(key_name,"key_audio") == 0) return KEY_AUDIO;
  if (strcmp(key_name,"key_video") == 0) return KEY_VIDEO;
  if (strcmp(key_name,"key_directory") == 0) return KEY_DIRECTORY;
  if (strcmp(key_name,"key_list") == 0) return KEY_LIST;
  if (strcmp(key_name,"key_memo") == 0) return KEY_MEMO;
  if (strcmp(key_name,"key_calendar") == 0) return KEY_CALENDAR;
  if (strcmp(key_name,"key_red") == 0) return KEY_RED;
  if (strcmp(key_name,"key_green") == 0) return KEY_GREEN;
  if (strcmp(key_name,"key_yellow") == 0) return KEY_YELLOW;
  if (strcmp(key_name,"key_blue") == 0) return KEY_BLUE;
  if (strcmp(key_name,"key_channelup") == 0) return KEY_CHANNELUP;
  if (strcmp(key_name,"key_channeldown") == 0) return KEY_CHANNELDOWN;
  if (strcmp(key_name,"key_first") == 0) return KEY_FIRST;
  if (strcmp(key_name,"key_last") == 0) return KEY_LAST;
  if (strcmp(key_name,"key_ab") == 0) return KEY_AB;
  if (strcmp(key_name,"key_next") == 0) return KEY_NEXT;
  if (strcmp(key_name,"key_restart") == 0) return KEY_RESTART;
  if (strcmp(key_name,"key_slow") == 0) return KEY_SLOW;
  if (strcmp(key_name,"key_shuffle") == 0) return KEY_SHUFFLE;
  if (strcmp(key_name,"key_break") == 0) return KEY_BREAK;
  if (strcmp(key_name,"key_previous") == 0) return KEY_PREVIOUS;
  if (strcmp(key_name,"key_digits") == 0) return KEY_DIGITS;
  if (strcmp(key_name,"key_teen") == 0) return KEY_TEEN;
  if (strcmp(key_name,"key_twen") == 0) return KEY_TWEN;
  if (strcmp(key_name,"key_videophone") == 0) return KEY_VIDEOPHONE;
  if (strcmp(key_name,"key_games") == 0) return KEY_GAMES;
  if (strcmp(key_name,"key_zoomin") == 0) return KEY_ZOOMIN;
  if (strcmp(key_name,"key_zoomout") == 0) return KEY_ZOOMOUT;
  if (strcmp(key_name,"key_zoomreset") == 0) return KEY_ZOOMRESET;
  if (strcmp(key_name,"key_wordprocessor") == 0) return KEY_WORDPROCESSOR;
  if (strcmp(key_name,"key_editor") == 0) return KEY_EDITOR;
  if (strcmp(key_name,"key_spreadsheet") == 0) return KEY_SPREADSHEET;
  if (strcmp(key_name,"key_graphicseditor") == 0) return KEY_GRAPHICSEDITOR;
  if (strcmp(key_name,"key_presentation") == 0) return KEY_PRESENTATION;
  if (strcmp(key_name,"key_database") == 0) return KEY_DATABASE;
  if (strcmp(key_name,"key_news") == 0) return KEY_NEWS;
  if (strcmp(key_name,"key_voicemail") == 0) return KEY_VOICEMAIL;
  if (strcmp(key_name,"key_addressbook") == 0) return KEY_ADDRESSBOOK;
  if (strcmp(key_name,"key_messenger") == 0) return KEY_MESSENGER;
  if (strcmp(key_name,"key_displaytoggle") == 0) return KEY_DISPLAYTOGGLE;
  if (strcmp(key_name,"key_brightness_toggle") == 0) return KEY_BRIGHTNESS_TOGGLE;
  if (strcmp(key_name,"key_displaytoggle") == 0) return KEY_DISPLAYTOGGLE;
  if (strcmp(key_name,"key_spellcheck") == 0) return KEY_SPELLCHECK;
  if (strcmp(key_name,"key_logoff") == 0) return KEY_LOGOFF;
  if (strcmp(key_name,"key_dollar") == 0) return KEY_DOLLAR;
  if (strcmp(key_name,"key_euro") == 0) return KEY_EURO;
  if (strcmp(key_name,"key_frameback") == 0) return KEY_FRAMEBACK;
  if (strcmp(key_name,"key_frameforward") == 0) return KEY_FRAMEFORWARD;
  if (strcmp(key_name,"key_context_menu") == 0) return KEY_CONTEXT_MENU;
  if (strcmp(key_name,"key_media_repeat") == 0) return KEY_MEDIA_REPEAT;
  if (strcmp(key_name,"key_10channelsup") == 0) return KEY_10CHANNELSUP;
  if (strcmp(key_name,"key_10channelsdown") == 0) return KEY_10CHANNELSDOWN;
  if (strcmp(key_name,"key_images") == 0) return KEY_IMAGES;
  if (strcmp(key_name,"key_del_eol") == 0) return KEY_DEL_EOL;
  if (strcmp(key_name,"key_del_eos") == 0) return KEY_DEL_EOS;
  if (strcmp(key_name,"key_ins_line") == 0) return KEY_INS_LINE;
  if (strcmp(key_name,"key_del_line") == 0) return KEY_DEL_LINE;
  if (strcmp(key_name,"key_fn") == 0) return KEY_FN;
  if (strcmp(key_name,"key_fn_esc") == 0) return KEY_FN_ESC;
  if (strcmp(key_name,"key_fn_f1") == 0) return KEY_FN_F1;
  if (strcmp(key_name,"key_fn_f2") == 0) return KEY_FN_F2;
  if (strcmp(key_name,"key_fn_f3") == 0) return KEY_FN_F3;
  if (strcmp(key_name,"key_fn_f4") == 0) return KEY_FN_F4;
  if (strcmp(key_name,"key_fn_f5") == 0) return KEY_FN_F5;
  if (strcmp(key_name,"key_fn_f6") == 0) return KEY_FN_F6;
  if (strcmp(key_name,"key_fn_f7") == 0) return KEY_FN_F7;
  if (strcmp(key_name,"key_fn_f8") == 0) return KEY_FN_F8;
  if (strcmp(key_name,"key_fn_f9") == 0) return KEY_FN_F9;
  if (strcmp(key_name,"key_fn_f10") == 0) return KEY_FN_F10;
  if (strcmp(key_name,"key_fn_f11") == 0) return KEY_FN_F11;
  if (strcmp(key_name,"key_fn_f12") == 0) return KEY_FN_F12;
  if (strcmp(key_name,"key_fn_1") == 0) return KEY_FN_1;
  if (strcmp(key_name,"key_fn_2") == 0) return KEY_FN_2;
  if (strcmp(key_name,"key_fn_d") == 0) return KEY_FN_D;
  if (strcmp(key_name,"key_fn_e") == 0) return KEY_FN_E;
  if (strcmp(key_name,"key_fn_f") == 0) return KEY_FN_F;
  if (strcmp(key_name,"key_fn_s") == 0) return KEY_FN_S;
  if (strcmp(key_name,"key_fn_b") == 0) return KEY_FN_B;
  if (strcmp(key_name,"key_brl_dot1") == 0) return KEY_BRL_DOT1;
  if (strcmp(key_name,"key_brl_dot2") == 0) return KEY_BRL_DOT2;
  if (strcmp(key_name,"key_brl_dot3") == 0) return KEY_BRL_DOT3;
  if (strcmp(key_name,"key_brl_dot4") == 0) return KEY_BRL_DOT4;
  if (strcmp(key_name,"key_brl_dot5") == 0) return KEY_BRL_DOT5;
  if (strcmp(key_name,"key_brl_dot6") == 0) return KEY_BRL_DOT6;
  if (strcmp(key_name,"key_brl_dot7") == 0) return KEY_BRL_DOT7;
  if (strcmp(key_name,"key_brl_dot8") == 0) return KEY_BRL_DOT8;
  if (strcmp(key_name,"key_brl_dot9") == 0) return KEY_BRL_DOT9;
  if (strcmp(key_name,"key_brl_dot10") == 0) return KEY_BRL_DOT10;
  if (strcmp(key_name,"key_numeric_0") == 0) return KEY_NUMERIC_0;
  if (strcmp(key_name,"key_numeric_1") == 0) return KEY_NUMERIC_1;
  if (strcmp(key_name,"key_numeric_2") == 0) return KEY_NUMERIC_2;
  if (strcmp(key_name,"key_numeric_3") == 0) return KEY_NUMERIC_3;
  if (strcmp(key_name,"key_numeric_4") == 0) return KEY_NUMERIC_4;
  if (strcmp(key_name,"key_numeric_5") == 0) return KEY_NUMERIC_5;
  if (strcmp(key_name,"key_numeric_6") == 0) return KEY_NUMERIC_6;
  if (strcmp(key_name,"key_numeric_7") == 0) return KEY_NUMERIC_7;
  if (strcmp(key_name,"key_numeric_8") == 0) return KEY_NUMERIC_8;
  if (strcmp(key_name,"key_numeric_9") == 0) return KEY_NUMERIC_9;
  if (strcmp(key_name,"key_numeric_star") == 0) return KEY_NUMERIC_STAR;
  if (strcmp(key_name,"key_numeric_pound") == 0) return KEY_NUMERIC_POUND;
  if (strcmp(key_name,"key_camera_focus") == 0) return KEY_CAMERA_FOCUS;
  if (strcmp(key_name,"key_wps_button") == 0) return KEY_WPS_BUTTON;
  if (strcmp(key_name,"key_touchpad_toggle") == 0) return KEY_TOUCHPAD_TOGGLE;
  if (strcmp(key_name,"key_touchpad_on") == 0) return KEY_TOUCHPAD_ON;
  if (strcmp(key_name,"key_touchpad_off") == 0) return KEY_TOUCHPAD_OFF;
  if (strcmp(key_name,"key_camera_zoomin") == 0) return KEY_CAMERA_ZOOMIN;
  if (strcmp(key_name,"key_camera_zoomout") == 0) return KEY_CAMERA_ZOOMOUT;
  if (strcmp(key_name,"key_camera_up") == 0) return KEY_CAMERA_UP;
  if (strcmp(key_name,"key_camera_down") == 0) return KEY_CAMERA_DOWN;
  if (strcmp(key_name,"key_camera_left") == 0) return KEY_CAMERA_LEFT;
  if (strcmp(key_name,"key_camera_right") == 0) return KEY_CAMERA_RIGHT;
  if (strcmp(key_name,"key_attendant_on") == 0) return KEY_ATTENDANT_ON;
  if (strcmp(key_name,"key_attendant_off") == 0) return KEY_ATTENDANT_OFF;
  if (strcmp(key_name,"key_attendant_toggle") == 0) return KEY_ATTENDANT_TOGGLE;
  if (strcmp(key_name,"key_lights_toggle") == 0) return KEY_LIGHTS_TOGGLE;
  if (strcmp(key_name,"key_als_toggle") == 0) return KEY_ALS_TOGGLE;
  if (strcmp(key_name,"key_buttonconfig") == 0) return KEY_BUTTONCONFIG;
  if (strcmp(key_name,"key_taskmanager") == 0) return KEY_TASKMANAGER;
  if (strcmp(key_name,"key_journal") == 0) return KEY_JOURNAL;
  if (strcmp(key_name,"key_controlpanel") == 0) return KEY_CONTROLPANEL;
  if (strcmp(key_name,"key_appselect") == 0) return KEY_APPSELECT;
  if (strcmp(key_name,"key_screensaver") == 0) return KEY_SCREENSAVER;
  if (strcmp(key_name,"key_voicecommand") == 0) return KEY_VOICECOMMAND;
  if (strcmp(key_name,"key_brightness_min") == 0) return KEY_BRIGHTNESS_MIN;
  if (strcmp(key_name,"key_brightness_max") == 0) return KEY_BRIGHTNESS_MAX;
  if (strcmp(key_name,"key_kbdinputassist_prev") == 0) return KEY_KBDINPUTASSIST_PREV;
  if (strcmp(key_name,"key_kbdinputassist_next") == 0) return KEY_KBDINPUTASSIST_NEXT;
  if (strcmp(key_name,"key_kbdinputassist_prevgroup") == 0) return KEY_KBDINPUTASSIST_PREVGROUP;
  if (strcmp(key_name,"key_kbdinputassist_nextgroup") == 0) return KEY_KBDINPUTASSIST_NEXTGROUP;
  if (strcmp(key_name,"key_kbdinputassist_accept") == 0) return KEY_KBDINPUTASSIST_ACCEPT;
  if (strcmp(key_name,"key_kbdinputassist_cancel") == 0) return KEY_KBDINPUTASSIST_CANCEL;


  return -2;
}

int get_output_axis(char *axis_name) {
  if (axis_name == NULL) return -2;
  if (strcmp(axis_name,"left_x") == 0) return ABS_X;
  if (strcmp(axis_name,"left_y") == 0) return ABS_Y;
  if (strcmp(axis_name,"right_x") == 0) return ABS_RX;
  if (strcmp(axis_name,"right_y") == 0) return ABS_RY;
  if (strcmp(axis_name,"mouse_x") == 0) return ABS_X;
  if (strcmp(axis_name,"mouse_y") == 0) return ABS_Y;
  if (strcmp(axis_name,"none") == 0) return NO_MAP;

  return -2;
}


