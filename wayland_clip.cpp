// Minimal Wayland clipboard paste example
// Created by Martin Stransky <stransky@redhat.com>
// Thanks to: Jonas Adahl -at- redhat.com
//            examples from https://jan.newmarch.name/Wayland/Input/
//
// build by gcc -o wayland_clip -g -O0 -fpermissive wayland_clip.cpp `pkg-config --libs --cflags gtk+-wayland-3.0` -lwayland-client
// run with WAYLAND_DEBUG=1 to see debug logs

#include <poll.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <wayland-client.h>
#include <errno.h>

struct wl_display *display;
struct wl_registry *registry;
int data_device_manager_version;
struct wl_data_device_manager *data_device_manager;
struct wl_event_queue *queue;
struct wl_data_device *data_device;
struct wl_seat *seat;
struct wl_data_offer *global_offer = NULL;
struct wl_keyboard *keyboard;

static void destroy( GtkWidget *widget,
                     gpointer   data )
{
    gtk_main_quit ();
}


static void
data_device_selection (void                  *data,
                       struct wl_data_device *wl_data_device,
                       struct wl_data_offer  *offer)
{
  fprintf (stderr, "data_device_selection, offer %p\n", offer);
  if(global_offer) {
    wl_data_offer_destroy(global_offer);
    global_offer = nullptr;
  }

  global_offer = offer;
}
  

static void
data_offer_offer (void                 *data,
                  struct wl_data_offer *wl_data_offer,
                  const char           *type)
{
  fprintf(stderr, "Offered MIME type = %s\n",type);
}

static void data_offer_dump()
{
}

static const struct wl_data_offer_listener data_offer_listener = {
  data_offer_offer,
  data_offer_dump,
  data_offer_dump
};

static void
data_device_data_offer (void                  *data,
                        struct wl_data_device *data_device,
                        struct wl_data_offer  *offer)
{
  wl_data_offer_add_listener (offer, &data_offer_listener, NULL);
}

static void data_device_dump(void)
{
}

static const struct wl_data_device_listener data_device_listener = {
  data_device_data_offer,
  data_device_dump,
  data_device_dump,
  data_device_dump,
  data_device_dump,
  data_device_selection
};


static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                       uint32_t format, int fd, uint32_t size)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface,
                      struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface)
{
  // When app looses focus the wl_data_offer is no longer valid
  // and needs to be released.
  if(global_offer) {
    wl_data_offer_destroy(global_offer);
    global_offer = nullptr;
  }
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                    uint32_t serial, uint32_t time, uint32_t key,
                    uint32_t state)
{
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                          uint32_t serial, uint32_t mods_depressed,
                          uint32_t mods_latched, uint32_t mods_locked,
                          uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
};


static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
                         enum wl_seat_capability caps)
{
    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
        keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
        wl_keyboard_destroy(keyboard);
        keyboard = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
      seat_handle_capabilities,
};

static void
gdk_registry_handle_global (void               *data,
                            struct wl_registry *registry,
                            uint32_t            id,
                            const char         *interface,
                            uint32_t            version)
{
  if (strcmp (interface, "wl_data_device_manager") == 0)
  {
      data_device_manager_version = MIN (version, 3);
      data_device_manager = (wl_data_device_manager *)
      wl_registry_bind (registry, id, &wl_data_device_manager_interface,
                        data_device_manager_version);
  } else if (strcmp(interface, "wl_seat") == 0) {
      seat = (wl_seat*)wl_registry_bind(registry, id, &wl_seat_interface, 1);
      wl_seat_add_listener(seat, &seat_listener, NULL);
  }
}


static void
gdk_registry_handle_global_remove (void               *data,
                                   struct wl_registry *registry,
                                   uint32_t            id)
{
}

static const struct wl_registry_listener registry_listener = {
    gdk_registry_handle_global,
    gdk_registry_handle_global_remove
};

int data_paste(void)
{
  if(!global_offer)
    return false;

  int pipe_fd[2] = {0, 0};
  if (pipe(pipe_fd) == -1)
      return false;

  //MIME must be from the ones returned by data_offer_offer()
  //wl_data_offer_receive(global_offer, "text/plain", pipe_fd[1]);
  //wl_data_offer_receive(global_offer, "UTF8_STRING", pipe_fd[1]);
  wl_data_offer_receive(global_offer, "TEXT", pipe_fd[1]);
  close(pipe_fd[1]);
  wl_display_flush(display);

  int len;
  char buffer[4096] = "";

  struct pollfd fds;
  fds.fd = pipe_fd[0];
  fds.events = POLLIN;

  // Choose some reasonable timeout here
  int ret = poll(&fds, 1, 200);
  if (ret == -1 || ret == 0)
    return false;

  len = read(pipe_fd[0], buffer, sizeof(buffer));
  if (len && len != -1) {
    fprintf(stderr, "Clipboard data: '%s' len = %d\n", buffer, len);
    return true;
  }
  else {
    perror("data_paste():");
  }

  close(pipe_fd[0]);
  pipe_fd[0] = 0;

  return false;
}

gint test_callback(gpointer dd)
{
  data_paste();
  return TRUE;
}

int main( int   argc,
          char *argv[] )
{
    GtkWidget *window;

    gtk_init (&argc, &argv);

    /* create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    g_signal_connect (window, "destroy",
          G_CALLBACK (destroy), NULL);

    gtk_widget_show_all (GTK_WIDGET(window));

    display = gdk_wayland_display_get_wl_display(gdk_display_get_default());
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (data_device_manager == nullptr)
      return(0);

    data_device = wl_data_device_manager_get_data_device (data_device_manager, seat);
    wl_data_device_add_listener(data_device, &data_device_listener, seat);
    wl_display_roundtrip(display);

    g_timeout_add (1000, test_callback, 0);

    gtk_main ();

    return 0;
}
