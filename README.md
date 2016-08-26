Minimal Wayland clipboard paste example

Created by Martin Stransky <stransky@redhat.com>

Thanks to: Jonas Adahl -at- redhat.com

           examples from https://jan.newmarch.name/Wayland/Input/

build by gcc -o wayland_clip -g -O0 -fpermissive wayland_clip.cpp `pkg-config --libs --cflags gtk+-wayland-3.0` -lwayland-client

run with WAYLAND_DEBUG=1 to see debug logs

