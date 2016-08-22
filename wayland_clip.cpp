// build by gcc -o wayland_clip -g -O0 -fpermissive wayland_clip.cpp `pkg-config --libs --cflags gtk+-wayland-3.0` -lwayland-client
// run with WAYLAND_DEBUG=1

#include <poll.h>
#include <sys/epoll.h>
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
struct wl_event_queue *queue;
struct wl_data_device *data_device;
struct wl_seat *seat;
struct wl_data_offer *global_offer = NULL;

static void destroy( GtkWidget *widget,
                     gpointer   data )
{
    gtk_main_quit ();
}

int pipe_fd[2] = {0, 0};

static void
data_device_selection (void                  *data,
                       struct wl_data_device *wl_data_device,
                       struct wl_data_offer  *offer)
{
  fprintf (stderr, "2. ****************** Setting offer %p\n", offer);
  if(global_offer) {
    wl_data_offer_destroy(global_offer);
    global_offer = nullptr;
  }

  global_offer = offer;
}
  
static bool
data_device_read_data (void)
{
  int len;
  char buffer[4096] = "";

  fprintf(stderr, "Data read start...\n");
  if(pipe_fd[0]) {
    len = read(pipe_fd[0], buffer, sizeof(buffer));
    if (len && len != -1) {
      close(pipe_fd[0]);
      close(pipe_fd[1]);
      pipe_fd[0] = pipe_fd[1] = 0;
      fprintf(stderr, "Clipboard data: '%s' len = %d\n", buffer, len);
      return true;
    }
  }
  return false;
}


static void
data_offer_offer (void                 *data,
                  struct wl_data_offer *wl_data_offer,
                  const char           *type)
{
  fprintf(stderr, "******************* data_offer_offer, type = %s\n",type);
}

static void data_offer_dump() {};

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
  fprintf (stderr, "1. ****************** data device data offer, data device %p, offer %p\n",
           data_device, offer);
  wl_data_offer_add_listener (offer, &data_offer_listener, NULL);
}

static void data_device_dump(void) {};

static const struct wl_data_device_listener data_device_listener = {
  data_device_data_offer,
  data_device_dump,
  data_device_dump,
  data_device_dump,
  data_device_dump,
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
 fprintf(stderr, "****************** test_callback\n");

 int clip_timeout = 100000;
 gint64 start = g_get_monotonic_time();

 #define MAX_EVENTS 2
 struct epoll_event ev, events[MAX_EVENTS];

 int epollfd = epoll_create1(0);
 if (epollfd == -1)
   return FALSE;

 ev.events = EPOLLIN;
 int display_fd = ev.data.fd = wl_display_get_fd(display);
 if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1)
   return FALSE;

 bool clip_data = false;
 while (!clip_data) {
   if (global_offer && !pipe_fd[0]) {
     if (pipe(pipe_fd) == -1)
       return TRUE;

     wl_data_offer_receive(global_offer, "TEXT", pipe_fd[1]);
     close(pipe_fd[1]);
     ev.data.fd = pipe_fd[0];
     if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1)
  	   return FALSE;
    }

    while (wl_display_prepare_read_queue(display, queue) != 0)
        wl_display_dispatch_queue_pending(display, queue);
    wl_display_flush(display);

    int timeout = (clip_timeout - (g_get_monotonic_time() - start))/1000;
    int nfds = epoll_wait(epollfd, events, MAX_EVENTS, timeout);
    if (nfds == -1 || nfds == 0) {
       wl_display_cancel_read(display);
       fprintf(stderr, "Poll timeout\n");
       break;
    }

    for (int i = 0; i < nfds; i++) {
      if (events[i].data.fd == display_fd) {
        wl_display_read_events(display);
        wl_display_dispatch_queue_pending(display, queue);
      } else {
        clip_data = data_device_read_data();
      }
    }
  }

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
    queue = wl_display_create_queue(display);

    registry = wl_display_get_registry(display);
    wl_proxy_set_queue((struct wl_proxy *)registry, queue);    
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_roundtrip_queue(display, queue);

    if (data_device_manager == nullptr)
      return(0);

    data_device = wl_data_device_manager_get_data_device (data_device_manager, seat);
    wl_proxy_set_queue((struct wl_proxy *)data_device, queue);
    wl_data_device_add_listener(data_device, &data_device_listener, seat);

    wl_display_roundtrip_queue(display, queue);

    g_timeout_add (1000, test_callback, 0);

    gtk_main ();

    return 0;
}
