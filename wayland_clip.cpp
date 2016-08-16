// build by gcc -o wayland_clip -g -O0 -fpermissive wayland_clip.cpp `pkg-config --libs --cflags gtk+-wayland-3.0` -lwayland-client

#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <wayland-client.h>

struct wl_display *display;
struct wl_registry *registry;
int data_device_manager_version;
struct wl_data_device_manager *data_device_manager;
struct wl_event_queue *wl_queue;
struct wl_data_device *data_device;
struct wl_seat *seat;
struct wl_data_offer *global_offer = NULL;

static void destroy( GtkWidget *widget,
                     gpointer   data )
{
    gtk_main_quit ();
}

static void
data_device_leave (void                  *data,
                   struct wl_data_device *data_device)
{
}

static void
data_device_motion (void                  *data,
                    struct wl_data_device *data_device,
                    uint32_t               time,
                    wl_fixed_t             x,
                    wl_fixed_t             y)
{
}

static void
data_device_drop (void                  *data,
                  struct wl_data_device *data_device)
{
}


static void
data_device_enter (void                  *data,
                   struct wl_data_device *data_device,
                   uint32_t               serial,
                   struct wl_surface     *surface,
                   wl_fixed_t             x,
                   wl_fixed_t             y,
                   struct wl_data_offer  *offer)
{
}

static void
data_device_selection (void                  *data,
                       struct wl_data_device *wl_data_device,
                       struct wl_data_offer  *offer)
{
  fprintf (stderr, "2. ****************** data_device_selection, data device %p, offer %p\n",
           data_device, offer);
  global_offer = offer;
}
  
int pipe_fd[2] = {0,0};

static void
data_device_read_data (void)
{
  if (!pipe_fd[0]) {
    if (pipe2(pipe_fd, O_CLOEXEC|O_NONBLOCK) == -1)
      return;

    wl_data_offer_receive (global_offer, "TEXT", pipe_fd[1]);
    wl_display_roundtrip(display);
    close(pipe_fd[1]);
  }

  int len;
  char buffer[4096] = "";

  len = read(pipe_fd[0], buffer, sizeof(buffer));
  if (len && len != -1) {
    if (data_device_manager_version >= WL_DATA_OFFER_FINISH_SINCE_VERSION)
      wl_data_offer_finish(global_offer);
    close(pipe_fd[0]);
    pipe_fd[0] = pipe_fd[1] = 0;
    global_offer = NULL;
  }

  if(len > 0)
    fprintf(stderr, "Data: '%s' len = %d\n", buffer, len);
}

static void
data_offer_offer (void                 *data,
                  struct wl_data_offer *wl_data_offer,
                  const char           *type)
{
  fprintf(stderr, "******************* data_offer_offer, type = %s\n",type);
}

static void
data_offer_source_actions (void                 *data,
                           struct wl_data_offer *wl_data_offer,
                           uint32_t              source_actions)
{
}

static void
data_offer_action (void                 *data,
                   struct wl_data_offer *wl_data_offer,
                   uint32_t              action)
{
}

static const struct wl_data_offer_listener data_offer_listener = {
  data_offer_offer,
  data_offer_source_actions,
  data_offer_action
};

static void
data_device_data_offer (void                  *data,
                        struct wl_data_device *data_device,
                        struct wl_data_offer  *offer)
{
  fprintf (stderr, "1. ****************** data device data offer, data device %p, offer %p\n",
           data_device, offer);
  wl_data_offer_add_listener (offer, &data_offer_listener, NULL);
}

static const struct wl_data_device_listener data_device_listener = {
  data_device_data_offer,
  data_device_enter,
  data_device_leave,
  data_device_motion,
  data_device_drop,
  data_device_selection
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


gint test_callback(gpointer dd)
{
 char *data = NULL;

/* GTK Code	
  GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  char *data = gtk_clipboard_wait_for_text(clipboard);
*/
  GdkDisplay *gdkDisplay = gdk_display_get_default();
  display = gdk_wayland_display_get_wl_display(gdkDisplay);
  struct wl_registry *registry = wl_display_get_registry(display);
//  wl_queue = wl_display_create_queue(display);
//  wl_proxy_set_queue((struct wl_proxy *)registry, wl_queue);
  wl_registry_add_listener(registry, &registry_listener, NULL);

//  wl_display_roundtrip_queue(display, wl_queue);
  wl_display_roundtrip(display);

  if (data_device_manager == nullptr)
      return(0);

  GdkSeat *gdkseat = gdk_display_get_default_seat(gdkDisplay);
  GdkDevice *device = gdk_seat_get_pointer(gdkseat);
/*
  struct wl_seat *seat = gdk_wayland_device_get_wl_seat(device);
  wl_seat_add_listener(seat, &seat_listener, display);
*/

  data_device = wl_data_device_manager_get_data_device (data_device_manager, seat);
  wl_data_device_add_listener(data_device, &data_device_listener, seat);

  while (1) {
      while (wl_display_prepare_read(display) != 0)
           wl_display_dispatch_pending(display);
      wl_display_flush(display);

      wl_display_read_events(display);
      if(global_offer) {
        data_device_read_data();
      }
  }

  gtk_main_quit ();
  return FALSE;
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

    g_timeout_add (250, test_callback, 0);

    gtk_main ();

    return 0;
}
