#include <sys/epoll.h>
#include <string.h>
#include <stdio.h>

#include "wiimoteglue.h"

/* We use epoll to wait for the next input to handle.
 * This should prevent using unecessary CPU time.
 */

#define EPOLL_MAX_EVENTS 10
struct epoll_event event;
struct epoll_event events[EPOLL_MAX_EVENTS];

int wiimoteglue_epoll_init(int *epfd) {
  *epfd = epoll_create(EPOLL_MAX_EVENTS);

  memset(events,0, sizeof(events));

  return 0;
}


int wiimoteglue_epoll_watch_monitor(int epfd, int mon_fd, void *monitor) {
  memset(&event, 0, sizeof(event));

  event.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
  event.data.ptr = monitor;

  return epoll_ctl(epfd, EPOLL_CTL_ADD, mon_fd, &event);

}

int wiimoteglue_epoll_watch_stdin(struct wiimoteglue_state* state, int epfd) {
  memset(&event, 0, sizeof(event));

  event.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
  event.data.ptr = state;

  return epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &event); //HACK magic constant
}

int wiimoteglue_epoll_watch_wiimote(int epfd, struct wii_device *device) {
  if (device == NULL) return 0; //TODO: ERROR HANDLING.
  memset(&event, 0, sizeof(event));

  event.events = EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP;
  event.data.ptr = device;
  return epoll_ctl(epfd, EPOLL_CTL_ADD, device->fd, &event);
}

void wiimoteglue_epoll_loop(int epfd, struct wiimoteglue_state *state) {
  int n;
  int i;

  while (state->keep_looping > 0) {
    n = epoll_wait(epfd, events, EPOLL_MAX_EVENTS, -1);
    for (i = 0; i < n; i++) {
      if (events[i].data.ptr == state->monitor) {
	//HANDLE UDEV STUFF
	wiimoteglue_udev_handle_event(state);
	printf("\n>>");
	fflush(stdout);
      } else if (events[i].data.ptr == state) {
	//HANDLE USER INPUT
	state->load_lines = 0;
	int ret = wiimoteglue_handle_input(state,0);
        if (ret < 0)
          close(0);
	printf("\n>>");
	fflush(stdout);
      } else {
	//HANDLE WII STUFF
	wiimoteglue_handle_wii_event(state,events[i].data.ptr);
      }
    }
  }

}

