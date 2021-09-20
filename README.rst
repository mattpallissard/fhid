====
fhid
====


A simple fake human interface device, runs as a user service used to send left and right mouse clicks via cli.  Not intended to be used for more than those brief moments where you have to move your hands off your keyboard and actually click a button.

overview
========

runs as a systemd user service, listens for cli 'commands', sends mouse clicks.  That's about it.

install
=======

1. The Makefile, like the rest of this project, is half-assed at best.  You'll almost certainly have to modify the binary installation path
2. This has some gaping security holes.

.. code::

   make
   make install
   sudo make udev # needed to run as a non-root user
   make start

usage
=====

cli is as follows

.. code::

   click --left
   click --left --release
   click --right
   click --right --release

you could feasibly use this as a bindsym with sway as follows.

.. code::

    bindsym --no-repeat Mod1+q exec ~/bin/click --left
    bindsym --no-repeat --release Mod1+q exec ~/bin/click --left --release


The uhid code was bastardized from the linux kernel sample `samples/uhid/uhid-example.c`
