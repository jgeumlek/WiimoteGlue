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
    printf("\tassign - change a button/axis mapping in one of the modes\n");
    printf("\tenable/disable - control extra controller features\n");
    printf("\tload - opens a file and runs the commands inside.\n");
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
    printf("\tbal_fl,bal_fr,bal_bl,bal_br - balance board front/back left/right axes\n");
    printf("\tbal_x,bal_y - balance board center-of-gravity axes.\n");
    printf("\n");

    printf("The recognized names for the synthetic gamepad output buttons are:\n");
    printf("up, down, left, right, north, south, east, west, start, select, mode, tl, tr, tl2, tr2, thumbl, thumbr, none\n");
    printf("(See the Linux gamepad API for these definitions. North/south/etc. refer to the action face buttons.)\n\n");

    printf("The recognized names for the output axes are:\n");
    printf("left_x, left_y - left stick axes\n");
    printf("right_x, right_y - right stick axes\n");
    printf("none - an ignored axis\n");

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
  if (strcmp(args[0],"assign") == 0) {
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


  printf("Command not recognized.\n");
}

void update_mapping(struct wiimoteglue_state *state, char *mode, char *in, char *out, char *opt) {
  struct event_map *mapping = NULL;

  if (mode == NULL || in == NULL || out == NULL) {
    printf("Invalid command format.\n");
    printf("usage: assign <mode> <wii input> <gamepad output> [invert]\n");
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
  printf("usage: assign <mode> <wii input> <gamepad output> [invert]\n");
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

  return -2;
}

int get_output_axis(char *axis_name) {
  if (axis_name == NULL) return -2;
  if (strcmp(axis_name,"left_x") == 0) return ABS_X;
  if (strcmp(axis_name,"left_y") == 0) return ABS_Y;
  if (strcmp(axis_name,"right_x") == 0) return ABS_RX;
  if (strcmp(axis_name,"right_y") == 0) return ABS_RY;
  if (strcmp(axis_name,"none") == 0) return NO_MAP;

  return -2;
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
